#ifndef HTTPD_BACK_H
#define HTTPD_BACK_H
#include <esp_http_server.h>
#define HTTPD_MAX_PASSWORD 16U

typedef void (*httpd_change_password_cb_t)(char *new_pass);

httpd_handle_t httpd_start_webserver(char *password, httpd_change_password_cb_t change_pass_cb);
void httpd_stop_webserver(httpd_handle_t server);

#endif