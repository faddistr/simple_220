#include <string.h>
#include <esp_log.h>
#include <esp_event.h>
#include "module.h"
#include "telegram_events.h"
#include "cmd_executor.h"
#include "ram_var_stor.h"

static const char *TAG="telegram_notifier";

static char *cclist;
static void *teleCtx;
static uint32_t count;
static telegram_int_t *cclist_arr;

static void cmd_cclist_add(const char *cmd_name, cmd_additional_info_t *info, void *private);
static void cmd_cclist_show(const char *cmd_name, cmd_additional_info_t *info, void *private);
static void cmd_stat_show(const char *cmd_name, cmd_additional_info_t *info, void *private);


static cmd_command_descr_t cmd_telegram_notifier_descr[] =
{
	{
		.name = "cclista",
		.cmd_cb = cmd_cclist_add,
	},
	{
		.name = "cclists",
		.cmd_cb = cmd_cclist_show,
	},
	{
		.name = "telestat",
		.cmd_cb = cmd_stat_show,
	},
};

static void cmd_stat_show(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	char tmp[32];
	telegram_chat_message_t	*msg;
	telegram_int_t chat_id;

	if (info->transport != CMD_SRC_TELEGRAM)
	{
		return;
	}

 	msg = telegram_get_message((telegram_update_t *)info->cmd_data);
    chat_id = telegram_get_chat_id(msg);

    if ((msg == NULL) || (chat_id == -1))
    {
    	ESP_LOGW(TAG, "Bad params!");
    	return;
    }

    snprintf(tmp, 32, "Total messages: %d", count);
	telegram_send_text_message(info->arg, chat_id, tmp);
}

static void cmd_cclist_add(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	telegram_chat_message_t	*msg;
	telegram_int_t chat_id;

	if (info->transport != CMD_SRC_TELEGRAM)
	{
		return;
	}

 	msg = telegram_get_message((telegram_update_t *)info->cmd_data);
    chat_id = telegram_get_chat_id(msg);

    if ((msg == NULL) || (chat_id == -1))
    {
    	ESP_LOGW(TAG, "Bad params!");
    	return;
    }

	telegram_send_text_message(info->arg, chat_id, "Dummy answer");
}

static void cmd_cclist_show(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	telegram_chat_message_t	*msg;
	telegram_int_t chat_id;

	if (info->transport != CMD_SRC_TELEGRAM)
	{
		return;
	}

 	msg = telegram_get_message((telegram_update_t *)info->cmd_data);
    chat_id = telegram_get_chat_id(msg);

    if ((msg == NULL) || (chat_id == -1))
    {
    	ESP_LOGW(TAG, "Bad params!");
    	return;
    }

    if (cclist != NULL)
    {
		telegram_send_text_message(info->arg, chat_id, cclist);
	} else
	{
		telegram_send_text_message(info->arg, chat_id, "Nobody subscribed :(");
	}
}

static void telegram_notifier_send_to_all(void)
{

}

static void telegram_event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	ESP_LOGI(TAG, "TELEGRAM_EVENT_ID = %d", event_id);
	switch (event_id)
	{
		case TELEGRAM_STARTED:
			ESP_LOGI(TAG, "Telegram started!");
			teleCtx = event_data;
			telegram_notifier_send_to_all();
			break;

    	case TELEGRAM_STOP:
    		ESP_LOGI(TAG, "Telegram stoped!");
    		teleCtx = NULL;
    		break;

    	case TELEGRAM_ERROR:
    	    ESP_LOGI(TAG, "Telegram error!");
    		teleCtx = NULL;
    		break;

    	case TELEGRAM_MESSAGE:
    		count++;
    		ESP_LOGI(TAG, "Telegram msg count: %d", count);
    		break;

    	default:
    		break;
	}
}

static bool cmd_register_notifier(void)
{
	bool res = true;
	size_t count = sizeof(cmd_telegram_notifier_descr) / sizeof(cmd_command_descr_t);

	for (uint32_t i = 0; i < count; i++)
	{
		res = cmd_register(&cmd_telegram_notifier_descr[i]);
		if (!res)
		{
			break;
		}
	}

	return res;
}

static void parse_cclist(void)
{

}

static void telegram_notifier_init(void)
{
	cclist = var_get("TELEGRAM_NOTFIER_CCLIST");

	ESP_LOGI(TAG, "Module started...");
    ESP_ERROR_CHECK(esp_event_handler_register(TELEGRAM_BASE, ESP_EVENT_ANY_ID, telegram_event_handler, NULL));
	
	if (!cmd_register_notifier())
	{
		ESP_LOGE(TAG, "Failed to register cmds");
	} 

	if (cclist != NULL)   
	{
		parse_cclist();	
	}
}

module_init(telegram_notifier_init);