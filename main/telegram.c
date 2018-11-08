
#include <string.h>
#include <stdbool.h>
#include <cJSON.h>
#include <esp_log.h>
#include <esp_http_client.h>
#include "freertos/timers.h"
#include "telegram.h"

#define TELEGRAM_MAX_PATH 128U
#define TELEGRAM_MAX_BUFFER 4095U

bool is_timer = false;

#define TIMER_INTERVAL_MSEC    (1L * 5L * 1000L)


typedef struct telegram_ctx
{
    char *token;	
    char bot_name[16];
	char path[TELEGRAM_MAX_PATH + 1];
	int32_t last_update_id;
	bool stop;
	esp_http_client_config_t httpClientCfg;
	TimerHandle_t timer;
} telegram_ctx_t;


static const char *TAG="telegram";

static telegram_ctx_t teleCtx;

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

static void telegram_send_text_message(double chat_id, const char *message)
{
	int content_length;
	esp_err_t err;
	char *payload = calloc(sizeof(char), strlen(message) + 64); /* TODO */
	esp_http_client_handle_t client = esp_http_client_init(&teleCtx.httpClientCfg);

	sprintf(teleCtx.path, TELEGRAM_SERVER"/bot%s/sendMessage", teleCtx.token);
	sprintf(payload, "{\"chat_id\": \"%f\", \"text\": \"%s:%s\"}", chat_id, teleCtx.bot_name, message);
    ESP_LOGI(TAG, "Send message: %s", payload);


	esp_http_client_set_url(client, teleCtx.path);
	esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_method(client, HTTP_METHOD_POST);
	esp_http_client_set_post_field(client, payload, strlen(payload));
	esp_http_client_perform(client);
	esp_http_client_close(client);
	esp_http_client_cleanup(client);
	free(payload);
}

static void telegram_new_message(double chat_id, const char *from, const char *text)
{
	ESP_LOGI(TAG, "New message: FROM: %s TEXT %s CHAT ID %f", from, text, chat_id);
	telegram_send_text_message(chat_id, text);
}

static void telegram_process_messages(cJSON *messages)
{
	uint32_t i;
	cJSON *update_id;
	cJSON *channel_post;

	for (i = 0 ; i < cJSON_GetArraySize(messages) ; i++)
	{
		cJSON *subitem = cJSON_GetArrayItem(messages, i);

		update_id = cJSON_GetObjectItem(subitem, "update_id");
		teleCtx.last_update_id = update_id->valueint;
		ESP_LOGI(TAG, "Update id %d", teleCtx.last_update_id);

		channel_post = cJSON_GetObjectItem(subitem, "channel_post");
		if (channel_post == NULL)
		{
			channel_post = cJSON_GetObjectItem(subitem, "private");
		}
		if (channel_post != NULL)
		{
			cJSON *chat = cJSON_GetObjectItem(channel_post, "chat"); 
			cJSON *chat_id = cJSON_GetObjectItem(chat, "id"); 
			cJSON *chat_title = cJSON_GetObjectItem(chat, "title"); 
			cJSON *text = cJSON_GetObjectItem(channel_post, "text");

			if (text != NULL)
			{
				telegram_new_message(chat_id->valuedouble, chat_title->valuestring, text->valuestring);
			}
		} 
	}
}

static void telegram_parse_messages(const char *buffer)
{
	cJSON *json = cJSON_Parse(buffer);
	cJSON *ok_item = cJSON_GetObjectItem(json, "ok");

	if (cJSON_IsBool(ok_item) && (ok_item->valueint))
	{
		cJSON *messages = cJSON_GetObjectItem(json, "result");
		if (cJSON_IsArray(messages))
		{
			telegram_process_messages(messages);
		}
	}

	cJSON_Delete(json);
}

static void telegram_getMessages(void)
{
	char post_data[32];
	char *buffer = NULL;
	int data_read = 0;
    int total_data_read = 0; 
	int content_length;
	esp_err_t err;
	esp_http_client_handle_t client;


	if (teleCtx.last_update_id)
	{
		sprintf(post_data, "limit=10&offset=%d", teleCtx.last_update_id + 1);
	} else
	{
		sprintf(post_data, "limit=10");
	}
	client = esp_http_client_init(&teleCtx.httpClientCfg);
	if (client  == NULL)
	{
		ESP_LOGE(TAG, "Failed to initialise HTTP connection");
		return;
	}
	sprintf(teleCtx.path, TELEGRAM_SERVER"/bot%s/getUpdates?%s", teleCtx.token, post_data);
	esp_http_client_set_url(client, teleCtx.path);
    esp_http_client_set_method(client, HTTP_METHOD_GET);

	ESP_LOGI(TAG, "open... %s PDATA %s\r\n", teleCtx.path, post_data);
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
 		telegram_parse_messages(buffer);
 	}
 	free(buffer);
}

static void telegram_timer_cb(TimerHandle_t pxTimer) 
{
	is_timer = true;
}

static void telegram_task(void * parm)
{
    ESP_LOGI(TAG, "Start... %s", 	teleCtx.token );

	teleCtx.timer = xTimerCreate("TelegramTimer", TIMER_INTERVAL_MSEC / portTICK_RATE_MS,
        pdTRUE, NULL, telegram_timer_cb);
	xTimerStart(teleCtx.timer, 0);

	while(!teleCtx.stop)
	{
		if(is_timer)
		{
			telegram_getMessages();
			is_timer = false;
		}
		vTaskDelay(1);
	}

	free(teleCtx.token);
}

void telegram_stop(void)
{
	teleCtx.stop = true;	
}

void telegram_init(const char *token)
{
	teleCtx.token = strdup(token);
	snprintf(teleCtx.bot_name, sizeof(teleCtx.bot_name), "smart_220");
	teleCtx.httpClientCfg.event_handler = telegram_http_event_handler;
	teleCtx.httpClientCfg.url = teleCtx.path;
	teleCtx.httpClientCfg.timeout_ms = 5000;
	teleCtx.stop = false;

	sprintf(teleCtx.path, TELEGRAM_SERVER"/bot%s/getUpdates", teleCtx.token);
	xTaskCreate(&telegram_task, "telegram_task", 8192, NULL, 5, NULL);
}
