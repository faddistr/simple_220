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

#define DEFAULT_SNTP_SERVER "pool.ntp.org"

static const char *TAG="SNTP";

static void init_sntp_client(void);
static void sntp_notification_cb(struct timeval *tv);

static void ip_event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	init_sntp_client();
}

static void sntp_notification_cb(struct timeval *tv)
{
	char strftime_buf[64];
    time_t now;
    struct tm timeinfo;

	ESP_LOGI(TAG, "Time sync...");
	settimeofday(tv, NULL);
	localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

    ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);
}

static void init_sntp_client(void)
{
	char *sntp_server = var_get("SNTP_SERVER");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);

	if (sntp_server == NULL)
	{   
    	sntp_setservername(0, DEFAULT_SNTP_SERVER);    } else
    {
    	sntp_setservername(0, sntp_server);
    	free(sntp_server);	
    }

    sntp_set_time_sync_notification_cb(sntp_notification_cb);
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    sntp_init();
}

static void init_sntp_module(void)
{
	ESP_LOGI(TAG, "Initializing SNTP module...");
	//TODO
//	setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
//	tzset();
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, ip_event_handler, NULL));
}

static void cmd_gettime(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
	char strftime_buf[64];
    time_t now;
    struct tm timeinfo;
    
    telegram_event_msg_t *evt = (telegram_event_msg_t *)info->cmd_data;
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

    telegram_send_text(info->arg, evt->chat_id, NULL, "%s %d", strftime_buf, sntp_get_sync_status());
}

cmd_register_static({
    {
        .name = "time",
        .cmd_cb = cmd_gettime,
    },
});

module_init(init_sntp_module);
