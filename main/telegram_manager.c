#include <string.h>
#include <esp_log.h>
#include <esp_event.h>
#include "telegram.h"
#include "ram_var_stor.h"
#include "telegram_events.h"
#include "module.h"

static const char *TAG="TELEGRAM_MANAGER";
static void *teleCtx;

ESP_EVENT_DEFINE_BASE(TELEGRAM_BASE);

static void telegram_new_message(void *teleCtx, telegram_update_t *info)
{
    uint32_t size_required;
    telegram_chat_message_t *msg = telegram_get_message(info);
    telegram_event_msg_t *event_msg;

    if ((msg == NULL) || (msg->text == NULL))
    {
        return;
    }

    size_required = sizeof(telegram_event_msg_t) + strlen(msg->text) + 1;

    event_msg = calloc(size_required, 1);
    if (event_msg == NULL)
    {
        ESP_LOGE(TAG, "No mem! Required size: %d", size_required);
        return;
    }

    strcpy(event_msg->text, msg->text);
    event_msg->ctx = teleCtx;
    event_msg->chat_id = telegram_get_chat_id(msg);
    event_msg->user_id = telegram_get_user_id(msg);

    if (event_msg->chat_id == -1)
    {
        return;
    }

    ESP_ERROR_CHECK(esp_event_post_to(simple_loop_handle, TELEGRAM_BASE, 
        TELEGRAM_MESSAGE, event_msg, sizeof(telegram_event_msg_t) + strlen(msg->text) + 1, portMAX_DELAY));

    free(event_msg);
}

static void telegram_new_file(void *teleCtx, telegram_update_t *info)
{
    telegram_event_file_t *event_file;
    telegram_chat_message_t *msg = telegram_get_message(info);
    telegram_document_t *file;
    uint32_t offset = 0;
    uint32_t data_size;

    if ((msg == NULL) || (msg->file == NULL))
    {
        return;
    }

    file = msg->file;
    data_size = strlen(file->id) + 1 + strlen(file->name) + 1 + strlen(file->mime_type) + 1 
        + (msg->caption?strlen(msg->caption):0) + 1;
    
    event_file = calloc(data_size + sizeof(telegram_event_file_t), 1);
    if (event_file == NULL)
    {
        ESP_LOGE(TAG, "No mem! Required size: %d", data_size + sizeof(telegram_event_file_t));
        return;
    }

    event_file->ctx = teleCtx;
    event_file->data_size = data_size;
    event_file->file_size = file->file_size;
    event_file->chat_id = telegram_get_chat_id(msg);
    event_file->user_id = telegram_get_user_id(msg);

    event_file->id_str_offset = offset;
    memcpy(&event_file->blob[offset], file->id, strlen(file->id) + 1);
    offset += strlen(file->id) + 1;


    if (file->name)
    {
        event_file->name_str_offset = offset;
        memcpy(&event_file->blob[offset], file->name, strlen(file->name) + 1);
        offset += strlen(file->name) + 1;
    }

    if (file->mime_type)
    {
        event_file->mime_type_str_offset = offset;
        memcpy(&event_file->blob[offset], file->mime_type, strlen(file->mime_type) + 1);
        offset += strlen(file->mime_type) + 1;
    }


    if (msg->caption)
    {
        event_file->caption_str_offset = offset;
        memcpy(&event_file->blob[offset], msg->caption, strlen(msg->caption) + 1);
        offset += strlen(msg->caption) + 1;

        ESP_LOGI(TAG, "caption = %s", msg->caption);
    }

    ESP_LOGI(TAG, "File: id: %s name: %s type: %s size: %lf chat_id: %lf", 
        file->id, file->name, file->mime_type, file->file_size, event_file->chat_id);

    ESP_ERROR_CHECK(esp_event_post_to(simple_loop_handle, TELEGRAM_BASE, 
        TELEGRAM_FILE, event_file, data_size + sizeof(telegram_event_file_t), portMAX_DELAY));

    free(event_file);
}


static void telegram_new_obj(void *teleCtx, telegram_update_t *info)
{
    /* telegram_update_t type is not compatible with esp_event_loop */
    if ((info == NULL) || (teleCtx == NULL))
    {
        return;
    }

    telegram_new_message(teleCtx, info);
    telegram_new_file(teleCtx, info);
}

static void ip_event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch(event_id) {
        case IP_EVENT_STA_GOT_IP:
            if (teleCtx == NULL)
            {
                esp_err_t err = ESP_ERR_INVALID_ARG;
                char *telegram_token = var_get("TELEGRAM_TOKEN");

                if (telegram_token == NULL)
                {
                    ESP_LOGE(TAG, "Failed to start telegram: Bad config!");
                    ESP_ERROR_CHECK(esp_event_post_to(simple_loop_handle, TELEGRAM_BASE, 
                        TELEGRAM_ERROR, &err, sizeof(err), portMAX_DELAY));
                    return;
                }

                teleCtx = telegram_init(telegram_token, telegram_new_obj);
                free(telegram_token);

                if (teleCtx == NULL)
                {
                    err = ESP_ERR_NO_MEM;
                    ESP_LOGE(TAG, "Failed to start telegram: No memory!");
                    ESP_ERROR_CHECK(esp_event_post_to(simple_loop_handle, TELEGRAM_BASE, 
                        TELEGRAM_ERROR, &err, sizeof(err), portMAX_DELAY));
                    return;
                }
                
                ESP_LOGI(TAG, "Telegram started! %X", (uint32_t)teleCtx);

                ESP_ERROR_CHECK(esp_event_post_to(simple_loop_handle, TELEGRAM_BASE, 
                        TELEGRAM_STARTED, &teleCtx, sizeof(teleCtx), portMAX_DELAY));

            }
        break;

        case IP_EVENT_STA_LOST_IP:
            ESP_ERROR_CHECK(esp_event_post_to(simple_loop_handle, TELEGRAM_BASE, 
                TELEGRAM_STOP, &teleCtx, sizeof(teleCtx), portMAX_DELAY));
            telegram_stop(teleCtx);
            teleCtx = NULL;
            break;

        default:
            ESP_LOGI(TAG, "IP_EVENT_ID = %d", event_id);
            break;
    }
}

static void telegram_manager_init(void)
{
	char *telegram_disable = var_get("TELEGRAM_DISABLE");

	if (telegram_disable != NULL)
	{
		if (!strcmp(telegram_disable, "1"))
		{
			free(telegram_disable);
			ESP_LOGI(TAG, "Telegram module disabled...");
			return;
		}
	}
	ESP_LOGI(TAG, "Telegram module init...");
	free(telegram_disable);

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, ip_event_handler, NULL));
}

module_init(telegram_manager_init);