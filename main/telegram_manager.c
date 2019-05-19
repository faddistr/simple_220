#include <string.h>
#include <esp_log.h>
#include <esp_event.h>
#include "telegram.h"
#include "ram_var_stor.h"
#include "cmd_executor.h"
#include "telegram_events.h"
#include "module.h"

static const char *TAG="TELEGRAM_MANAGER";
static void *teleCtx;

ESP_EVENT_DEFINE_BASE(TELEGRAM_BASE);

static void telegram_new_message(void *teleCtx, telegram_update_t *info)
{
    char *end = NULL;
    cmd_additional_info_t *cmd_info = NULL;
    telegram_chat_message_t *msg = NULL;
    telegram_int_t chat_id = -1;
    bool res = false;
    telegram_event_msg_t event_msg = {.ctx = teleCtx, .info = info};

    if (info == NULL)
    {
        return;
    }

    ESP_ERROR_CHECK(esp_event_post(TELEGRAM_BASE, 
        TELEGRAM_MESSAGE, &event_msg, sizeof(telegram_event_msg_t), portMAX_DELAY));

    msg = telegram_get_message(info);

    if ((msg == NULL) || (msg->text == NULL))
    {
        return;
    }

    chat_id = telegram_get_chat_id(msg);
    if (chat_id == -1)
    {
        return;
    }

    cmd_info = calloc(1, sizeof(cmd_additional_info_t));
    end = strchr(msg->text, ' ');

    ESP_LOGI(TAG, "Telegram message: %s from: %f", msg->text, chat_id);

    cmd_info->transport = CMD_SRC_TELEGRAM;
    cmd_info->arg = teleCtx;
    cmd_info->cmd_data = info;

    if (end != NULL)
    {
        char *cmd = NULL;

        *end = '\0';
        cmd = strdup(msg->text);
        *end = ' ';

        res = cmd_execute(cmd, cmd_info);
        free(cmd);
    } else
    {

        res = cmd_execute(msg->text, cmd_info);
    }

    if (!res)
    {
        telegram_send_text_message(cmd_info->arg, chat_id, "Command not found!");
    }

    free(cmd_info);
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
                    ESP_ERROR_CHECK(esp_event_post(TELEGRAM_BASE, 
                        TELEGRAM_ERROR, &err, sizeof(err), portMAX_DELAY));
                    return;
                }

                teleCtx = telegram_init(telegram_token, telegram_new_message);
                free(telegram_token);

                if (teleCtx == NULL)
                {
                    err = ESP_ERR_NO_MEM;
                    ESP_LOGE(TAG, "Failed to start telegram: No memory!");
                    ESP_ERROR_CHECK(esp_event_post(TELEGRAM_BASE, 
                        TELEGRAM_ERROR, &err, sizeof(err), portMAX_DELAY));
                    return;
                }
                
                ESP_LOGI(TAG, "Telegram started! %X", (uint32_t)teleCtx);

                ESP_ERROR_CHECK(esp_event_post(TELEGRAM_BASE, 
                        TELEGRAM_STARTED, &teleCtx, sizeof(teleCtx), portMAX_DELAY));

            }
        break;

        case IP_EVENT_STA_LOST_IP:
            ESP_ERROR_CHECK(esp_event_post(TELEGRAM_BASE, 
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