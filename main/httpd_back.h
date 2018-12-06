#ifndef HTTPD_BACK_H
#define HTTPD_BACK_H
#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    const char *key;
    const char *value;
} httpd_arg_t;

typedef void(* cmd_cb_t)(void *ctx, httpd_arg_t *argv, uint32_t argc, void *user_sess);

void httpd_send_answ(void *req_ptr, const char *str, uint32_t len);
void httpd_set_sess(void *req_ptr, void *sess);
void *httpd_start_webserver(cmd_cb_t cmd_cb);
void httpd_stop_webserver(void *server_ptr);

#endif