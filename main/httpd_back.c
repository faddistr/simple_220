#include <esp_log.h>
#include <esp_system.h>
#include <esp_http_server.h>
#include <esp_http_client.h>
#include "httpd_back.h"
#define HTTPD_MAX_QUERY_SIZE 256U

static const char *TAG="httpd_back";

typedef struct
{
    cmd_cb_t       cmd_cb;
    httpd_handle_t server;
    httpd_uri_t    cmd;
} httpd_back_internal_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt);
static esp_err_t cmd_handler(httpd_req_t *req);

static esp_http_client_config_t config =
{
    .event_handler = http_event_handler,
};

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}

static esp_err_t cmd_handler(httpd_req_t *req)
{
    httpd_back_internal_t *ctx = (httpd_back_internal_t *)req->user_ctx;
    esp_err_t res;
    char *buf = NULL;
    uint32_t i = 0;
    uint32_t arg_count;
    httpd_arg_t *arg_array = NULL;
    size_t query_size = httpd_req_get_url_query_len(req);

    if (ctx == NULL)
    {
        ESP_LOGE(TAG, "Internal error!!!");
        return ESP_OK;
    }

    if ((query_size > HTTPD_MAX_QUERY_SIZE) || (query_size == 0))
    {
        ESP_LOGW(TAG, "URL query too big or 0 %d", query_size);   
        return ESP_OK;
    }

    query_size++;
    buf = calloc(sizeof(char), query_size + 1);
    if (buf == NULL)
    {
        ESP_LOGE(TAG, "No mem!");
        return ESP_OK;   
    }

    res = httpd_req_get_url_query_str(req, buf, query_size);
    if (res != ESP_OK)
    {
        free(buf);
        ESP_LOGW(TAG, "URL query get failed %d", res);
        return ESP_OK;   
    }

    arg_count = 1; //at least one if length > 0
    while (buf[i])
    {
        if (buf[i] == '&')
        {
            arg_count++;
        }
        i++;
    }

    arg_array = calloc(sizeof(httpd_arg_t), arg_count);
    if (arg_array == NULL)
    {
        free(buf);
        ESP_LOGE(TAG, "No mem! (arg_array)");
        return ESP_OK;   
    }

    i = 0;
    arg_count = 0;
    while (buf[i])
    {
        if (arg_array[arg_count].key == NULL)
        {
            arg_array[arg_count].key = &buf[i];
        }

        if (buf[i] == '&')
        {
            buf[i] = '\0';
            ESP_LOGI(TAG, "{%s, %s}", arg_array[arg_count].key, (arg_array[arg_count].value == NULL)?"NULL":arg_array[arg_count].value);
            arg_count++;
        }

        if (buf[i] == '=')
        {
            buf[i] = '\0';
            if ((arg_array[arg_count].value == NULL) && (buf[i + 1] != '&'))
            {
                arg_array[arg_count].value  = &buf[i + 1];
            }
        }
        i++;
    }
     ESP_LOGI(TAG, "{%s, %s}", arg_array[arg_count].key, (arg_array[arg_count].value == NULL)?"NULL":arg_array[arg_count].value);

    arg_count++;

    ctx->cmd_cb(req, arg_array, arg_count);
    free(buf);
    free(arg_array);
    return ESP_OK;
}

static void httpd_free_res(httpd_back_internal_t *ctx)
{
    free(ctx->cmd.uri);
    free(ctx);
}

void httpd_send_answ(void *req_ptr, const char *str, uint32_t len)
{
    esp_err_t res;
    httpd_req_t *req = NULL;

    if (req_ptr == NULL)
    {
        ESP_LOGW(TAG, "httpd_send_answ: Wrong argument");
        return;
    }

    req = (httpd_req_t *)req_ptr;

    res = httpd_resp_send(req, str, len);
    if (res != ESP_OK)
    {
        ESP_LOGW(TAG, "httpd_resp_send failed %d", res);
    }
}

void *httpd_start_webserver(cmd_cb_t arg_cmd_cb)
{
    httpd_back_internal_t *ctx = NULL;
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (arg_cmd_cb == NULL)
    {
        ESP_LOGE(TAG, "arg_cmd_cb == NULL");
        return NULL;
    }

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        esp_err_t res;
        
        ctx = calloc(1, sizeof(httpd_back_internal_t));
        if (ctx == NULL)
        {
            ESP_LOGE(TAG, "No mem!!!");
            httpd_stop(server);
            return NULL;
        }

        ctx->cmd_cb = arg_cmd_cb;
        ctx->server = server;
        ctx->cmd.uri = strdup("/cmd");
        ctx->cmd.method = HTTP_GET;
        ctx->cmd.handler = cmd_handler;
        ctx->cmd.user_ctx = ctx;
        ESP_LOGI(TAG, "Registering URI handlers");

        res = httpd_register_uri_handler(ctx->server, &ctx->cmd);
        if (res != ESP_OK)
        {
            ESP_LOGE(TAG, "httpd_register_uri_handler failed %d", res);
            httpd_free_res(ctx);
            ctx = NULL;
        }

        return ctx;
    }

    ESP_LOGI(TAG, "Error starting server!");

    return NULL;
}

void httpd_stop_webserver(void *server_ptr)
{
    httpd_back_internal_t *ctx = (httpd_back_internal_t *)server_ptr;
    if (ctx == NULL)
    {
        ESP_LOGE(TAG, "httpd_stop_webserver NULL argument");
        return;
    }

    ESP_LOGI(TAG, "Stopping web server...");
    httpd_stop(ctx->server);
    httpd_free_res(ctx);
}
