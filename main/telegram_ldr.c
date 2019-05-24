#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include <esp_event.h>
#include <string.h>
#include "module.h"
#include "config.h"
#include "cmd_executor.h"
#include "telegram_events.h"

static const char *TAG="telegram_ldr";
static TaskHandle_t teleOTAHnd;

static void *teleCtx;
static telegram_event_file_t *file_evt;
static telegram_document_t file;
static char *file_path;

static void telegram_ota_task(void * param)
{
    ESP_LOGI(TAG, "Start!");

    ESP_LOGI(TAG, "File: id: %s name: %s type: %s size: %lf chat_id: %lf", 
        file.id, file.name, file.mime_type, file.file_size, file_evt->chat_id);

	while(teleCtx != NULL)
	{
		vTaskDelay(20);
	}

	ESP_LOGI(TAG, "Stop");
	teleCtx = NULL;
}

static uint8_t *config_buffer;
static uint32_t counter;
static telegram_int_t chat_id;

static bool telegram_load_config_cb(void *ctx, uint8_t *buf, int size, int total_len)
{
    //TODO context
    if ((size == 0) && (config_buffer) && (counter > 0))
    {
        config_load_vars_mem(config_buffer, counter);
        free(config_buffer);
        config_buffer = NULL;
        telegram_send_text_message(ctx, chat_id, "Config loaded!");
        return true;
    }

    if (size < 0)
    {
        free(config_buffer);
        config_buffer = NULL;
        return false;
    }

    if ((total_len > 0) && (!config_buffer))
    {
        config_buffer = calloc(sizeof(uint8_t), total_len);
        counter = 0;
        if (!config_buffer)
        {
            ESP_LOGE(TAG, "No mem");
            return false;
        }
    }

    if ((size > 0) && (config_buffer))
    {
        memcpy(&config_buffer[counter], buf, size);
        counter += size;
    }

    return true;
}

static void telegram_event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	switch (event_id)
	{
    	case TELEGRAM_STOP:
    		if (teleCtx != NULL)
    		{
    			ESP_LOGI(TAG, "Telegram stopped!");
    			free(file_evt);
				vTaskDelete(teleOTAHnd);
			}
    		break;

    	case TELEGRAM_FILE:
    		if (teleCtx == NULL)
    		{
    			telegram_event_file_t *evt = (telegram_event_file_t *)event_data;

    			teleCtx = evt->ctx;
                file_evt = calloc(sizeof(telegram_event_file_t) + evt->data_size, 1);
                if (file_evt == NULL)
                {
                    ESP_LOGE(TAG, "No mem!");
                    return;
                }

    			memcpy(file_evt, evt, sizeof(telegram_event_file_t) + evt->data_size);

                file.id = &file_evt->blob[file_evt->id_str_offset];
                file.mime_type = &file_evt->blob[file_evt->mime_type_str_offset];
                file.name = &file_evt->blob[file_evt->name_str_offset];
                file.file_size = file_evt->file_size;

                ESP_LOGI(TAG, "File: id: %s name: %s type: %s size: %lf chat_id: %lf", 
                    file.id, file.name, file.mime_type, file.file_size, file_evt->chat_id);

                file_path = telegram_get_file_path(teleCtx, file.id);

                telegram_send_text_message(teleCtx, file_evt->chat_id, file.id);
                telegram_send_text_message(teleCtx, file_evt->chat_id, file_path);
    		//	xTaskCreate(&telegram_ota_task, "telegram_ota_task", 8192, NULL, 6, &teleOTAHnd);

                if (!strcmp(file.name, "config.txt"))
                {
                    chat_id = file_evt->chat_id;
                    telegram_get_file(teleCtx, file.id, teleCtx, telegram_load_config_cb);
                }
                
                free(file_evt);
                free(file_path);
                teleCtx = NULL;
    		}
    		break;

    	default:
    		break;
	}
}

static void telegram_ota_init(void)
{
	ESP_LOGI(TAG, "Module started...");
    ESP_ERROR_CHECK(esp_event_handler_register_with(simple_loop_handle, TELEGRAM_BASE, ESP_EVENT_ANY_ID, telegram_event_handler, NULL));
}

typedef struct
{
    uint8_t *send_ptr;
    uint32_t total_size;
    uint32_t counter;
} getFile_helper_t;

static uint32_t send_file_cb(void *ctx, uint8_t *buf, uint32_t max_size)
{
    getFile_helper_t *hnd = (getFile_helper_t *)ctx;
    uint32_t part_size = max_size;

    if (hnd->total_size == 0)
    {
        ESP_LOGW(TAG, "Data sent!");
        return 0;
    }

    if (part_size > hnd->total_size)
    {
        part_size = hnd->total_size;
    }

    memcpy(buf, &hnd->send_ptr[hnd->counter], part_size);

    hnd->total_size -= part_size;
    hnd->counter += part_size;
    if (hnd->total_size == 0)
    {
        free(hnd->send_ptr);
        free(hnd);
    }

    return part_size;
}

static void cmd_configfile(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
    telegram_event_msg_t *evt = (telegram_event_msg_t *)info->cmd_data;
    getFile_helper_t *ctx = calloc(sizeof(getFile_helper_t), 1);

    if (ctx == NULL)
    {
        ESP_LOGE(TAG, "No mem!");
        return;
    }

    ctx->send_ptr = config_get_vars_mem(&ctx->total_size);

    if ((ctx->send_ptr == NULL) || (ctx->total_size == 0))
    {
        ESP_LOGE(TAG, "Failed to get config");
        return;
    }

    telegram_send_file(info->arg, evt->chat_id, "config", "config.txt", ctx->total_size, ctx, send_file_cb);
}

static uint32_t send_testfile_cb(void *ctx, uint8_t *buf, uint32_t max_size)
{
    ESP_LOGE(TAG, "MSIZE:%d", max_size);
    memset(buf, 'B', max_size);
    sprintf((char *)buf, "Hello world\n");
    buf[strlen((char *)buf)] = (uint8_t)'A';

    return max_size;
}

static void cmd_testfile(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
    telegram_event_msg_t *evt = (telegram_event_msg_t *)info->cmd_data;
    telegram_send_file(info->arg, evt->chat_id, "test", "test.txt", 256, NULL, send_testfile_cb);
}


module_init(telegram_ota_init);
cmd_register_static({
    {
        .name = "testfile",
        .cmd_cb = cmd_testfile,
    },
    {
        .name = "getconfig",
        .cmd_cb = cmd_configfile,
    },
});