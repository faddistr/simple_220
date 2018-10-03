#include <esp_log.h>
#include <driver/gpio.h>
#include "plug.h"

#define PLUG_KEYS_DEF_VAL 1U

#define GPIO_KEY_0 12
#define GPIO_KEY_1 14
#define GPIO_KEY_2 26
#define GPIO_KEY_3 27

#define KEY_GPIO_OUTPUT_PIN_SEL ((1ULL<<GPIO_KEY_0) | (1ULL<<GPIO_KEY_1) | (1ULL<<GPIO_KEY_2) | (1ULL<<GPIO_KEY_3))

static const char *TAG="plug";
static gpio_num_t plug_keys[] = {GPIO_KEY_0, GPIO_KEY_1, GPIO_KEY_2, GPIO_KEY_3}; 


static void init_keys(void);

void plug_init(void)
{
    ESP_LOGI(TAG, "Starting plug manager...");
    init_keys();
}

esp_err_t plug_set_key(plug_key_t key_num, bool val)
{
    ESP_LOGI(TAG, "Set key: %u val: %d", key_num, val);
    if (key_num >= PLUG_KEY_NUM)
    {
        ESP_LOGW(TAG, "Wrong key number!");
    	return ESP_FAIL;	
    }

	return gpio_set_level(plug_keys[key_num], !val);
}

void plug_deinit(void)
{
    ESP_LOGI(TAG, "Stopping plug manager...");
}

static void init_keys(void)
{
	uint32_t i;

	gpio_config_t io_conf = {
	    .intr_type = GPIO_PIN_INTR_DISABLE,
	    .mode = GPIO_MODE_OUTPUT,
	    .pin_bit_mask = KEY_GPIO_OUTPUT_PIN_SEL,
	    .pull_down_en = 0,
	    .pull_up_en = 0,
	};

    gpio_config(&io_conf);

    for (i = 0; i < PLUG_KEY_NUM; i++)
    {
    	gpio_set_level(plug_keys[i], PLUG_KEYS_DEF_VAL);
    }
}
