#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <esp_log.h>
#include "plug.h"
#include "cmd_executor.h"
#include "httpd_back.h"
#include "module.h"

static const char *TAG="C_PLUG";

static void cmd_plug_cb(const char *cmd_name, cmd_additional_info_t *info, void *private);

static cmd_command_descr_t cmd_plug_descr =
{
	.name = "plug",
	.cmd_cb = cmd_plug_cb,
};

static bool cmd_plug(void *args)
{
	uint32_t key = 0;
	uint32_t val = 0;
	char *save_ptr = NULL;
	bool is_key = false;
	bool is_val = false;
	char *token = strtok_r((char *)args, " ", &save_ptr);

	if (token == NULL)
	{
		return false;
	}

	do
	{
		if (!strcmp(token, "key"))
		{
			token = strtok_r(NULL, " ", &save_ptr);
			if (token == NULL)
			{
				break;
			}

			is_key = (sscanf(token, "%d", &key) == 1);
		}

		if (!strcmp(token, "value"))
		{
			token = strtok_r(NULL, " ", &save_ptr);
			if (token == NULL)
			{
				break;
			}

			is_val = (sscanf(token, "%d", &val) == 1);
		}

		if (is_key && is_val)
		{
			break;
		}
		token = strtok_r(NULL, " ", &save_ptr);
	} while (token != NULL);

	if (is_key && is_val)
	{
		plug_set_key(key, !!val);
		return true;
	} 
	
	return false;
}

static void cmd_plug_cb_httpb(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	if (cmd_plug(info->cmd_data))
	{
		httpd_send_answ(info->arg, "OK", 2);
	} else
	{
		httpd_send_answ(info->arg, "FAIL", 4);
	}
}

static void cmd_plug_cb_telegram(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	telegram_chat_message_t *msg = telegram_get_message((telegram_update_t *)info->cmd_data);
    telegram_int_t chat_id = telegram_get_chat_id(msg);

    if ((msg == NULL) || (chat_id == -1))
    {
    	ESP_LOGW(TAG, "Bad params!");
    	return;
    }

	if (cmd_plug(msg->text))
	{
		telegram_send_text_message(info->arg, chat_id, "OK");
	} else
	{
		telegram_send_text_message(info->arg, chat_id, "FAIL");
	}
}

static void cmd_plug_cb(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	ESP_LOGI(TAG, "SRC: %X", info->transport);
	switch (info->transport)
	{
		case CMD_SRC_HTTPB:
			cmd_plug_cb_httpb(cmd_name, info, private);
			break;

		case CMD_SRC_TELEGRAM:
			cmd_plug_cb_telegram(cmd_name, info, private);
			break;

		default:
			break;
	}
}

static void cmd_plug_init(void) 
{
	ESP_LOGI(TAG, "Module init...");
	if (!cmd_register(&cmd_plug_descr))
	{
		ESP_LOGE(TAG, "Fail while adding commands!");
	}
}

module_init(cmd_plug_init);