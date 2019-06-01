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
static bool key_values[PLUG_KEY_NUM] = {};

#define PLUG_NAME_TMP "PLUG_KEY_"

static esp_err_t plug_set_key(plug_key_t key_num, bool value)
{
    char name[32];

    ESP_LOGI(TAG, "Set key: %u value: %d", key_num, value);
    if (key_num >= PLUG_KEY_NUM)
    {
        ESP_LOGW(TAG, "Wrong key number!");
    	return ESP_FAIL;	
    }

    sprintf(name, PLUG_NAME_TMP"%d", (uint32_t)key_num);
    var_add_attr(name, value?"1":"0", true);
    key_values[key_num] = value;

	return gpio_set_level(plug_keys[key_num], !value);
}

static bool cmd_plug(void *args)
{
    uint32_t key = 0;
    uint32_t value = 0;
    char *save_ptr = NULL;
    bool is_key = false;
    bool is_value = false;
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

            is_value = (sscanf(token, "%d", &value) == 1);
        }

        if (is_key && is_value)
        {
            break;
        }
        token = strtok_r(NULL, " ", &save_ptr);
    } while (token != NULL);

    if (is_key && is_value)
    {
        plug_set_key(key, !!value);
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
    static telegram_kbrd_t keyboard = 
    {
        .type = TELEGRAM_KBRD_MARKUP_REMOVE,
        .kbrd = {
            .markup_remove = {false},
        },
    };
    
    telegram_event_msg_t *evt = ((telegram_event_msg_t *)info->cmd_data);

    if (cmd_plug(evt->text))
    {
        telegram_kbrd(evt->ctx, evt->chat_id, "OK", &keyboard);
    } else
    {
        telegram_kbrd(evt->ctx, evt->chat_id, "FAIL", &keyboard);
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


static void cmd_plug_kbrd(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
    telegram_event_msg_t *evt = ((telegram_event_msg_t *)info->cmd_data);
    static telegram_kbrd_inline_btn_t row0[] = {{.text = "0", .callback_data = "key 0"}, {.text = "1", .callback_data = "key 1"}, {NULL}};
    static telegram_kbrd_inline_btn_t row1[] = {{.text = "2", .callback_data = "key 2"}, {.text = "3", .callback_data = "key 3"}, {NULL}};
    
    static telegram_kbrd_inline_row_t kbrd_btns[] =
    {
        { row0, },
        { row1, },
        { NULL },
    };

    static telegram_kbrd_t keyboard = 
    {
        .type = TELEGRAM_KBRD_INLINE,
        .kbrd = {
            .inl.rows = kbrd_btns,
        },
    };

    if (info->transport != CMD_SRC_TELEGRAM)
    {
        return;
    }

    
    telegram_kbrd(evt->ctx, evt->chat_id, "Select plug to activate", &keyboard);
}

static void cmd_plug_kbrd_markup(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
    telegram_event_msg_t *evt = ((telegram_event_msg_t *)info->cmd_data);
    static telegram_kbrd_btn_t row0_m[] = {{.text = "plug key 0 value 1", }, {.text = "plug key 1 value 1", }, {NULL}};
    static telegram_kbrd_btn_t row1_m[] = {{.text = "plug key 2 value 1", }, {.text = "plug key 2 value 1", }, {NULL}};
    
    static telegram_kbrd_markup_row_t kbrd_btns_markup[] =
    {
        {row0_m, },
        {row1_m, },
        {NULL},
    };

    static telegram_kbrd_t keyboard_m = 
    {
        .type = TELEGRAM_KBRD_MARKUP,
        .kbrd = {
            .markup = {.rows = kbrd_btns_markup, .resize = true, .one_time = true, .selective = false, },
        },
    };



    if (info->transport != CMD_SRC_TELEGRAM)
    {
        return;
    }

    telegram_kbrd(evt->ctx, evt->chat_id, "Select plug to activate", &keyboard_m);
}

static void cmd_plug_kbrd_markup_remove(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
    telegram_event_msg_t *evt = ((telegram_event_msg_t *)info->cmd_data);
    static telegram_kbrd_btn_t row0_m[] = {{.text = "plug key 0 value 1", }, {.text = "plug key 1 value 1", }, {NULL}};
    static telegram_kbrd_btn_t row1_m[] = {{.text = "plug key 2 value 1", }, {.text = "plug key 2 value 1", }, {NULL}};
    
    static telegram_kbrd_markup_row_t kbrd_btns_markup[] =
    {
        {row0_m, },
        {row1_m, },
        {NULL},
    };

    static telegram_kbrd_t keyboard_m = 
    {
        .type = TELEGRAM_KBRD_MARKUP,
        .kbrd = {
            .markup = {.rows = kbrd_btns_markup, .resize = true, .one_time = false, .selective = false, },
        },
    };

    if (info->transport != CMD_SRC_TELEGRAM)
    {
        return;
    }

    telegram_kbrd(evt->ctx, evt->chat_id, "Select plug to activate", &keyboard_m);
}

static void cmd_plug_kbrd_force_reply(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
    telegram_event_msg_t *evt = ((telegram_event_msg_t *)info->cmd_data);
    static telegram_kbrd_t keyboard = 
    {
        .type = TELEGRAM_KBRD_FORCE_REPLY,
        .kbrd = {
            .force_reply = {false},
        },
    };


    if (info->transport != CMD_SRC_TELEGRAM)
    {
        return;
    }

    telegram_kbrd(evt->ctx, evt->chat_id, "Select plug to activate", &keyboard);
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
                plug_set_key(i, true);
            }

            if (*cfg == '0')
            {
                plug_set_key(i, false);
            }

            free(cfg);
        }
    }
}

static void telegram_event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    uint32_t key_num;

    telegram_event_cb_query_t *query = (telegram_event_cb_query_t *)event_data;
    if ((sscanf(&query->blob[query->data_str_offset], "key %d", &key_num) == 1) && (key_num < PLUG_KEY_NUM))
    {
        plug_set_key(key_num, !key_values[key_num]);
        telegram_answer_cb_query(query->ctx, &query->blob[query->id_str_offset], key_values[key_num]?"on":"off",
            true, NULL, 0);
    }
}

static void plug_init(void)
{
    ESP_LOGI(TAG, "Starting plug manager...");
    init_keys();
    load_config();
    ESP_ERROR_CHECK(esp_event_handler_register_with(simple_loop_handle, TELEGRAM_BASE, 
        TELEGRAM_CBQUERY, telegram_event_handler, NULL));
}

cmd_register_static({
{
    .name = "plug",
    .cmd_cb = cmd_plug_cb,
},
{
    .name = "p",
    .cmd_cb = cmd_plug_kbrd,
},
{
    .name = "m",
    .cmd_cb = cmd_plug_kbrd_markup,
},
{
    .name = "mr",
    .cmd_cb = cmd_plug_kbrd_markup_remove,
},
{
    .name = "f",
    .cmd_cb = cmd_plug_kbrd_force_reply,
},
});

module_init(plug_init);