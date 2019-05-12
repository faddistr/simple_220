#include "httpd_back.h"
#include "module.h"
#include <string.h>
#include <esp_log.h>
#include <esp_event.h>
#include "ram_var_stor.h"
#include "cmd_executor.h"
static const char *TAG="HTTPD_MANAGER";

static void *server;
static char *httpd_password;

static void httpd_back_new_message(void *ctx, httpd_arg_t *argv, uint32_t argc, void *sess)
{
    uint32_t i;
    cmd_additional_info_t *info = NULL;

    if (httpd_password != NULL)
    {
        for (i = 0; i < argc; i++)
        {
           if (!strcmp(argv[i].key, "pass"))
           {
                if (strcmp(argv[i].value, httpd_password))
                {
                    ESP_LOGW(TAG, "Incorrect password!");
                    httpd_send_answ(info->arg, "FAIL: Incorrect password!", 0);
                    return;
                }
           } 
        } 
    }

    info = calloc(1, sizeof(cmd_additional_info_t));
    if (info == NULL)
    {
        ESP_LOGE(TAG, "No mem for respond.");
        return;
    }

    info->transport = CMD_SRC_HTTPB;
    info->arg = ctx;
    info->user_ses = sess;
    for (i = 0; i < argc; i++)
    {
        info->cmd_data = &argv[i].value;
        cmd_execute(argv[i].key, info);
        httpd_set_sess(ctx, info->user_ses);
    }

    free(info);
}

static void httpd_event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch(event_id) {
        case IP_EVENT_STA_GOT_IP:
            if (server == NULL)
            {
                server = httpd_start_webserver(httpd_back_new_message);

                if (server == NULL)
                {
                    ESP_LOGI(TAG, "HTTPD module failed...");
                    var_add("HTTPD_STAT", "FAILED");
                } else
                {
                    var_add("HTTPD_STAT", "RUNNING");
                    httpd_password = var_get("HTTPD_PASSWORD");
                }
            }
        break;

        case IP_EVENT_STA_LOST_IP:
            httpd_stop_webserver(server);
            free(httpd_password);
            server = NULL;
            httpd_password = NULL;
            var_add("HTTPD_STAT", "NO_IP");
            break;

        default:
            ESP_LOGI(TAG, "IP_EVENT_ID = %d", event_id);
            break;
    }
}

static void httpd_starter_init(void)
{
    char *httpd_disable = var_get("HTTPD_DISABLE");

    if (httpd_disable != NULL)
    {
        if (!strcmp(httpd_disable, "1"))
        {
            free(httpd_disable);
            var_add("HTTPD_STAT", "MODULE_DISABLED");
            ESP_LOGI(TAG, "HTTPD module disabled...");
            return;
        }
    }
    ESP_LOGI(TAG, "HTTPD module init...");
    free(httpd_disable);

    var_add("HTTPD_STAT", "MODULE_INIT");
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, httpd_event_handler, NULL));
}

module_init(httpd_starter_init);