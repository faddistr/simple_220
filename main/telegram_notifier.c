#include <string.h>
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
    telegram_event_msg_t *evt = ((telegram_event_msg_t *)info->cmd_data);
    telegram_cclist_search_helper_t hlp = {.id = evt->user_id,};
    char *chat_id = (char *)&evt->text[strlen(cmd_name)];

    if (info->transport != CMD_SRC_TELEGRAM)
    {
        return;
    }

    if (*chat_id != '\0')
    {
        sscanf(chat_id + 1, "%lf", &hlp.id);
    }

    hlp.present = false;
    if (teleNList != NULL)
    {
        telegram_cclist_search(teleNList, &hlp);
        if (hlp.present)
        {
            snprintf(tmp, 32, "FAIL, Present: %.0f", hlp.id);
            telegram_send_text_message(evt->ctx, evt->chat_id, tmp);    
            return;
        }
    } 

    chat_id_mem = calloc(sizeof(telegram_int_t), 1);
    if (chat_id_mem == NULL)
    {
        ESP_LOGE(TAG, "No mem!");
        return;
    }

    *chat_id_mem = hlp.id;
    teleNList = tlist_add(teleNList, chat_id_mem);
    telegram_cclist_save(teleNList, "TELEGRAM_NOTIFIER_CCLIST", true);
    snprintf(tmp, 32, "OK, Added: %.0f", hlp.id);
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

static void telegram_notifier_send_event(event_to_send_t *event)
{
	char tmp[64];

	snprintf(tmp, 64, "EVT = %d ID = %d", (uint32_t)event->base, (uint32_t)event->id);
	telegram_notifier_send_for_all(tmp);
}


static void cmd_telegram_cclist_del(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
    char tmp[32];
    telegram_int_t chat_id_mem;
    telegram_event_msg_t *evt = ((telegram_event_msg_t *)info->cmd_data);
    char *chat_id = (char *)&evt->text[strlen(cmd_name)];

    if (info->transport != CMD_SRC_TELEGRAM)
    {
        return;
    }

    if (*chat_id == '\0')
    {
        telegram_send_text_message(evt->ctx, evt->chat_id, "Usage: cclistd $chat_id");    
        return;
    }

    if (teleNList != NULL)
    {
        sscanf(chat_id, "%lf", &chat_id_mem);
        if (!telegram_cclist_del(teleNList, chat_id_mem))
        {
            telegram_send_text_message(evt->ctx, evt->chat_id, "FAIL:Not found!");    
            return;
        }

        snprintf(tmp, 32, "OK, Deleted: %s", chat_id);
        telegram_send_text_message(evt->ctx, evt->chat_id, tmp);    
   		telegram_cclist_save(teleNList, "TELEGRAM_NOTIFIER_CCLIST", true);
        return;
        
    } else
    {
        telegram_send_text_message(evt->ctx, evt->chat_id, "FAIL list is NULL"); 
    }
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
	char *tmp = calloc(sizeof(char), 128);
	char *file_id_current = var_get("TELEGRAM_FW_CURRENT");
	const char *def_str = "Channel open";
	char *heart_beat = var_get("TELEGRAM_NOTIFIER_HEARTBEAT");


	if (heart_beat != NULL)
	{
		sprintf(tmp, "%s %s", heart_beat, file_id_current?file_id_current:"SCRATCH");
		free(heart_beat);
	} else
	{
		sprintf(tmp, "%s %s", def_str, file_id_current?file_id_current:"SCRATCH");
	}

	telegram_notifier_send_for_all(tmp);
	free(tmp);
	free(file_id_current);
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
				telegram_cclist_free(teleNList);
				teleNList = NULL;
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
		.name = "cclistd",
		.cmd_cb = cmd_telegram_cclist_del,
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
