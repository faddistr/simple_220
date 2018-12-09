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
#include "httpd_back.h"
#include "telegram.h"
#include "config.h"
#include "cmd_executor.h"
#include "cmd_manager.h"


#define CONFIG_BUTTON 25U

static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;

static const char *TAG="MAIN";

static EventGroupHandle_t s_wifi_event_group;
static config_t config;
static bool is_config = false;
static void *teleCtx;
static void *server;


static void smartconfig_task(void * parm);
static void sc_callback(smartconfig_status_t status, void *pdata);

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

static void telegram_new_message(void *teleCtx, telegram_update_t *info)
{
    cmd_additional_info_t *cmd_info = calloc(1, sizeof(cmd_additional_info_t));

    if (info == NULL)
    {
        return;
    }

    cmd_info->transport = CMD_SRC_TELEGRAM;
    cmd_info->sys_config = &config;
    cmd_info->arg = teleCtx;

    //cmd_execute(info, cmd_info);
    if (cmd_info->sys_config_changed)
    {
        config_save(cmd_info->sys_config);
    }

    free(cmd_info);
}

static void httpd_back_send(void *ctx, void *buff, uint32_t buff_len)
{
    if (buff_len == 0)
    {
        return;
    }

    httpd_send_answ(ctx, buff, buff_len);
}

static void httpd_back_new_message(void *ctx, httpd_arg_t *argv, uint32_t argc, void *sess)
{
    uint32_t i;
    cmd_additional_info_t *info = calloc(1, sizeof(cmd_additional_info_t));

    if (info == NULL)
    {
        return;
    }

    info->transport = CMD_SRC_HTTPB;
    info->send_cb = httpd_back_send;
    info->arg = ctx;
    info->user_ses = sess;
    info->sys_config = &config;
    for (i = 0; i < argc; i++)
    {
        cmd_execute(argv[i].key, argv[i].value, info);
        httpd_set_sess(ctx, info->user_ses);
        if (info->sys_config_changed)
        {
            config_save(info->sys_config);
        }
    }

    httpd_send_answ(ctx, NULL, 0);
    free(info);
}

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
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
            ESP_LOGI(TAG, "Got IP: '%s'",
                    ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
            teleCtx = telegram_init(config.telegram_token, telegram_new_message);

            /* Start the web server */
            if (server == NULL) {
                server = httpd_start_webserver(httpd_back_new_message);
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
            telegram_stop(&teleCtx);
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

static void initialise_wifi(void)
{
    wifi_config_t wifi_config = {0};

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    s_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    if (true)//!check_button_press())
    {
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
    } else
    {
        config_t null_config = {};
        ESP_LOGI(TAG, "Erasing config...");
        config_save(&null_config);
    }

    ESP_ERROR_CHECK(esp_wifi_start());
}


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
    cmd_init();
    cmd_manager_register_all();
    initialise_wifi();
}
