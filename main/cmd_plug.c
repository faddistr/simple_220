#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <esp_log.h>
#include "plug.h"
#include "cmd_executor.h"
#include "telegram_events.h"
#include "httpd_back.h"
#include "module.h"

static void cmd_plug_cb(const char *cmd_name, cmd_additional_info_t *info, void *private);

cmd_register_static({
{
	.name = "plug",
	.cmd_cb = cmd_plug_cb,
} 
});

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
	telegram_event_msg_t *evt = ((telegram_event_msg_t *)info->cmd_data);

	if (cmd_plug(evt->text))
	{
		telegram_send_text_message(evt->ctx, evt->chat_id, "OK");
	} else
	{
		telegram_send_text_message(evt->ctx, evt->chat_id, "FAIL");
	}
}

static void cmd_plug_cb(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
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
