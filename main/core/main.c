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
#include <driver/gpio.h>
#include "ota.h"

#include "telegram.h"
#include "config.h"
#include "cmd_executor.h"
#include "ram_var_stor.h"
#include "module.h"

#define CONFIG_BUTTON 25U

static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;

static const char *TAG="MAIN";

static bool check_button_press(void)
{
    static const gpio_config_t button_conf = {
        .intr_type = GPIO_PIN_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = 1ULL<<CONFIG_BUTTON,
        .pull_down_en = 0,
        .pull_up_en = 0,
    };

    gpio_config(&button_conf);
    return gpio_get_level(CONFIG_BUTTON);
}

#if 0
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
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
        {
            char *tmp = strdup(ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            ESP_LOGI(TAG, "SYSTM_EVENT_STA_GOT_IP");
            xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
            var_add("WIFI_CLIENT_IP", tmp);
            free(tmp);
            teleCtx = telegram_init(config.telegram_token, telegram_new_message);

            /* Start the web server */
            if (server == NULL) {
                server = httpd_start_webserver(httpd_back_new_message);
            }
        }
            break;

        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
            xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
            ESP_ERROR_CHECK(esp_wifi_connect());

            /* Stop the web server */
            if (server) {
                httpd_stop_webserver(server);
                server = NULL;
            }
            if (teleCtx)
            {
                telegram_stop(&teleCtx);
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

#endif

void app_main()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    var_init();
    ESP_ERROR_CHECK(config_load_vars());
    module_init_all();
}
