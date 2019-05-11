#include <string.h>
#include <tcpip_adapter.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include "ram_var_stor.h"
#include "module.h"

static const char *TAG="wifi_client";

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
        case SYSTEM_EVENT_STA_START:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
            var_add("WIFI_CLIENT_STAT", "SYSTEM_EVENT_STA_START");
            ESP_ERROR_CHECK(esp_wifi_connect());
            break;

        case SYSTEM_EVENT_STA_GOT_IP:
        	{
	            char *tmp = strdup(ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
	            var_add("WIFI_CLIENT_IP", tmp);
	           	ESP_LOGI(TAG, "IP: %s", tmp);
	           	free(tmp);
	            var_add("WIFI_CLIENT_STAT", "SYSTEM_EVENT_STA_GOT_IP");
        	}
            break;

        case SYSTEM_EVENT_STA_DISCONNECTED:
        	var_add("WIFI_CLIENT_STAT", "SYSTEM_EVENT_STA_DISCONNECTED");
            ESP_ERROR_CHECK(esp_wifi_connect());
            break;
        default:
            break;
    }

    return ESP_OK;
}

static void initialise_wifi(void)
{
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t wifi_config = {0};
    char *ssid = var_get("WIFI_CLIENT_SSID");
    char *password = var_get("WIFI_CLIENT_PASSWORD");

    if ((ssid == NULL) || (password == NULL))
    {
    	ESP_LOGW(TAG, "Bad config!");
    	free(ssid);
    	free(password);
    	return;
    }

    strcpy((char *)wifi_config.sta.ssid, ssid);
    strcpy((char *)wifi_config.sta.password, password);
    free(ssid);
    free(password);

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void wifi_client_init(void)
{
	char *wifi_client_disable = var_get("WIFI_CLIENT_DISABLE");

	if (wifi_client_disable != NULL)
	{
		if (!strcmp(wifi_client_disable, "1"))
		{
			free(wifi_client_disable);
			ESP_LOGI(TAG, "WiFi client disabled...");
			return;
		}
	}

	free(wifi_client_disable);
	ESP_LOGI(TAG, "WiFi client init...");
	initialise_wifi();
}

module_init(wifi_client_init);