#ifndef JPG_STREAMER_H
#define JPG_STREAMER_H
#include <esp_http_server.h>

httpd_handle_t mjpg_start_webserver(void);
void mjpg_stop_webserver(httpd_handle_t server);


#endif