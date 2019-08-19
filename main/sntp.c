#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_sntp.h>
#include "telegram.h"
#include "telegram_events.h"
#include "ram_var_stor.h"
#include "module.h"
#include "cmd_executor.h"

#define DEFAULT_SNTP_SERVER "1.ua.pool.ntp.org"
#define DEFAULT_TIME_ZONE 	"CET-2CEST,M3.5.0/2,M10.5.0/3"

static const char *TAG="SNTP_MODULE";

static void init_sntp_client(void);
static void sntp_notification_cb(struct timeval *tv);

static void ip_event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	init_sntp_client();
}

static void sntp_notification_cb(struct timeval *tv)
{
	struct timeval tv_now;
	char strftime_buf[64];
    time_t now;
    struct tm *timeinfo;
	
	now = tv->tv_sec;
	timeinfo = localtime(&now);

	ESP_LOGI(TAG, "Time sync...");
	strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", timeinfo);

	ESP_LOGI(TAG, "Received date/time is: %s", strftime_buf);

	gettimeofday(&tv_now, NULL);

	now = tv_now.tv_sec;
	timeinfo = localtime(&now);


	strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", timeinfo);
    ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);
}

static void init_sntp_client(void)
{
	char *sntp_server = var_get("SNTP_SERVER_ADDRESS");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);

	if (sntp_server == NULL)
	{   
    	sntp_setservername(0, DEFAULT_SNTP_SERVER);    
    } else
    {
    	sntp_setservername(0, sntp_server);
    	free(sntp_server);	
    }

    sntp_set_time_sync_notification_cb(sntp_notification_cb);
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_init();
}

static void init_sntp_module(void)
{
	char *tz = var_get("TIME_ZONE");

	ESP_LOGI(TAG, "Initializing SNTP module...");
	if (tz == NULL)
	{
		setenv("TZ", DEFAULT_TIME_ZONE, 1);
	} else
	{
		setenv("TZ", tz, 1);
		free(tz);
	}

	tzset();
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, ip_event_handler, NULL));
}

static void cmd_gettime(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	struct timeval tv_now;
	char strftime_buf[64];
    time_t now;
    struct tm *timeinfo;
	telegram_event_msg_t *evt = (telegram_event_msg_t *)info->cmd_data;

    gettimeofday(&tv_now, NULL);
    now = tv_now.tv_sec;
	timeinfo = localtime(&now);
	strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", timeinfo);
    telegram_send_text(info->arg, evt->chat_id, NULL, "%s", strftime_buf);
}

cmd_register_static({
    {
        .name = "time",
        .cmd_cb = cmd_gettime,
    },
});

module_init(init_sntp_module);
