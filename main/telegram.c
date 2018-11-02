
#include <string.h>
#include <stdbool.h>
#include <cJSON.h>
#include <esp_log.h>
#include <esp_http_client.h>
#include "freertos/timers.h"
#include "telegram.h"

#define TELEGRAM_MAX_PATH 128U
#define TELEGRAM_MAX_BUFFER 512U

bool is_timer = false;

#define TIMER_INTERVAL_MSEC    (1L * 5L * 1000L)


typedef struct telegram_ctx
{
	const char *token;	
	char path[TELEGRAM_MAX_PATH + 1];
	char *buffer;
	esp_http_client_config_t httpClientCfg;
	esp_http_client_handle_t httpClientHnd;
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

static void telegram_getMessages(void)
{
	int data_read = 0;
    int total_data_read = 0; 
	int content_length;
	esp_err_t err;

	sprintf(teleCtx.path, TELEGRAM_SERVER"/bot%s/getUpdates", teleCtx.token);
	esp_http_client_set_url(teleCtx.httpClientHnd, teleCtx.path);

	ESP_LOGI(TAG, "open... %s\r\n", teleCtx.path);
    err = esp_http_client_open(teleCtx.httpClientHnd, 0);
    if (err == ESP_OK) {
    	esp_http_client_fetch_headers(teleCtx.httpClientHnd);

    	content_length =  esp_http_client_get_content_length(teleCtx.httpClientHnd);
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                esp_http_client_get_status_code(teleCtx.httpClientHnd),
               content_length);
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        esp_http_client_close(teleCtx.httpClientHnd);
        return;
    }

    if (content_length < TELEGRAM_MAX_BUFFER)
    {
    	teleCtx.buffer = calloc(1, content_length + 1);
    } else
    {
     	esp_http_client_close(teleCtx.httpClientHnd);
    	return;
    }

    while (1) {
        data_read = esp_http_client_read(teleCtx.httpClientHnd, &teleCtx.buffer[total_data_read], content_length);
        total_data_read += data_read;
        if ((data_read == 0)) {
        	 	ESP_LOGW(TAG, "RCV: %s\r\n", teleCtx.buffer);
            ESP_LOGI(TAG, "Connection closed,all data received total_data_read = %d content_length = %d data_read = %d",
            	total_data_read, content_length, data_read);
            break;
        }
    }

 	ESP_LOGW(TAG, "RCV: %s\r\n", teleCtx.buffer);

 	free(teleCtx.buffer);
 	esp_http_client_close(teleCtx.httpClientHnd);
}

static void telegram_timer_cb(TimerHandle_t pxTimer) 
{
	is_timer = true;
}

static void telegram_task(void * parm)
{
    ESP_LOGI(TAG, "Start...");

	teleCtx.timer = xTimerCreate("TelegramTimer", TIMER_INTERVAL_MSEC / portTICK_RATE_MS,
        pdTRUE, NULL, telegram_timer_cb);
	xTimerStart(teleCtx.timer, 0);

	while(1)
	{
		if(is_timer)
		{
			telegram_getMessages();
			is_timer = false;
		}
		vTaskDelay(1);
	}

	esp_http_client_cleanup(teleCtx.httpClientHnd);
}

void telegram_init(const char *token)
{
	teleCtx.token = token;
	teleCtx.httpClientCfg.event_handler = telegram_http_event_handler;
	teleCtx.httpClientCfg.url = teleCtx.path;

	sprintf(teleCtx.path, TELEGRAM_SERVER"/bot%s/getUpdates", teleCtx.token);

	teleCtx.httpClientHnd = esp_http_client_init(&teleCtx.httpClientCfg);
	if (teleCtx.httpClientHnd  == NULL)
	{
		ESP_LOGE(TAG, "Failed to initialise HTTP connection");
		return;
	}
	xTaskCreate(&telegram_task, "telegram_task", 8192, NULL, 5, NULL);
}