#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include "module.h"
#include "ram_var_stor.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "cmd_executor.h"
#include "telegram_events.h"

#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

static const char *TAG = "sd_card";
#define TIMER_INTERVAL_MSEC (3000U)


#define LOG_FILE_NAME_FMT "/sdcard/%d.log"

typedef struct
{
  TimerHandle_t timer;
  bool is_timer;
  TaskHandle_t task;
  SemaphoreHandle_t sem;
  FILE *file;
  char file_name[sizeof(LOG_FILE_NAME_FMT) + 30];
} redirect_log_t;

static redirect_log_t *ctx = NULL;

static void sd_log_stat_cb(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
    telegram_event_msg_t *evt = ((telegram_event_msg_t *)info->cmd_data);
    telegram_send_text(evt->ctx, evt->chat_id, NULL, "Current log: %s", (ctx->file == NULL)?"FAILED":ctx->file_name);
}

static uint32_t sd_file_send_cb(telegram_data_event_t evt, void *teleCtx_ptr, void *ictx, void *evt_data)
{
    telegram_write_data_evt_t *write_data = (telegram_write_data_evt_t *)evt_data;

    switch (evt)
    {
        case TELEGRAM_READ_DATA:   
            //ESP_LOGI(TAG, "TELEGRAM_READ_DATA");
            return fread(write_data->buf, sizeof(uint8_t), write_data->pice_size, (FILE *)ictx);

        case TELEGRAM_RESPONSE:
            ESP_LOGI(TAG, "TELEGRAM_RESPONSE");
            break; 

        case TELEGRAM_ERR:
            ESP_LOGE(TAG, "TELEGRAM_ERR");
            break;

        case TELEGRAM_END:
            fclose((FILE *)ictx);
            break;

        default:
            break;
    }

    return 0;
}

static void sd_file_get_cb(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
  struct stat buffer; 
  FILE *file = NULL;
  telegram_event_msg_t *evt = ((telegram_event_msg_t *)info->cmd_data);
  char *file_name = strchr(evt->text,  ' ');


  if (file_name == NULL)
  {
      telegram_send_text_message(evt->ctx, evt->chat_id, "Full file path should be provided");
      return;
  }

  file_name++;
  if (stat(file_name, &buffer) != 0)
  {
      telegram_send_text_message(evt->ctx, evt->chat_id, "File not found!");
      return;
  }

  file = fopen(file_name, "r");
  if (file == NULL)
  {
      telegram_send_text_message(evt->ctx, evt->chat_id, "File open error");
      return;
  }

  telegram_send_file(evt->ctx, evt->chat_id, strrchr(file_name, '/') + 1, strrchr(file_name, '/') + 1, buffer.st_size, file, 
    sd_file_send_cb);
}

static int _log_vprintf(const char *fmt, va_list args) 
{
  int ret;
  if (!ctx)
  {
    return 0;
  }

  while (!xSemaphoreTake(ctx->sem, portMAX_DELAY))
  {
    ESP_LOGW(TAG, "Mutex wait warn!");
  }  

  ret = vprintf(fmt, args);
  if (ctx->file)
  {
    ret = vfprintf(ctx->file, fmt, args);
  }
  xSemaphoreGive(ctx->sem);
  return ret;
}

static void redirect_log_timer(TimerHandle_t pxTimer)
{
  redirect_log_t *ctx = (redirect_log_t *)pvTimerGetTimerID(pxTimer);
  ctx->is_timer = true;
}

