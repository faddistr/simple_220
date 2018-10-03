#ifndef CONFIG_H
#define CONFIG_H

#include <esp_err.h>

#define MAX_CONFIG_SSID 64U
#define MAX_CONFIG_PASSWORD 64U


typedef struct 
{
	char ssid[MAX_CONFIG_SSID + 1];
	char password[MAX_CONFIG_PASSWORD + 1];
} config_t;


esp_err_t config_load(config_t *config);
void config_save(config_t *config);
#endif