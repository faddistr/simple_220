
#include <string.h>
#include <stdbool.h>
#include <cJSON.h>
#include <esp_log.h>
#include <esp_http_client.h>
#include "freertos/timers.h"
#include "telegram.h"

#define TELEGRAM_MAX_PATH 128U
#define TELEGRAM_MAX_BUFFER 4095U

#define TIMER_INTERVAL_MSEC    (1L * 5L * 1000L)

#define TELEGRAM_INLINE_BTN_FMT "[{\"text\": \"%s\", \"callback_data\": \"%s\"}]"
#define TELEGRAM_INLINE_KBRD_FMT "\"inline_keyboard\": "
#define TELEGRMA_MSG_FMT "{\"chat_id\": \"%f\", \"text\": \"%s\"}"
#define TELEGRMA_MSG_MARKUP_FMT "{\"chat_id\": \"%f\", \"text\": \"%s\", \"reply_markup\": {%s}}"


typedef struct
{
    char *token;	
	char path[TELEGRAM_MAX_PATH + 1];
	int32_t last_update_id;
	bool stop;
	esp_http_client_config_t httpClientCfg;
	TimerHandle_t timer;
	bool is_timer;
	telegram_on_msg_cb_t on_msg_cb;
} telegram_ctx_t;

static const char *TAG="telegram";

static esp_err_t telegram_http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            ESP_LOGI(TAG, "RCVD: %s\r\n", (char *)evt->data);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}

static void telegram_process_channel_post(telegram_ctx_t *teleCtx, cJSON *channel_post)
{
	cJSON *chat = cJSON_GetObjectItem(channel_post, "chat"); 
	cJSON *chat_id = cJSON_GetObjectItem(chat, "id"); 
	cJSON *chat_title = cJSON_GetObjectItem(chat, "title"); 
	cJSON *text = cJSON_GetObjectItem(channel_post, "text");

	if (text != NULL)
	{
		teleCtx->on_msg_cb(teleCtx, chat_id->valuedouble, chat_title->valuestring, text->valuestring);
	}
}

static void telegram_process_private(telegram_ctx_t *teleCtx, cJSON *message)
{
	cJSON *chat = cJSON_GetObjectItem(message, "chat"); 
	cJSON *chat_id = cJSON_GetObjectItem(chat, "id"); 
	cJSON *chat_title = cJSON_GetObjectItem(chat, "first_name"); 
	cJSON *text = cJSON_GetObjectItem(message, "text");

	if (text != NULL)
	{
		teleCtx->on_msg_cb(teleCtx, chat_id->valuedouble, chat_title->valuestring, text->valuestring);
	}
}

static void telegram_parse_message(telegram_ctx_t *teleCtx, cJSON *subitem)
{
	cJSON *channel_post = cJSON_GetObjectItem(subitem, "channel_post");
	if (channel_post != NULL)
	{
		telegram_process_channel_post(teleCtx, channel_post);
		return;
	} 

	channel_post = cJSON_GetObjectItem(subitem, "message");
	if (channel_post != NULL)
	{
		telegram_process_private(teleCtx, channel_post);
		return;
	}
}

static void telegram_process_messages(telegram_ctx_t *teleCtx, cJSON *messages)
{
	uint32_t i;
	cJSON *update_id;


	for (i = 0 ; i < cJSON_GetArraySize(messages) ; i++)
	{
		cJSON *subitem = cJSON_GetArrayItem(messages, i);

		update_id = cJSON_GetObjectItem(subitem, "update_id");
		teleCtx->last_update_id = update_id->valueint;
		ESP_LOGI(TAG, "Update id %d", teleCtx->last_update_id);
		telegram_parse_message(teleCtx, subitem);
	}
}

static void telegram_parse_messages(telegram_ctx_t *teleCtx, const char *buffer)
{
	cJSON *json = cJSON_Parse(buffer);
	cJSON *ok_item = cJSON_GetObjectItem(json, "ok");

	if (cJSON_IsBool(ok_item) && (ok_item->valueint))
	{
		cJSON *messages = cJSON_GetObjectItem(json, "result");
		if (cJSON_IsArray(messages))
		{
			telegram_process_messages(teleCtx, messages);
		}
	}

	cJSON_Delete(json);
}

