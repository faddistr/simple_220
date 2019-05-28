#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <esp_log.h>
#include <esp_err.h>
#include <driver/gpio.h>
#include "ram_var_stor.h"
#include "module.h"
#include "telegram_events.h"
#include "httpd_back.h"
#include "cmd_executor.h"

#define GPIO_KEY_0 13
#define GPIO_KEY_1 12
#define GPIO_KEY_2 14
#define GPIO_KEY_3 27

typedef enum
{
    PLUG_KEY_0,
    PLUG_KEY_1,
    PLUG_KEY_2,
    PLUG_KEY_3,
    PLUG_KEY_NUM
} plug_key_t;

#define KEY_GPIO_OUTPUT_PIN_SEL ((1ULL<<GPIO_KEY_0) | (1ULL<<GPIO_KEY_1) | (1ULL<<GPIO_KEY_2) | (1ULL<<GPIO_KEY_3))

static const char *TAG="plug";
static gpio_num_t plug_keys[] = {GPIO_KEY_0, GPIO_KEY_1, GPIO_KEY_2, GPIO_KEY_3}; 

#define PLUG_NAME_TMP "PLUG_KEY_"

static esp_err_t plug_set_key(plug_key_t key_num, bool val)
{
    char name[32];

    ESP_LOGI(TAG, "Set key: %u val: %d", key_num, val);
    if (key_num >= PLUG_KEY_NUM)
    {
        ESP_LOGW(TAG, "Wrong key number!");
    	return ESP_FAIL;	
    }

    sprintf(name, PLUG_NAME_TMP"%d", (uint32_t)key_num);
    var_add_attr(name, val?"0":"1", true);

	return gpio_set_level(plug_keys[key_num], !val);
}

static bool cmd_plug(void *args)
{
    uint32_t key = 0;
    uint32_t val = 0;
    char *save_ptr = NULL;
    bool is_key = false;
    bool is_val = false;
    char *token = strtok_r((char *)args, " ", &save_ptr);

    if (token == NULL)
    {
        return false;
    }

    do
    {
        if (!strcmp(token, "key"))
        {
            token = strtok_r(NULL, " ", &save_ptr);
            if (token == NULL)
            {
                break;
            }

            is_key = (sscanf(token, "%d", &key) == 1);
        }

        if (!strcmp(token, "value"))
        {
            token = strtok_r(NULL, " ", &save_ptr);
            if (token == NULL)
            {
                break;
            }

            is_val = (sscanf(token, "%d", &val) == 1);
        }

        if (is_key && is_val)
        {
            break;
        }
        token = strtok_r(NULL, " ", &save_ptr);
    } while (token != NULL);

    if (is_key && is_val)
    {
        plug_set_key(key, !!val);
        return true;
    } 
    
    return false;
}

static void cmd_plug_cb_httpb(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
    if (cmd_plug(info->cmd_data))
    {
        httpd_send_answ(info->arg, "OK", 2);
    } else
    {
        httpd_send_answ(info->arg, "FAIL", 4);
    }
}

static void cmd_plug_cb_telegram(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
    telegram_event_msg_t *evt = ((telegram_event_msg_t *)info->cmd_data);

    if (cmd_plug(evt->text))
    {
        telegram_send_text_message(evt->ctx, evt->chat_id, "OK");
    } else
    {
        telegram_send_text_message(evt->ctx, evt->chat_id, "FAIL");
    }
}

static void cmd_plug_cb(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
    switch (info->transport)
    {
        case CMD_SRC_HTTPB:
            cmd_plug_cb_httpb(cmd_name, info, private);
            break;

        case CMD_SRC_TELEGRAM:
            cmd_plug_cb_telegram(cmd_name, info, private);
            break;

        default:
            break;
    }
}


static void init_keys(void)
{
	gpio_config_t io_conf = {
	    .intr_type = GPIO_PIN_INTR_DISABLE,
	    .mode = GPIO_MODE_OUTPUT,
	    .pin_bit_mask = KEY_GPIO_OUTPUT_PIN_SEL,
	    .pull_down_en = 0,
	    .pull_up_en = 0,
	};

    gpio_config(&io_conf);
}


static void load_config(void)
{
    char *cfg;
    char cfg_name[32];

    for (uint32_t i = 0; i < (sizeof(plug_keys) / sizeof(plug_keys[0])); i++)
    {
        sprintf(cfg_name, PLUG_NAME_TMP"%d", i);
        cfg = var_get(cfg_name);

        if (cfg)
        {
            if (*cfg == '1')
            {
                gpio_set_level(plug_keys[i], 0);
            }

            if (*cfg == '0')
            {
                gpio_set_level(plug_keys[i], 1);
            }

            free(cfg);
        }
    }
}

static void plug_init(void)
{
    ESP_LOGI(TAG, "Starting plug manager...");
    init_keys();
    load_config();
}

cmd_register_static({
{
    .name = "plug",
    .cmd_cb = cmd_plug_cb,
} 
});

module_init(plug_init);