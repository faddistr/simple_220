#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <esp_log.h>
#include "ram_var_stor.h"
#include "cmd_executor.h"
#include "telegram.h"
#include "config.h"

static const char *TAG="C_SYS";

static void cmd_getvar_cb(const char *cmd_name, cmd_additional_info_t *info, void *private);
static void cmd_loadvar_cb(const char *cmd_name, cmd_additional_info_t *info, void *private);
static void cmd_savevar_cb(const char *cmd_name, cmd_additional_info_t *info, void *private);
static void cmd_setvar_cb(const char *cmd_name, cmd_additional_info_t *info, void *private);
static void cmd_setvar_f_cb(const char *cmd_name, cmd_additional_info_t *info, void *private);
static void cmd_delvar_cb(const char *cmd_name, cmd_additional_info_t *info, void *private);

static cmd_command_descr_t cmd_sys_descr[] =
{
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
};

static void cmd_savevar_cb_telegram(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	telegram_chat_message_t *msg = telegram_get_message((telegram_update_t *)info->cmd_data);
    telegram_int_t chat_id = telegram_get_chat_id(msg);

    if ((msg == NULL) || (chat_id == -1))
    {
    	ESP_LOGW(TAG, "Bad params!");
    	return;
    }

    config_save_vars();
	telegram_send_text_message(info->arg, chat_id, "OK");
}

static void cmd_loadvar_cb_telegram(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	esp_err_t res;
	char temp[16];
	telegram_chat_message_t *msg = telegram_get_message((telegram_update_t *)info->cmd_data);
    telegram_int_t chat_id = telegram_get_chat_id(msg);

    if ((msg == NULL) || (chat_id == -1))
    {
    	ESP_LOGW(TAG, "Bad params!");
    	return;
    }

    res = config_load_vars();
    sprintf(temp, "Result: %d", res);
	telegram_send_text_message(info->arg, chat_id, temp);
}

static void cmd_getvar_cb_telegram(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	telegram_chat_message_t *msg = telegram_get_message((telegram_update_t *)info->cmd_data);
    telegram_int_t chat_id = telegram_get_chat_id(msg);
    char *var_name = NULL;
    char *var_value = NULL;

    if ((msg == NULL) || (chat_id == -1))
    {
    	ESP_LOGW(TAG, "Bad params!");
    	return;
    }

    var_name = strchr(msg->text, ' ');
    if (var_name == NULL)
    {
    	telegram_send_text_message(info->arg, chat_id, "Bad params!");
    	return;
    }

    var_name++;
    var_value = var_get(var_name);
    if (var_value == NULL)
    {
    	telegram_send_text_message(info->arg, chat_id, "Variable not found!");
    	return;
    }

    telegram_send_text_message(info->arg, chat_id, var_name);
	telegram_send_text_message(info->arg, chat_id, var_value);

    free(var_value);
}


static void cmd_setvar_cb_telegram(const char *cmd_name, cmd_additional_info_t *info, void *private, bool save_to_flash)
{
	telegram_chat_message_t *msg = telegram_get_message((telegram_update_t *)info->cmd_data);
    telegram_int_t chat_id = telegram_get_chat_id(msg);
    char *var_name = NULL;
    char *var_value = NULL;

    if ((msg == NULL) || (chat_id == -1))
    {
    	ESP_LOGW(TAG, "Bad params!");
    	return;
    }

    var_name = strchr(msg->text, ' ');
    if (var_name == NULL)
    {
    	telegram_send_text_message(info->arg, chat_id, "Bad params!");
    	return;
    }

    var_name++;
    var_value = strchr(var_name, ' ');
    *var_value = '\0';
    var_value++;

 	var_add_attr(var_name, var_value, save_to_flash);
	telegram_send_text_message(info->arg, chat_id, "OK");
}	

static void cmd_delvar_cb_telegram(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	telegram_chat_message_t *msg = telegram_get_message((telegram_update_t *)info->cmd_data);
    telegram_int_t chat_id = telegram_get_chat_id(msg);
    char *var_name = NULL;

    if ((msg == NULL) || (chat_id == -1))
    {
    	ESP_LOGW(TAG, "Bad params!");
    	return;
    }

    var_name = strchr(msg->text, ' ');
    if (var_name == NULL)
    {
    	telegram_send_text_message(info->arg, chat_id, "Bad params!");
    	return;
    }

    var_name++;
    var_del(var_name);
    telegram_send_text_message(info->arg, chat_id, "OK");
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

bool cmd_sys_register(void)
{
	bool res = true;
	size_t count = sizeof(cmd_sys_descr) / sizeof(cmd_command_descr_t);

	for (uint32_t i = 0; i < count; i++)
	{
		res = cmd_register(&cmd_sys_descr[i]);
		if (!res)
		{
			break;
		}
	}

	return res;
}