static void telegram_getMessages(telegram_ctx_t *teleCtx)
{
	char post_data[32];
	char *buffer = NULL;
	int data_read = 0;
    int total_data_read = 0; 
	int content_length;
	esp_err_t err;
	esp_http_client_handle_t client;


	if (teleCtx->last_update_id)
	{
		sprintf(post_data, "limit=10&offset=%d", teleCtx->last_update_id + 1);
	} else
	{
		sprintf(post_data, "limit=10");
	}

	client = esp_http_client_init(&teleCtx->httpClientCfg);
	if (client  == NULL)
	{
		ESP_LOGE(TAG, "Failed to initialise HTTP connection");
		return;
	}
	sprintf(teleCtx->path, TELEGRAM_SERVER"/bot%s/getUpdates?%s", teleCtx->token, post_data);
	esp_http_client_set_url(client, teleCtx->path);
    esp_http_client_set_method(client, HTTP_METHOD_GET);

	ESP_LOGI(TAG, "open... %s PDATA %s\r\n", teleCtx->path, post_data);
    err = esp_http_client_open(client, 0);
    if (err == ESP_OK) {
    	esp_http_client_fetch_headers(client);

    	content_length =  esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
               content_length);
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        esp_http_client_close(client);
        return;
    }

    if (content_length < TELEGRAM_MAX_BUFFER)
    {
    	buffer = calloc(1, content_length + 1);
    } else
    {
     	esp_http_client_close(client);
    	return;
    }

    while (1) {
        data_read = esp_http_client_read(client, &buffer[total_data_read], content_length);
        total_data_read += data_read;
        if (data_read < 0)
        {
        	break;
        }


        if ((data_read == 0)) {
            ESP_LOGI(TAG, "Connection closed,all data received total_data_read = %d content_length = %d data_read = %d",
            	total_data_read, content_length, data_read);
            break;
        }
    }
    
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

 	ESP_LOGW(TAG, "RCV: %s\r\n", buffer);

 	if (data_read >= 0)
 	{
 		telegram_parse_messages(teleCtx, buffer);
 	}

 	free(buffer);
}

static void telegram_timer_cb(TimerHandle_t pxTimer) 
{
	telegram_ctx_t *teleCtx = (telegram_ctx_t *)pvTimerGetTimerID(pxTimer);
	teleCtx->is_timer = true;
}

static void telegram_task(void * param)
{
	telegram_ctx_t *teleCtx = (telegram_ctx_t *)param;

    ESP_LOGI(TAG, "Start... %s", 	teleCtx->token );
	teleCtx->timer = xTimerCreate("TelegramTimer", TIMER_INTERVAL_MSEC / portTICK_RATE_MS,
        pdTRUE, teleCtx, telegram_timer_cb);
	xTimerStart(teleCtx->timer, 0);

	while(!teleCtx->stop)
	{
		if(teleCtx->is_timer)
		{
			telegram_getMessages(teleCtx);
			teleCtx->is_timer = false;
		}
		vTaskDelay(1);
	}

	free(teleCtx->token);
}

void telegram_stop(void *teleCtx_ptr)
{
	telegram_ctx_t *teleCtx = NULL;

	if (teleCtx_ptr == NULL)
	{
		return;
	}

	teleCtx = (telegram_ctx_t *)teleCtx_ptr;

	teleCtx->stop = true;	
}

void *telegram_init(const char *token, telegram_on_msg_cb_t on_msg_cb)
{
	telegram_ctx_t *teleCtx = NULL;

	if (on_msg_cb == NULL)
	{
		return NULL;
	} 

	teleCtx = calloc(1, sizeof(telegram_ctx_t));
	if (teleCtx != NULL)
	{
		teleCtx->token = strdup(token);
		teleCtx->httpClientCfg.event_handler = telegram_http_event_handler;
		teleCtx->httpClientCfg.url = teleCtx->path;
		teleCtx->httpClientCfg.timeout_ms = 5000;
		teleCtx->on_msg_cb = on_msg_cb;
	}

	sprintf(teleCtx->path, TELEGRAM_SERVER"/bot%s/getUpdates", teleCtx->token);
	xTaskCreate(&telegram_task, "telegram_task", 8192, teleCtx, 5, NULL);

	return teleCtx;
}

