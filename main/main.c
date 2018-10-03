#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <tcpip_adapter.h>
#include <esp_smartconfig.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>

#include "httpd_back.h"
#include "config.h"

#define EXAMPLE_WIFI_SSID "some WiFi"
#define EXAMPLE_WIFI_PASS "some pass"
#if 0
static config_t default_config = 
{
    .ssid = EXAMPLE_WIFI_SSID,
    .password = EXAMPLE_WIFI_PASS,
};
#endif
static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;

static const char *TAG="MAIN";

static EventGroupHandle_t s_wifi_event_group;
static config_t config;
static bool is_config = false;

static void smartconfig_task(void * parm);
static void sc_callback(smartconfig_status_t status, void *pdata);

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    httpd_handle_t *server = (httpd_handle_t *) ctx;

    switch(event->event_id) {
        case SYSTEM_EVENT_STA_START:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
            if (is_config)
            {
                ESP_ERROR_CHECK(esp_wifi_connect());
            } else
            {
                ESP_LOGI(TAG, "Starting esptouch...");
                xTaskCreate(smartconfig_task, "smartconfig_task", 4096, NULL, 3, NULL);
            }
            break;

        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
            ESP_LOGI(TAG, "Got IP: '%s'",
                    ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);

            /* Start the web server */
            if (*server == NULL) {
                *server = httpd_start_webserver();
            }
            break;

        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
            xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
            ESP_ERROR_CHECK(esp_wifi_connect());

            /* Stop the web server */
            if (*server) {
                httpd_stop_webserver(*server);
                *server = NULL;
            }
            break;
        default:
            break;
    }

    return ESP_OK;
}

static void sc_callback(smartconfig_status_t status, void *pdata)
{
    switch (status) {
        case SC_STATUS_WAIT:
            ESP_LOGI(TAG, "SC_STATUS_WAIT");
            break;
        case SC_STATUS_FIND_CHANNEL:
            ESP_LOGI(TAG, "SC_STATUS_FINDING_CHANNEL");
            break;
        case SC_STATUS_GETTING_SSID_PSWD:
            ESP_LOGI(TAG, "SC_STATUS_GETTING_SSID_PSWD");
            break;
        case SC_STATUS_LINK:
            ESP_LOGI(TAG, "SC_STATUS_LINK");
            wifi_config_t *wifi_config = pdata;
            ESP_LOGI(TAG, "SSID:%s", wifi_config->sta.ssid);
            ESP_LOGI(TAG, "PASSWORD:%s", wifi_config->sta.password);
            ESP_ERROR_CHECK( esp_wifi_disconnect() );
            ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config) );
            ESP_ERROR_CHECK( esp_wifi_connect() );
            strncpy(config.ssid, (char *)wifi_config->sta.ssid, MAX_CONFIG_SSID);
            strncpy(config.password, (char *)wifi_config->sta.password, MAX_CONFIG_PASSWORD);
            config_save(&config);
            break;

        case SC_STATUS_LINK_OVER:
            ESP_LOGI(TAG, "SC_STATUS_LINK_OVER");
            if (pdata != NULL) {
                uint8_t phone_ip[4] = { 0 };
                memcpy(phone_ip, (uint8_t* )pdata, 4);
                ESP_LOGI(TAG, "Phone ip: %d.%d.%d.%d\n", phone_ip[0], phone_ip[1], phone_ip[2], phone_ip[3]);
            }
            xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
            break;
        default:
            break;
    }
}

static void smartconfig_task(void * parm)
{
    EventBits_t uxBits;
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
    ESP_ERROR_CHECK( esp_smartconfig_start(sc_callback) );
    while (1) {
        uxBits = xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY); 
        if(uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi Connected to ap");
        }
        if(uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();
            vTaskDelete(NULL);
        }
    }
}

static void initialise_wifi(void *arg)
{
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_WIFI_SSID,
            .password = EXAMPLE_WIFI_PASS,
        },
    };

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    s_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, arg));
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    if (config_load(&config) != ESP_OK)
    {
        ESP_LOGI(TAG, "Failed to load config...");
    } else
    {
        ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
        strncpy((char *)wifi_config.sta.ssid, config.ssid, MAX_CONFIG_SSID);
        strncpy((char *)wifi_config.sta.password, config.password, MAX_CONFIG_PASSWORD);
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
        is_config = true;
    }

    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main()
{
    static httpd_handle_t server = NULL;
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
#if 0
    config_save(&default_config);
#endif
    initialise_wifi(&server);
}
