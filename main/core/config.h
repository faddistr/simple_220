#ifndef CONFIG_H
#define CONFIG_H

#include <esp_err.h>
#include "telegram.h"
#define MAX_CONFIG_SSID 32U
#define MAX_CONFIG_PASSWORD 16U


typedef struct 
{
	char ssid[MAX_CONFIG_SSID + 1];
	char password[MAX_CONFIG_PASSWORD + 1];
	char user_pass[MAX_CONFIG_PASSWORD + 1];
	char telegram_token[TELEGRAM_MAX_TOKEN_LEN + 1];
} config_t;


esp_err_t config_load(config_t *config);
void config_save(config_t *config);
void config_save_vars(void);
esp_err_t config_load_vars(void);

uint8_t *config_get_vars_mem(uint32_t *total_size);
void config_load_vars_mem(uint8_t *src, size_t total_size);

#endif