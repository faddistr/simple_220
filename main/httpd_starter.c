#include "httpd_back.h"
#include "module.h"
#include <string.h>
#include <esp_log.h>
#include "ram_var_stor.h"
#include "cmd_executor.h"
static const char *TAG="HTTPD_STARTER";

static void *server;

//TODO: check password
static void httpd_back_new_message(void *ctx, httpd_arg_t *argv, uint32_t argc, void *sess)
{
    uint32_t i;
    cmd_additional_info_t *info = calloc(1, sizeof(cmd_additional_info_t));

    if (info == NULL)
    {
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

static void httpd_starter_init(void)
{
    char *httpd_password;
    char *httpd_disable = var_get("HTTP_DISABLE");

    if (httpd_disable != NULL)
    {
        if (!strcmp(httpd_disable, "1"))
        {
            free(httpd_disable);
            ESP_LOGI(TAG, "HTTPD module disabled...");
            return;
        }
    }
    ESP_LOGI(TAG, "HTTPD module init...");
    free(httpd_disable);
    server = httpd_start_webserver(httpd_back_new_message);

    if (server == NULL)
    {
         ESP_LOGI(TAG, "HTTPD module failed...");
    }
}

//module_init(httpd_starter_init);