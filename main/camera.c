#include <string.h>
#include <esp_log.h>
#include <esp_system.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_camera.h"
#include "module.h"
#include "telegram_events.h"
#include "ram_var_stor.h"
#include "cmd_executor.h"
#include "jpg_streamer.h"

static const char *TAG = "camera";


static const camera_config_t camera_config = {
    .pin_pwdn = -1,
    .pin_reset = CONFIG_RESET,
    .pin_xclk = CONFIG_XCLK,
    .pin_sscb_sda = CONFIG_SDA,
    .pin_sscb_scl = CONFIG_SCL,

    .pin_d7 = CONFIG_D7,
    .pin_d6 = CONFIG_D6,
    .pin_d5 = CONFIG_D5,
    .pin_d4 = CONFIG_D4,
    .pin_d3 = CONFIG_D3,
    .pin_d2 = CONFIG_D2,
    .pin_d1 = CONFIG_D1,
    .pin_d0 = CONFIG_D0,
    .pin_vsync = CONFIG_VSYNC,
    .pin_href = CONFIG_HREF,
    .pin_pclk = CONFIG_PCLK,

    //XCLK 20MHz or 10MHz
    .xclk_freq_hz = CONFIG_XCLK_FREQ,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_SVGA,   //QQVGA-UXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 12, //0-63 lower number means higher quality
    .fb_count = 1       //if more than one, i2s runs in continuous mode. Use only with JPEG
};

static esp_err_t init_camera(void)
{
  //initialize the camera
  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Camera Init Failed");
    return err;
  }

  return ESP_OK;
}

static uint32_t load_photo_cb(telegram_data_event_t evt, void *teleCtx_ptr, void *ctx, void *evt_data)
{    
    switch (evt)
    {
        case TELEGRAM_READ_DATA:   
            ESP_LOGI(TAG, "TELEGRAM_READ_DATA");
            {
              camera_fb_t *pic = (camera_fb_t *)ctx;
              telegram_write_data_evt_t *write_data = (telegram_write_data_evt_t *)evt_data;
              uint32_t part_size = write_data->pice_size;

              if (write_data->offset >= pic->len)
              {
                return 0;
              }

              if (part_size > (pic->len - write_data->offset))
              {
                  part_size = (pic->len - write_data->offset);
              }

              memcpy(write_data->buf, &pic->buf[write_data->offset], part_size);

              return part_size;
            }
            break;

        case TELEGRAM_RESPONSE:
            ESP_LOGI(TAG, "TELEGRAM_RESPONSE");
            break; 

        case TELEGRAM_WRITE_DATA:
            ESP_LOGI(TAG, "TELEGRAM_WRITE_DATA");
            break;

        case TELEGRAM_ERR:
            ESP_LOGE(TAG, "TELEGRAM_ERR");
            break;

        case TELEGRAM_END:
            ESP_LOGI(TAG, "TELEGRAM_END");
            break;

        default:
            break;
    }

    return 0;
}

static void cmd_photo_cb(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
    ESP_LOGI(TAG, "Taking picture...");
    telegram_event_msg_t *evt = ((telegram_event_msg_t *)info->cmd_data);
    camera_fb_t *pic = esp_camera_fb_get();
    int64_t timestamp = esp_timer_get_time();

    char *pic_name = malloc(17 + sizeof(int64_t));
    sprintf(pic_name, "pic_%lli.jpg", timestamp);

    telegram_send_file_full(evt->ctx, evt->chat_id, pic_name, pic_name, pic->len, pic, load_photo_cb, TELEGRAM_PHOTO);
    free(pic_name);
    esp_camera_fb_return(pic);
}

static httpd_handle_t streamer;


static void cmd_mjpg_start_cb(const char *cmd_name, cmd_additional_info_t *info, void *private)
{

    telegram_event_msg_t *evt = ((telegram_event_msg_t *)info->cmd_data);

    if (streamer != NULL)
    {
      telegram_send_text_message(evt->ctx, evt->chat_id, "Already started");
      return;
    }


    streamer = mjpg_start_webserver();
    if (streamer == NULL)
    {
      telegram_send_text_message(evt->ctx, evt->chat_id, "Failed to start: no memory");
      return;
    }

    telegram_send_text_message(evt->ctx, evt->chat_id, "Started!");
}

static void cmd_mjpg_stop_cb(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
    telegram_event_msg_t *evt = ((telegram_event_msg_t *)info->cmd_data);

    if (streamer == NULL)
    {
      telegram_send_text_message(evt->ctx, evt->chat_id, "Already stopped");
      return;
    }

    mjpg_stop_webserver(streamer);
    streamer = NULL;

    telegram_send_text_message(evt->ctx, evt->chat_id, "Stopped!");
}

static void camera_init(void)
{
  static const cmd_command_descr_t camera_cmd[] = 
  {
      {
          .name = "photo",
          .cmd_cb = cmd_photo_cb,
      },
      {
          .name = "mjpg_start",
          .cmd_cb = cmd_mjpg_start_cb,
      },
      {
          .name = "mjpg_stop",
          .cmd_cb = cmd_mjpg_stop_cb,
      }
  };
  ESP_LOGI(TAG, "Init...");
  if (init_camera() != ESP_OK)
  {
    ESP_LOGI(TAG, "No camera");
    return;
  }

  ESP_LOGI(TAG, "I have a camera!!!");

  var_add("HAS_CAMERA", "1");
  if (!cmd_register_many((cmd_command_descr_t *)camera_cmd, sizeof(camera_cmd) / sizeof(camera_cmd[0])))
  {
      ESP_LOGE(TAG, "Failed to add commands");
  }
}

static void event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  switch (event_id)
  {
    case MODULE_EVENT_DONE:
      camera_init();
      break;

    case MODULE_EVENT_OTA:
      ESP_LOGI(TAG, "Stopping camera!");
      if (streamer != NULL)
      {
          mjpg_stop_webserver(streamer);
          streamer = NULL;
      }
      esp_camera_deinit();
      break;

    default:
      break;
  }
 
}

static void module_camera_init(void)
{
  ESP_ERROR_CHECK(esp_event_handler_register_with(simple_loop_handle, MODULE_BASE, ESP_EVENT_ANY_ID, event_handler, NULL));
}

module_init(module_camera_init);