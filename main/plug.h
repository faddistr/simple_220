#ifndef PLUG_H
#define PLUG_H
#include <esp_err.h>
#include <stdint.h>
#include <stdbool.h>


typedef enum
{
	PLUG_KEY_0,
	PLUG_KEY_1,
	PLUG_KEY_2,
	PLUG_KEY_3,
	PLUG_KEY_NUM
} plug_key_t;

void plug_init(void);
esp_err_t plug_set_key(plug_key_t key_num, bool val);
void plug_deinit(void);

#endif