static void telegram_send_message(void *teleCtx_ptr, double chat_id, const char *message, const char *additional_json)
{
	esp_err_t err;
	esp_http_client_handle_t client;
	char *payload = NULL;
	telegram_ctx_t *teleCtx = NULL;

	if (teleCtx_ptr == NULL)
	{
		ESP_LOGE(TAG, "Send message: Null argument");
		return;
	}

	teleCtx = (telegram_ctx_t *)teleCtx_ptr;
	payload = calloc(sizeof(char), strlen(message) + 1
		+ ((additional_json == NULL) ? strlen(TELEGRMA_MSG_FMT) : (strlen(TELEGRMA_MSG_MARKUP_FMT) + strlen(additional_json))));

    client = esp_http_client_init(&teleCtx->httpClientCfg);

	sprintf(teleCtx->path, TELEGRAM_SERVER"/bot%s/sendMessage", teleCtx->token);
	if (additional_json == NULL)
	{
		sprintf(payload, TELEGRMA_MSG_FMT, chat_id, message);
	} else
	{
		sprintf(payload, TELEGRMA_MSG_MARKUP_FMT, chat_id, message, additional_json);
	} 
    ESP_LOGI(TAG, "Send message: %s", payload);

    do
    {
		err = esp_http_client_set_url(client, teleCtx->path);
		if (err != ESP_OK)
		{
			ESP_LOGE(TAG, "esp_http_client_set_url failed err %d", err);
			break;
		}

		err = esp_http_client_set_header(client, "Content-Type", "application/json");
		if (err != ESP_OK)
		{
			ESP_LOGE(TAG, "Send message: esp_http_client_set_header failed err %d", err);
			break;
		}

	    err = esp_http_client_set_method(client, HTTP_METHOD_POST);
	    if (err != ESP_OK)
		{
			ESP_LOGE(TAG, "Send message: esp_http_client_set_method failed err %d", err);
			break;
		}

		err = esp_http_client_set_post_field(client, payload, strlen(payload));
		if (err != ESP_OK)
		{
			ESP_LOGE(TAG, "Send message: esp_http_client_set_post_field failed err %d", err);
			break;
		}

		err= esp_http_client_perform(client);
		if (err != ESP_OK)
		{
			ESP_LOGE(TAG, "Send message: esp_http_client_perform failed err %d", err);
			break;
		}
	} while (0);

	esp_http_client_cleanup(client);
	free(payload);
}

static char *telegram_make_markup_kbrd(telegram_kbrd_markup_t *kbrd)
{
	return NULL;
}

static char *telegram_make_inline_kbrd(telegram_kbrd_inline_t *kbrd)
{
	char *str = NULL;
	size_t reqSize = 0;
	size_t count = 0;
	telegram_kbrd_inline_btn_t *btn = kbrd->buttons;

	while (btn->text)
	{
		reqSize += strlen(btn->text) + strlen(btn->callback_data);
		btn++;
		count++;
	}

	if (reqSize == 0)
	{
		return NULL;
	}

	str = (char *)calloc(sizeof(char), reqSize + count*strlen(TELEGRAM_INLINE_BTN_FMT) + count + 1
		+ strlen(TELEGRAM_INLINE_KBRD_FMT) + 2 + 1); //count +1 number of comas, 2 - brackets

	if (str == NULL)
	{
		return NULL;
	}

	count = sprintf(str, TELEGRAM_INLINE_KBRD_FMT"[");
	btn = kbrd->buttons;
	while (btn->text)
	{
		count += sprintf(&str[count], TELEGRAM_INLINE_BTN_FMT, btn->text, btn->callback_data);
		btn++;
		if (btn->text)
		{
			count += sprintf(&str[count], ",");
		}
	}

	sprintf(&str[count], "]");
	return str;
}

void telegram_kbrd(void *teleCtx_ptr, double chat_id, const char *message, telegram_kbrd_t *kbrd)
{
	char *json_res = NULL;

	if ((kbrd == NULL) || (teleCtx_ptr == NULL))
	{
		ESP_LOGE(TAG, "telegram_kbrd ARG is NULL");
		return;
	}

	switch (kbrd->type)
	{
		case TELEGRAM_KBRD_MARKUP:
			json_res = telegram_make_markup_kbrd(&kbrd->kbrd.markup);
			break;

		case TELEGRAM_KBRD_INLINE:
			json_res = telegram_make_inline_kbrd(&kbrd->kbrd.inl);
			break;

		default:
			ESP_LOGE(TAG, "Bad keyboard type");
			return;
	}

	telegram_send_message(teleCtx_ptr, chat_id, message, json_res);
	free(json_res);
}

void telegram_send_text_message(void *teleCtx_ptr, double chat_id, const char *message)
{
	telegram_send_message(teleCtx_ptr, chat_id, message, NULL);
}
