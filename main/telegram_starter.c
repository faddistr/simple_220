#include <string.h>
#include <esp_log.h>
#include "telegram.h"
#include "ram_var_stor.h"
#include "cmd_executor.h"
#include "module.h"

static const char *TAG="TELEGRAM_STARTER";
static void *teleCtx;

static void telegram_new_message(void *teleCtx, telegram_update_t *info)
{
    char *end = NULL;
    cmd_additional_info_t *cmd_info = NULL;
    telegram_chat_message_t *msg = NULL;
    telegram_int_t chat_id = -1;
    bool res = false;

    if (info == NULL)
    {
        return;
    }

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

static void telegram_starter_init(void)
{
	char *telegram_token;
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

	telegram_token = var_get("TELEGRAM_TOKEN");
	if (telegram_token == NULL)
	{
		ESP_LOGE(TAG, "Failed to start telegram: Bad config!");
	}

	teleCtx = telegram_init(telegram_token, telegram_new_message);
	free(telegram_token);

	if (teleCtx == NULL)
	{
		ESP_LOGE(TAG, "Failed to start telegram...");
	}
}

module_init(telegram_starter_init);