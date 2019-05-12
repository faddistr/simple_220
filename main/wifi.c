#include <string.h>
#include <tcpip_adapter.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include "ram_var_stor.h"
#include "module.h"

static const char *TAG="wifi_client";

static void event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch(event_id) {
            case WIFI_EVENT_STA_START:
                var_add("WIFI_CLIENT_STAT", "WIFI_EVENT_STA_START");
                ESP_ERROR_CHECK(esp_wifi_connect());
                break;

            case WIFI_EVENT_STA_CONNECTED:
                var_add("WIFI_CLIENT_STAT", "WIFI_EVENT_STA_CONNECTED");
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
            	var_add("WIFI_CLIENT_STAT", "WIFI_EVENT_STA_DISCONNECTED");
                ESP_ERROR_CHECK(esp_wifi_connect());
                break;

            case WIFI_EVENT_STA_STOP:
                var_add("WIFI_CLIENT_STAT", "WIFI_EVENT_STA_STOP");
                break;

            default:
                ESP_LOGI(TAG, "WIFI_EVENT_ID = %d", event_id);
                break;
        }
    }

    if (event_base == IP_EVENT)
    {
        switch(event_id)
        {
            case IP_EVENT_STA_GOT_IP:
            {
                ip_event_got_ip_t *info = (ip_event_got_ip_t *)event_data;

                if (info->if_index == TCPIP_ADAPTER_IF_STA)
                {
                    var_add("WIFI_CLIENT_IP", ip4addr_ntoa(&info->ip_info.ip));
                    var_add("WIFI_CLIENT_NETMASK", ip4addr_ntoa(&info->ip_info.netmask));
                    var_add("WIFI_CLIENT_GW", ip4addr_ntoa(&info->ip_info.gw));
                    var_add("WIFI_CLIENT_IPCHG", info->ip_changed?"1":"0");
                    var_add("WIFI_CLIENT_STAT", "IP_EVENT_STA_GOT_IP");
                }
            }
            break;

            case IP_EVENT_GOT_IP6:
            {
                ip_event_got_ip6_t *info = (ip_event_got_ip6_t *)event_data;

                if (info->if_index == TCPIP_ADAPTER_IF_STA)
                {
                    var_add("WIFI_CLIENT_IPV6", ip6addr_ntoa(&info->ip6_info.ip));
                }
            }
            break;


            default:
                ESP_LOGI(TAG, "IP_EVENT_ID = %d", event_id);
                break;
        }
    }
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
    ESP_ERROR_CHECK(esp_event_handler_register(ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, event_handler, NULL));
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