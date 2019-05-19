#include <esp_log.h>
#include <esp_event.h>
#include "module.h"
#include "telegram_events.h"
#include "cmd_executor.h"
#include "ram_var_stor.h"
#include "tlist.h"
#include "telegram_events.h"
#include "telegram_cclist.h"

static const char *TAG="telegram_notifier";

typedef struct
{
	esp_event_base_t base;
	int32_t id;
	void *data;
	bool is_event;
} event_to_send_t;

static void *teleCtx;
static void *teleNList;
static uint32_t count;

static void cmd_stat_show(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	char tmp[32];
	telegram_event_msg_t *evt = ((telegram_event_msg_t *)info->cmd_data);

	if (info->transport != CMD_SRC_TELEGRAM)
	{
		return;
	}

    snprintf(tmp, 32, "Total messages: %d", count);
	telegram_send_text_message(evt->ctx, evt->chat_id, tmp);
}

static void cmd_telegram_cclist_add(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	char tmp[32];
	telegram_int_t *chat_id_mem;
	telegram_cclist_search_helper_t hlp;
	telegram_event_msg_t *evt = ((telegram_event_msg_t *)info->cmd_data);

	if (info->transport != CMD_SRC_TELEGRAM)
	{
		return;
	}

    hlp.id = evt->chat_id;
    hlp.present = false;
    if (teleNList != NULL)
    {
	    telegram_cclist_search(teleNList, &hlp);
	    if (hlp.present)
	    {
	    	snprintf(tmp, 32, "FAIL, Present: %lf", evt->chat_id);
	    	telegram_send_text_message(evt->ctx, evt->chat_id, tmp);	
	    	return;
	    }
	} else
	{
		telegram_send_text_message(evt->ctx, evt->chat_id, "You are my first subscriber! ^_^");	
	}

    chat_id_mem = calloc(sizeof(telegram_int_t), 1);
    if (chat_id_mem == NULL)
    {
    	ESP_LOGE(TAG, "No mem!");
    	return;
    }

	*chat_id_mem = evt->chat_id;
    teleNList = tlist_add(teleNList, chat_id_mem);
    telegram_cclist_save(teleNList, "TELEGRAM_NOTIFIER_CCLIST", true);
    snprintf(tmp, 32, "OK, Added: %lf", evt->chat_id);
    telegram_send_text_message(evt->ctx, evt->chat_id, tmp);	
}

static void cmd_telegram_cclist_show(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	char *cclist = NULL;
	telegram_event_msg_t *evt = ((telegram_event_msg_t *)info->cmd_data);

	if (info->transport != CMD_SRC_TELEGRAM)
	{
		return;
	}

    cclist = var_get("TELEGRAM_NOTIFIER_CCLIST");
    if (cclist != NULL)
    {
		telegram_send_text_message(evt->ctx, evt->chat_id, cclist);
	} else
	{
		telegram_send_text_message(evt->ctx, evt->chat_id, "Nobody subscribed :(");
	}

	free(cclist);
}

static bool telegram_notifier_send_to_one(void *ctx, void *data, void *tlist_el)
{	
	telegram_send_text_message(teleCtx, *((telegram_int_t *)data), (char *)ctx);
	return false;
}

static void telegram_notifier_send_for_all(char *str_to_send)
{
	if (teleNList == NULL)
	{
		return;
	}

	tlist_for_each(teleNList, telegram_notifier_send_to_one, str_to_send);
}

static void telegram_notifier_free_obj(void *ctx, void *data)
{
	free(data);
}

static void telegram_notifier_free(void)
{
	tlist_free_all(teleNList, telegram_notifier_free_obj, NULL);
}

static void telegram_notifier_send_event(event_to_send_t *event)
{
	char tmp[64];

	snprintf(tmp, 64, "EVT = %d ID = %d", (uint32_t)event->base, (uint32_t)event->id);
	telegram_notifier_send_for_all(tmp);
}

static void all_event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	event_to_send_t event_to_send = {};

	if (event_base == TELEGRAM_BASE)
	{
		return;
	}

	event_to_send.base = event_base;
	event_to_send.id = event_id;
	event_to_send.data = event_data;
	
	telegram_notifier_send_event(&event_to_send);
}

static void telegram_notifier_send_hello(void)
{
	const char *def_str = "Channel open!";
	char *heart_beat = var_get("TELEGRAM_NOTIFIER_HEARTBEAT");

	if (heart_beat != NULL)
	{
		telegram_notifier_send_for_all(heart_beat);
		free(heart_beat);
	} else
	{
		telegram_notifier_send_for_all((char *)def_str);
	}
}

static void telegram_event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	ESP_LOGI(TAG, "TELEGRAM_EVENT_ID = %d", event_id);
	switch (event_id)
	{
		case TELEGRAM_STARTED:
			if (teleCtx == NULL)
			{
				teleCtx = *((void **)event_data);
				ESP_LOGI(TAG, "Telegram started! %X ",(uint32_t)teleCtx);
				ESP_ERROR_CHECK(esp_event_handler_register_with(simple_loop_handle, ESP_EVENT_ANY_BASE, 
					ESP_EVENT_ANY_ID, all_event_handler, NULL));

				teleNList = telegram_cclist_load(teleNList, "TELEGRAM_NOTIFIER_CCLIST");
				telegram_notifier_send_hello();
			}
			break;

    	case TELEGRAM_STOP:
    		if (teleCtx != NULL)
			{
				ESP_ERROR_CHECK(esp_event_handler_unregister_with(simple_loop_handle, 
					ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, all_event_handler));

				teleCtx = NULL;
				telegram_notifier_free();
			}
    		break;

    	case TELEGRAM_ERROR:
    	    ESP_LOGI(TAG, "Telegram error!");
    		break;

    	case TELEGRAM_MESSAGE:
    		count++;
    		ESP_LOGI(TAG, "Telegram msg count: %d", count);
    		break;

    	default:
    		break;
	}
}

static void telegram_notifier_init(void)
{
	ESP_LOGI(TAG, "Module started...");
    ESP_ERROR_CHECK(esp_event_handler_register_with(simple_loop_handle, TELEGRAM_BASE, 
    	ESP_EVENT_ANY_ID, telegram_event_handler, NULL));
}

module_init(telegram_notifier_init);
cmd_register_static({
	{
		.name = "cclista",
		.cmd_cb = cmd_telegram_cclist_add,
	},
	{
		.name = "cclists",
		.cmd_cb = cmd_telegram_cclist_show,
	},
	{
		.name = "telestat",
		.cmd_cb = cmd_stat_show,
	},
});