static void redirect_log_task(void * param)
{

  ESP_LOGI(TAG, "\n***** REDIRECTING LOG TO %s %X******\n", ctx->file_name, (uint32_t)ctx->file);
  esp_log_set_vprintf(&_log_vprintf);
  ESP_LOGI(TAG, "%s\n", ctx->file_name);

  while(1)
  {

    if (ctx->is_timer)
    {
        while (!xSemaphoreTake(ctx->sem, portMAX_DELAY))
        {
          ESP_LOGW(TAG, "Mutex wait warn!");
        }  
        fflush(ctx->file);
        fclose(ctx->file);
        ctx->file = fopen(ctx->file_name, "a"); 
        if (ctx->file == NULL)
        {
          ESP_LOGE(TAG, "Failed to open file for log!");
        } else
        {
          ctx->is_timer = false;
        }
        xSemaphoreGive(ctx->sem); 
    }

    vTaskDelay(TIMER_INTERVAL_MSEC / 2);
  }

}

static void redirect_log_get_file_name(redirect_log_t *ctx)
{
  uint32_t i = 0;
  struct stat buffer;   
  do
  {
    sprintf(ctx->file_name, LOG_FILE_NAME_FMT, i);
    ESP_LOGI(TAG, "Trying %s....", ctx->file_name);
    if (stat (ctx->file_name, &buffer) == 0)
    {
      ESP_LOGI(TAG, "File exists");
    } else
    {
      ESP_LOGI(TAG, "Found!");
      return;
    }
    i++;
  } while (1);
}

static void redirect_log(void)
{
  char *redirDisable = var_get("LOG_REDIRECT_DISABLE");

  if (redirDisable != NULL)
  {
    bool disable = (*redirDisable == '1');
    free(redirDisable);

    if (disable)
    {
      ESP_LOGI(TAG, "Log redirect disable!\n");
      return;
    }
  }

  ctx = malloc(sizeof(redirect_log_t));
  if (ctx == NULL)
  {
    ESP_LOGE(TAG, "No mem to redirect log (1)");
    return;
  }

  memset(ctx, 0x00, sizeof(redirect_log_t));
  redirect_log_get_file_name(ctx);

  ctx->file = fopen(ctx->file_name, "w"); 
  if (ctx->file == NULL)
  {
    ESP_LOGE(TAG, "Failed to open file for log!");
    free(ctx);
    return;
  } else
  {
    ESP_LOGI(TAG, "Redirected log file open: %X", (uint32_t)ctx->file);
  }

  ctx->timer = xTimerCreate("LogSaveTimer", TIMER_INTERVAL_MSEC / portTICK_RATE_MS,
          pdTRUE, ctx, redirect_log_timer);

  if (ctx->timer == NULL)
  {
    free(ctx);
    ESP_LOGE(TAG, "No mem to redirect log (2)");
    return;
  }

  vSemaphoreCreateBinary(ctx->sem);
  xTimerStart(ctx->timer, 0);

  xTaskCreatePinnedToCore(&redirect_log_task, "LogSave_task", 2048,  ctx, 1, &ctx->task, 1);  
}


static void init_sdcard(void)
{
  static const cmd_command_descr_t sd_cmd[] = 
  {
      {
          .name = "sd_log_stat",
          .cmd_cb = sd_log_stat_cb,
      },
      {
          .name = "sd_file_get",
          .cmd_cb = sd_file_get_cb,
      },
  };
  esp_err_t ret = ESP_FAIL;
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
  };
  sdmmc_card_t *card;

  ESP_LOGI(TAG, "Mounting SD card...");
  ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);



  if (ret == ESP_OK)
  {
    char temp[32];


    ESP_LOGI(TAG, "SD card mount successfully!");
    var_add("HAS_SD", "1");
    sprintf(temp, "%d", card->csd.capacity);
    var_add("SD_SIZE", temp);
    redirect_log();
    if (!cmd_register_many((cmd_command_descr_t *)sd_cmd, sizeof(sd_cmd) / sizeof(sd_cmd[0])))
    {
        ESP_LOGE(TAG, "Failed to add commands");
    }
  }
  else
  {
    ESP_LOGE(TAG, "Failed to mount SD card VFAT filesystem. Error: %s", esp_err_to_name(ret));
  }
}

module_init(init_sdcard);