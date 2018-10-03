#ifndef HTTPD_BACK_H
#define HTTPD_BACK_H
#include <http_server.h>

httpd_handle_t httpd_start_webserver(void);
void httpd_stop_webserver(httpd_handle_t server);

#endif