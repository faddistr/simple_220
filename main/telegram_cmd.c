#include <string.h>
#include <esp_log.h>
#include "telegram.h"
#include "ram_var_stor.h"
#include "cmd_executor.h"
#include "telegram_events.h"
#include "module.h"

static const char *TAG="TELEGRAM_CMD";

static void telegram_new_message(telegram_event_msg_t *evt)
{
	char *end;
    cmd_additional_info_t *cmd_info = NULL;
    bool res = false;

    cmd_info = calloc(1, sizeof(cmd_additional_info_t));
    end = strchr(evt->text, ' ');

    ESP_LOGI(TAG, "Telegram message: %s", evt->text);

    cmd_info->transport = CMD_SRC_TELEGRAM;
    cmd_info->arg = evt->ctx;
    cmd_info->cmd_data = evt;

    if (end != NULL)
    {
        char *cmd = NULL;

        *end = '\0';
        cmd = strdup(evt->text);
        *end = ' ';

        res = cmd_execute(cmd, cmd_info);
        free(cmd);
    } else
    {
        res = cmd_execute(evt->text, cmd_info);
    }

    if (!res)
    {
        telegram_send_text_message(cmd_info->arg, evt->chat_id, "Command not found!");
    }

    free(cmd_info);
}

static void telegram_event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	telegram_event_msg_t *evt = (telegram_event_msg_t *)event_data;
	
	if ((event_id != TELEGRAM_MESSAGE) || (event_base != TELEGRAM_BASE))
	{
		ESP_LOGE(TAG, "Wrong params");
		return;
	}

	telegram_new_message(evt);
}

static void telegram_cmd_init(void)
{
	ESP_LOGI(TAG, "Module started...");
    ESP_ERROR_CHECK(esp_event_handler_register_with(simple_loop_handle, TELEGRAM_BASE, TELEGRAM_MESSAGE, telegram_event_handler, NULL));
}

module_init(telegram_cmd_init);