#ifndef CONFIG_H
#define CONFIG_H

#include <esp_err.h>
#include "telegram.h"

void config_save_vars(void);
esp_err_t config_load_vars(void);

uint8_t *config_get_vars_mem(uint32_t *total_size);
void config_load_vars_mem(uint8_t *src, size_t total_size);

#endif