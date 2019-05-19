#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include <esp_event.h>
#include <string.h>
#include "module.h"
#include "telegram_events.h"

static const char *TAG="telegram_ota";
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

module_init(telegram_ota_init);