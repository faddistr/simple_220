#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <esp_log.h>
#include "ram_var_stor.h"
#include "cmd_executor.h"
#include "telegram.h"
#include "telegram_events.h"
#include "config.h"
#include "module.h"

static const char *TAG="C_SYS";

static void cmd_savevar_cb_telegram(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	telegram_event_msg_t *evt = (telegram_event_msg_t *)info->cmd_data;

    config_save_vars();
	telegram_send_text_message(info->arg, evt->chat_id, "OK");
}

static void cmd_loadvar_cb_telegram(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	esp_err_t res;
	char temp[16];
	telegram_event_msg_t *evt = (telegram_event_msg_t *)info->cmd_data;

    res = config_load_vars();
    sprintf(temp, "Result: %d", res);
	telegram_send_text_message(info->arg, evt->chat_id, temp);
}

static void cmd_getvar_cb_telegram(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	telegram_event_msg_t *evt = (telegram_event_msg_t *)info->cmd_data;
    char *var_name = NULL;
    char *var_value = NULL;

    var_name = strchr(evt->text, ' ');
    if (var_name == NULL)
    {
    	telegram_send_text_message(info->arg, evt->chat_id, "Bad params!");
    	return;
    }

    var_name++;
    var_value = var_get(var_name);
    if (var_value == NULL)
    {
    	telegram_send_text_message(info->arg, evt->chat_id, "Variable not found!");
    	return;
    }

    telegram_send_text_message(info->arg, evt->chat_id, var_name);
	telegram_send_text_message(info->arg, evt->chat_id, var_value);

    free(var_value);
}


static void cmd_setvar_cb_telegram(const char *cmd_name, cmd_additional_info_t *info, void *private, bool save_to_flash)
{
	telegram_event_msg_t *evt = (telegram_event_msg_t *)info->cmd_data;
    char *var_name = NULL;
    char *var_value = NULL;

    var_name = strchr(evt->text, ' ');
    if (var_name == NULL)
    {
    	telegram_send_text_message(info->arg, evt->chat_id, "Bad params!");
    	return;
    }

    var_name++;
    var_value = strchr(var_name, ' ');
    *var_value = '\0';
    var_value++;

 	var_add_attr(var_name, var_value, save_to_flash);
	telegram_send_text_message(info->arg, evt->chat_id, "OK");
}	

static void cmd_delvar_cb_telegram(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	telegram_event_msg_t *evt = (telegram_event_msg_t *)info->cmd_data;
    char *var_name = NULL;

    var_name = strchr(evt->text, ' ');
    if (var_name == NULL)
    {
    	telegram_send_text_message(info->arg, evt->chat_id, "Bad params!");
    	return;
    }

    var_name++;
    var_del(var_name);
    telegram_send_text_message(info->arg, evt->chat_id, "OK");
}

static void cmd_setvar_cb(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	ESP_LOGI(TAG, "SRC: %X", info->transport);
	switch (info->transport)
	{
		case CMD_SRC_HTTPB:
			break;

		case CMD_SRC_TELEGRAM:
			cmd_setvar_cb_telegram(cmd_name, info, private, false);
			break;

		default:
			break;
	}
}

static void cmd_setvar_f_cb(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	ESP_LOGI(TAG, "SRC: %X", info->transport);
	switch (info->transport)
	{
		case CMD_SRC_HTTPB:
			break;

		case CMD_SRC_TELEGRAM:
			cmd_setvar_cb_telegram(cmd_name, info, private, true);
			break;

		default:
			break;
	}
}

static void cmd_getvar_cb(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	ESP_LOGI(TAG, "SRC: %X", info->transport);
	switch (info->transport)
	{
		case CMD_SRC_HTTPB:
			break;

		case CMD_SRC_TELEGRAM:
			cmd_getvar_cb_telegram(cmd_name, info, private);
			break;

		default:
			break;
	}
}

static void cmd_loadvar_cb(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	ESP_LOGI(TAG, "SRC: %X", info->transport);
	switch (info->transport)
	{
		case CMD_SRC_HTTPB:
			break;

		case CMD_SRC_TELEGRAM:
			cmd_loadvar_cb_telegram(cmd_name, info, private);
			break;

		default:
			break;
	}
}

static void cmd_savevar_cb(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	ESP_LOGI(TAG, "SRC: %X", info->transport);
	switch (info->transport)
	{
		case CMD_SRC_HTTPB:
			break;

		case CMD_SRC_TELEGRAM:
			cmd_savevar_cb_telegram(cmd_name, info, private);
			break;

		default:
			break;
	}
}


static void cmd_delvar_cb(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	ESP_LOGI(TAG, "SRC: %X", info->transport);
	switch (info->transport)
	{
		case CMD_SRC_HTTPB:
			break;

		case CMD_SRC_TELEGRAM:
			cmd_delvar_cb_telegram(cmd_name, info, private);
			break;

		default:
			break;
	}
}

cmd_register_static({
	{
		.name = "GetVar",
		.cmd_cb = cmd_getvar_cb,
	},

	{
		.name = "SetVar",
		.cmd_cb = cmd_setvar_cb,
	},

	{
		.name = "SetVarF",
		.cmd_cb = cmd_setvar_f_cb,
	},

	{

		.name = "DelVar",
		.cmd_cb = cmd_delvar_cb,
	},

	{
		.name = "Save",
		.cmd_cb = cmd_savevar_cb,
	},

	{
		.name = "Load",
		.cmd_cb = cmd_loadvar_cb,
	},
});
