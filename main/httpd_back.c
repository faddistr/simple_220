#include <esp_log.h>
#include "httpd_back.h"
#include "plug.h"

static const char *TAG="httpd_back";

static char *user_pass = NULL;
static httpd_change_password_cb_t chg_pass_cb;

const char *key_set_ok_str = "{\r\n\t\"stat\":\"ok\"\r\n}";
const char *key_set_fail_str = "{\r\n\t\"stat\":\"fail\"\r\n}";

static esp_err_t plug_get_handler(httpd_req_t *req);
static esp_err_t ctrl_get_handler(httpd_req_t *req);

static httpd_uri_t plug = {
    .uri       = "/plug",
    .method    = HTTP_GET,
    .handler   = plug_get_handler,
};


static httpd_uri_t ctrl = {
    .uri       = "/ctrl",
    .method    = HTTP_GET,
    .handler   = ctrl_get_handler,
};

static esp_err_t plug_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    esp_err_t res = ESP_FAIL;

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            do
            {
                char key[2];
                char stat[2];
                uint32_t key_int;
                uint32_t stat_int;

                ESP_LOGI(TAG, "Found URL query => %s", buf);
                if (0 != strlen(user_pass))
                {
                    char password[HTTPD_MAX_PASSWORD + 1];

                    if ((httpd_query_key_value(buf, "password", password, sizeof(password)) != ESP_OK)
                        || (0 != strncmp(user_pass, password, HTTPD_MAX_PASSWORD)))
                    {
                        ESP_LOGW(TAG, "Auth failed...");
                        break;
                    }
                }

                if ((httpd_query_key_value(buf, "key", key, sizeof(key)) != ESP_OK)
                    || (httpd_query_key_value(buf, "stat", stat, sizeof(stat)) != ESP_OK))
                {
                    ESP_LOGW(TAG, "Param missed!");
                    break;
                } 

                key_int = (key[0] - 0x30); /* convert to int */
                if (key_int > 9)
                {
                    ESP_LOGW(TAG, "key not int!");
                    break;
                }

                stat_int = (stat[0] - 0x30);
                if (stat_int > 9)
                {
                    ESP_LOGW(TAG, "stat not int!");
                    break;
                }

                res = plug_set_key(key_int, !!stat_int);
            } while (0);
        }
        free(buf);
    }


    if (res == ESP_OK)
    {
        httpd_resp_send(req, key_set_ok_str, strlen(key_set_ok_str));
    } else
    {
        httpd_resp_send(req, key_set_fail_str, strlen(key_set_fail_str));
    }

    return ESP_OK;
}

static esp_err_t ctrl_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    esp_err_t res = ESP_FAIL;

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            do
            {
                char param[HTTPD_MAX_PASSWORD + 1];
                ESP_LOGI(TAG, "Found URL query => %s", buf);
                if (0 != strlen(user_pass))
                {
                    char password[HTTPD_MAX_PASSWORD + 1];

                    if ((httpd_query_key_value(buf, "password", password, sizeof(password)) != ESP_OK)
                        || (0 != strncmp(user_pass, password, HTTPD_MAX_PASSWORD)))
                    {
                        ESP_LOGW(TAG, "Auth failed...");
                        break;
                    }
                }

                if (httpd_query_key_value(buf, "new_pass", param, sizeof(param)) == ESP_OK)
                {
                    free(user_pass);
                    user_pass = strdup(param);
                    if (chg_pass_cb)
                    {
                        chg_pass_cb(param);
                    }
                    res = ESP_OK;
                } 
            } while (0);
        }
        free(buf);
    }


    if (res == ESP_OK)
    {
        httpd_resp_send(req, key_set_ok_str, strlen(key_set_ok_str));
    } else
    {
        httpd_resp_send(req, key_set_fail_str, strlen(key_set_fail_str));
    }

    return ESP_OK;
}


httpd_handle_t httpd_start_webserver(char *password, httpd_change_password_cb_t change_pass_cb)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        user_pass = strdup(password);
        chg_pass_cb = change_pass_cb;
        plug_init();
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &plug);
        httpd_register_uri_handler(server, &ctrl);

        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");

    return NULL;
}

void httpd_stop_webserver(httpd_handle_t server)
{
    ESP_LOGI(TAG, "Stopping web server...");
    free(user_pass);
    user_pass = NULL;
    chg_pass_cb = NULL;
    plug_deinit();
    httpd_stop(server);
}