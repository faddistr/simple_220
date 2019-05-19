#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include <esp_event.h>
#include "module.h"
#include "telegram_events.h"
#include "cmd_executor.h"
#include "ram_var_stor.h"
#include "tlist.h"
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
static event_to_send_t event_to_send;
static TaskHandle_t teleNotHnd;


static void cmd_stat_show(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	char tmp[32];
	telegram_chat_message_t	*msg;
	telegram_int_t chat_id;

	if (info->transport != CMD_SRC_TELEGRAM)
	{
		return;
	}

	ESP_LOGI(TAG, "H: %X", (uint32_t)info->arg);

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

static void cmd_telegram_cclist_add(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	char tmp[32];
	telegram_chat_message_t	*msg;
	telegram_int_t chat_id;
	telegram_int_t *chat_id_mem;
	telegram_cclist_search_helper_t hlp;

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

    hlp.id = chat_id;
    hlp.present = false;
    if (teleNList != NULL)
    {
	    telegram_cclist_search(teleNList, &hlp);
	    if (hlp.present)
	    {
	    	snprintf(tmp, 32, "FAIL, Present: %lf", chat_id);
	    	telegram_send_text_message(info->arg, chat_id, tmp);	
	    	return;
	    }
	} else
	{
		telegram_send_text_message(info->arg, chat_id, "You are my first subscriber! ^_^");	
	}

    chat_id_mem = calloc(sizeof(telegram_int_t), 1);
    if (chat_id_mem == NULL)
    {
    	ESP_LOGE(TAG, "No mem!");
    	return;
    }

	*chat_id_mem = chat_id;
    teleNList = tlist_add(teleNList, chat_id_mem);
    telegram_cclist_save(teleNList, "TELEGRAM_NOTIFIER_CCLIST", true);
    snprintf(tmp, 32, "OK, Added: %lf", chat_id);
    telegram_send_text_message(info->arg, chat_id, tmp);	
}

static void cmd_telegram_cclist_show(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	char *cclist = NULL;
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

    cclist = var_get("TELEGRAM_NOTIFIER_CCLIST");
    if (cclist != NULL)
    {
		telegram_send_text_message(info->arg, chat_id, cclist);
	} else
	{
		telegram_send_text_message(info->arg, chat_id, "Nobody subscribed :(");
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

static void all_event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	if (event_base == TELEGRAM_BASE)
	{
		return;
	}

	event_to_send.base = event_base;
	event_to_send.id = event_id;
	event_to_send.data = event_data;
	event_to_send.is_event = true;
}

static void telegram_notifier_send_event(event_to_send_t *event)
{
	char tmp[64];

	snprintf(tmp, 64, "EVT = %d ID = %d", (uint32_t)event->base, (uint32_t)event->id);
	telegram_notifier_send_for_all(tmp);
}

static void telegram_notifier_task(void * param)
{
	const char *def_str = "Channel open!";
	char *heart_beat = var_get("TELEGRAM_NOTIFIER_HEARTBEAT");;

    ESP_LOGI(TAG, "Start!");
	teleNList = telegram_cclist_load(teleNList, "TELEGRAM_NOTIFIER_CCLIST");	
	if (heart_beat != NULL)
	{
		telegram_notifier_send_for_all(heart_beat);
		free(heart_beat);
	} else
	{
		telegram_notifier_send_for_all((char *)def_str);
	}

	ESP_ERROR_CHECK(esp_event_handler_register(ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, all_event_handler, NULL));
	while(teleCtx != NULL)
	{
		if (event_to_send.is_event)
		{
			telegram_notifier_send_event(&event_to_send);
			event_to_send.is_event = false;
		}
		vTaskDelay(1);
	}
}


static void telegram_event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	ESP_LOGI(TAG, "TELEGRAM_EVENT_ID = %d", event_id);
	switch (event_id)
	{
		case TELEGRAM_STARTED:
			teleCtx = *((void **)event_data);
			ESP_LOGI(TAG, "Telegram started! %X ",(uint32_t)teleCtx);
			xTaskCreate(&telegram_notifier_task, "telegram_notifier_task", 8192, NULL, 6, &teleNotHnd);
			break;

    	case TELEGRAM_STOP:
    		ESP_LOGI(TAG, "Telegram stoped!");
    		teleCtx = NULL;
    		telegram_notifier_free();
			vTaskDelay(portTICK_RATE_MS);
			vTaskDelete(teleNotHnd);
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

static void telegram_notifier_init(void)
{
	ESP_LOGI(TAG, "Module started...");
    ESP_ERROR_CHECK(esp_event_handler_register(TELEGRAM_BASE, ESP_EVENT_ANY_ID, telegram_event_handler, NULL));
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
