#include <freertos/FreeRTOS.h>
#include <freertos/tasks.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <string.h>
#include "module.h"
#include "ram_var_stor.h"
#include "config.h"
#include "cmd_executor.h"
#include "telegram_events.h"

#define TELEGRAM_MAX_CONFIG_SIZE (4096U)
#define TELEGRAM_MAX_FW_SIZE (2U * 1024U * 1024U)

static const char *TAG="telegram_ldr";

typedef enum
{
    TELEGRAM_CONFIG,
    TELEGRAM_FW,
} telegram_file_t;

typedef struct
{
    esp_ota_handle_t hnd;
    const esp_partition_t *partition;   
} fw_load_ctx_t;

typedef struct
{
    telegram_file_t file_type;
    bool income;
    uint8_t *buf;
    telegram_int_t chat_id;
    uint32_t total_size;
    uint32_t counter;
    void *additional_ctx;
    char *file_id;
} ioFile_helper_t;

static uint32_t send_config(ioFile_helper_t *hlp, telegram_write_data_evt_t *hnd, void *teleCtx_ptr)
{
    uint32_t part_size = hnd->pice_size;

    if (part_size > hlp->total_size)
    {
        part_size = hlp->total_size;
    }

    memcpy(hnd->buf, &hlp->buf[hlp->counter], part_size);

    hlp->total_size -= part_size;
    hlp->counter += part_size;
    return part_size;
}

static uint32_t send_file_prc(ioFile_helper_t *hlp, telegram_write_data_evt_t *hnd, void *teleCtx_ptr)
{
    if (hlp->income)
    {
        ESP_LOGE(TAG, "Wrong state!(1)");
        return 0;
    }

    switch (hlp->file_type)
    {
        case TELEGRAM_CONFIG:
            return send_config(hlp, hnd, teleCtx_ptr);
        
        default:
            ESP_LOGE(TAG, "Wrong state!(2)");
            break;
    }

    return 0;
}

static void send_file_response(ioFile_helper_t *hlp, telegram_update_t *update, void *teleCtx_ptr)
{
    telegram_chat_message_t *msg = telegram_get_message(update);
    
    if (hlp->income)
    {
        ESP_LOGE(TAG, "Wrong state!(3)");
        return;
    }

    if (msg && (msg->file))
    {
        ESP_LOGI(TAG, "file_id: %s", msg->file->id);
        telegram_send_text_message(teleCtx_ptr, hlp->chat_id,  msg->file->id);
    }
}

static uint32_t receive_config_prc(ioFile_helper_t *hlp, telegram_write_data_evt_t *hnd, void *teleCtx_ptr)
{
    if (hnd->pice_size > hlp->total_size)
    {
        ESP_LOGE(TAG, "Too big packet!");
        return 0;
    }

    memcpy(&hlp->buf[hlp->counter], hnd->buf, hnd->pice_size);
    hlp->counter += hnd->pice_size;    

    return hnd->pice_size;
}

static uint32_t receive_fw_prc(ioFile_helper_t *hlp, telegram_write_data_evt_t *hnd, void *teleCtx_ptr)
{
    fw_load_ctx_t *ota = (fw_load_ctx_t *)hlp->additional_ctx;
    esp_err_t err = esp_ota_write(ota->hnd, 
        (const void *)hnd->buf, hnd->pice_size);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA failed! %d", err);
        return 0;
    }

    return hnd->pice_size;
}


static void send_err_user(void *teleCtx_ptr, telegram_int_t chat_id, const char *err_descr, esp_err_t err_code)
{
    char tmp[32];

    strncpy(tmp, err_descr, 23);
    sprintf(&tmp[strlen(tmp)], " %X", err_code);
    telegram_send_text_message(teleCtx_ptr, chat_id, tmp);
}

static uint32_t load_file_prc(ioFile_helper_t *hlp, telegram_write_data_evt_t *hnd, void *teleCtx_ptr)
{
    if (!hlp->income)
    {
        ESP_LOGE(TAG, "Wrong state!(4)");
        return 0    ;
    }

    switch (hlp->file_type)
    {
        case TELEGRAM_CONFIG:
            return receive_config_prc(hlp, hnd, teleCtx_ptr);

        case TELEGRAM_FW:
            return receive_fw_prc(hlp, hnd, teleCtx_ptr);
        
        default:
            ESP_LOGE(TAG, "Wrong state!(5)");
            break;
    }

    return 0;
}

static void receive_config_fin(ioFile_helper_t *hlp, void *teleCtx_ptr)
{
    if (hlp->counter > 0)
    {
        char *file_id_current = var_get("TELEGRAM_CONFIG_CURRENT");
        config_load_vars_mem(hlp->buf, hlp->counter);
        telegram_send_text_message(teleCtx_ptr, hlp->chat_id, "Config loaded!");

        if (file_id_current)
        {
            var_add_attr("TELEGRAM_CONFIG_PREVIOUS", file_id_current, true);
            free(file_id_current);
        }

        var_add_attr("TELEGRAM_CONFIG_CURRENT", hlp->file_id, true);
    }
}

static void receive_fw_fin(ioFile_helper_t *hlp, void *teleCtx_ptr)
{
    fw_load_ctx_t *ota = (fw_load_ctx_t *)hlp->additional_ctx;
    esp_err_t err;

    if (ota == NULL)
    {
        return;
    }

    do
    {
        err = esp_ota_end(ota->hnd);
        if (err != ESP_OK)
        {
            break;
        }

        err = esp_ota_set_boot_partition(ota->partition);
        if (err != ESP_OK)
        {
            break;
        }
    } while(0);


    free(ota);
    if (err == ESP_OK)
    {
        char *file_id_current = var_get("TELEGRAM_FW_CURRENT");

        if (file_id_current)
        {
             var_add_attr("TELEGRAM_FW_PREVIOUS", file_id_current, true);
             free(file_id_current);
        }

        var_add_attr("TELEGRAM_FW_CURRENT", hlp->file_id, true);
        config_save_vars();

        telegram_send_text_message(teleCtx_ptr, hlp->chat_id, "FW loaded!");
        esp_restart();
    } else
    {
        send_err_user(teleCtx_ptr, hlp->chat_id, "Load new FW failed:", err);
    }
}

static void load_file_fin(ioFile_helper_t *hlp, void *teleCtx_ptr)
{
    switch (hlp->file_type)
    {
        case TELEGRAM_CONFIG:
            receive_config_fin(hlp, teleCtx_ptr);
            break;

        case TELEGRAM_FW:
            receive_fw_fin(hlp, teleCtx_ptr);
            break;
        
        default:
            ESP_LOGE(TAG, "Wrong state!(6)");
            break;
    }
}

static void send_file_fin(ioFile_helper_t *hlp, void *teleCtx_ptr)
{
    switch (hlp->file_type)
    {
        case TELEGRAM_CONFIG:
            break;

        default:
            ESP_LOGE(TAG, "Wrong state!(7)");
            break;
    }   
}

static void io_file_fin(ioFile_helper_t *hlp, void *teleCtx_ptr)
{
    if (hlp->income)
    {
        load_file_fin(hlp, teleCtx_ptr);
    } else
    {
        send_file_fin(hlp, teleCtx_ptr);
    }
}

static uint32_t load_file_cb(telegram_data_event_t evt, void *teleCtx_ptr, void *ctx, void *evt_data)
{
    ioFile_helper_t *hlp = (ioFile_helper_t *)ctx;
    
    switch (evt)
    {
        case TELEGRAM_READ_DATA:   
            ESP_LOGI(TAG, "TELEGRAM_READ_DATA");
            return send_file_prc(hlp, (telegram_write_data_evt_t *)evt_data, teleCtx_ptr);

        case TELEGRAM_RESPONSE:
            ESP_LOGI(TAG, "TELEGRAM_RESPONSE");
            send_file_response(hlp, (telegram_update_t *)evt_data, teleCtx_ptr);
            break; 

        case TELEGRAM_WRITE_DATA:
            ESP_LOGI(TAG, "TELEGRAM_WRITE_DATA");
            return load_file_prc(hlp, (telegram_write_data_evt_t *)evt_data, teleCtx_ptr);

        case TELEGRAM_ERR:
            ESP_LOGE(TAG, "TELEGRAM_ERR");
            break;

        case TELEGRAM_END:
            ESP_LOGI(TAG, "TELEGRAM_END");
            io_file_fin(hlp, teleCtx_ptr);
            free(hlp->file_id);
            free(hlp->buf);
            free(hlp);
            break;

        default:
            break;
    }

    return 0;
}

static ioFile_helper_t *receive_config(telegram_event_file_t *evt)
{
    ioFile_helper_t *hlp = NULL;
    if (evt->file_size > TELEGRAM_MAX_CONFIG_SIZE)
    {
        return NULL;
    }

    hlp = (ioFile_helper_t *)calloc(sizeof(ioFile_helper_t), 1);
    if (hlp == NULL)
    {
        return NULL;
    }

    hlp->buf = (uint8_t *)calloc(sizeof(uint8_t), evt->file_size);
    if (!hlp->buf)
    {
        free(hlp);
        return NULL;
    }

    hlp->file_type = TELEGRAM_CONFIG;


    return hlp;
}

static ioFile_helper_t *receive_fw(telegram_event_file_t *evt)
{
    ioFile_helper_t *hnd = NULL;
    esp_err_t err;
    fw_load_ctx_t *ota;

    if (evt->file_size > TELEGRAM_MAX_FW_SIZE)
    {
        ESP_LOGW(TAG, "total_size > TELEGRAM_MAX_FW_SIZE");
        return NULL;
    }

    ESP_ERROR_CHECK(esp_event_post_to(simple_loop_handle, MODULE_BASE, MODULE_EVENT_OTA, 
        &evt->file_size, sizeof(evt->file_size), portMAX_DELAY));

    vTaskDelay(1000);


    hnd = (ioFile_helper_t *)calloc(sizeof(ioFile_helper_t), 1);
    if (hnd == NULL)
    {
        ESP_LOGE(TAG, "No mem!");
        return NULL;
    }

    hnd->file_type = TELEGRAM_FW;

    hnd->additional_ctx = calloc(sizeof(fw_load_ctx_t), 1);
    if (!hnd->additional_ctx)
    {
        free(hnd);
        ESP_LOGE(TAG, "No mem!");
        return NULL;
    }

    ESP_LOGI(TAG, "Starting OTA...");
    ota = (fw_load_ctx_t *)hnd->additional_ctx;
    do
    {
        ota->partition = esp_ota_get_next_update_partition(NULL);
        if (ota->partition == NULL)
        {
            ESP_LOGE(TAG, "Passive OTA partition not found");
            err = ESP_ERR_NOT_FOUND;
            break;
        }

        err = esp_ota_begin(ota->partition, OTA_SIZE_UNKNOWN, &ota->hnd);
        if (err != ESP_OK)
        {
            break;
        }

    } while(0);

    if (err != ESP_OK)
    {
        free(ota);
        hnd->additional_ctx = NULL;
        free(hnd);
        hnd = NULL;
        send_err_user(evt->ctx, evt->chat_id, "Failed to start OTA:", err);
    }


    return hnd;
}


static void receive_file(telegram_event_file_t * evt)
{
    ioFile_helper_t *hlp = NULL;
    char *file_id = (char *)&evt->blob[evt->id_str_offset];
    char *caption = (char *)&evt->blob[evt->caption_str_offset];

    if (*caption == '\0')
    {
        return;
    }

    if (!strcmp(caption, "config"))
    {
        hlp = receive_config(evt);
    }

    if (!strcmp(caption, "fw"))
    {
        hlp = receive_fw(evt);
    }

    if (hlp)
    {
        hlp->income = true;
        hlp->chat_id = evt->chat_id;
        hlp->total_size = evt->file_size;
        hlp->file_id = strdup(file_id);
        telegram_get_file(evt->ctx, file_id, hlp, load_file_cb);
    }
}

static void telegram_event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	switch (event_id)
	{
    	case TELEGRAM_STOP:
    		break;

    	case TELEGRAM_FILE:
            receive_file((telegram_event_file_t *)event_data);
    		break;

    	default:
    		break;
	}
}


static void cmd_configfile(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
    telegram_event_msg_t *evt = (telegram_event_msg_t *)info->cmd_data;
    ioFile_helper_t *ctx = calloc(sizeof(ioFile_helper_t), 1);

    if (ctx == NULL)
    {
        ESP_LOGE(TAG, "No mem!");
        return;
    }

    ctx->chat_id = evt->chat_id;
    ctx->buf = config_get_vars_mem(&ctx->total_size);

    if ((ctx->buf == NULL) || (ctx->total_size == 0))
    {
        ESP_LOGE(TAG, "Failed to get config");
        return;
    }

    telegram_send_file(info->arg, evt->chat_id, "config", "config.txt", ctx->total_size, ctx, load_file_cb);
}

static void cmd_getpath(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
    telegram_event_msg_t *evt = (telegram_event_msg_t *)info->cmd_data;
    char *file_id = (char *)&evt->text[strlen(cmd_name)];

    if (*file_id == '\0')
    {
         telegram_send_text_message(info->arg, evt->chat_id, "Wrong args");
         return;
    } else
    {
        char *file = telegram_get_file_path(info->arg, file_id + 1);
        if (file == NULL)
        {
            telegram_send_text_message(info->arg, evt->chat_id, "file_id not found");
            return;
        }

        telegram_send_text_message(info->arg, evt->chat_id, file);
        free(file);
    }

}

static uint32_t send_testfile_cb(telegram_data_event_t evt, void *teleCtx_ptr, void *ctx, void *evt_data)
{
    telegram_int_t *chat_id = (telegram_int_t *)ctx;
    telegram_write_data_evt_t *hnd = (telegram_write_data_evt_t *)evt_data;
    telegram_update_t *update = (telegram_update_t *)evt_data;

    switch (evt)
    {
        case TELEGRAM_READ_DATA:
            {
                ESP_LOGI(TAG, "TELEGRAM_READ_DATA");
                ESP_LOGE(TAG, "MSIZE:%d", hnd->pice_size);
                memset(hnd->buf, 'B', hnd->pice_size);
                sprintf((char *)hnd->buf, "Hello world\n");
                hnd->buf[strlen((char *)hnd->buf)] = (uint8_t)'A';

                return hnd->pice_size;
            }
            break;

        case TELEGRAM_RESPONSE:
            {
                telegram_chat_message_t *msg = telegram_get_message(update);
                ESP_LOGI(TAG, "TELEGRAM_RESPONSE");

                if (msg && (msg->file))
                {
                    ESP_LOGI(TAG, "file_id: %s %lf", msg->file->id, *chat_id);
                    telegram_send_text_message(teleCtx_ptr, *chat_id,  msg->file->id);
                }
            }
            break; 

        case TELEGRAM_WRITE_DATA:
            ESP_LOGI(TAG, "TELEGRAM_WRITE_DATA");
            break;

        case TELEGRAM_ERR:
            ESP_LOGE(TAG, "TELEGRAM_ERR");
            break;

        case TELEGRAM_END:
            ESP_LOGI(TAG, "TELEGRAM_END");
            free(ctx);
            break;

        default:
            break;
    }

    return 0;
}

static void cmd_testfile(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
    telegram_event_msg_t *evt = (telegram_event_msg_t *)info->cmd_data;
    telegram_int_t *chat_id = calloc(1, sizeof(telegram_int_t));

    *chat_id = evt->chat_id;
    telegram_send_file(info->arg, evt->chat_id, "test", "test.txt", 256, chat_id, send_testfile_cb);
}


static void telegram_ota_init(void)
{
    ESP_LOGI(TAG, "Module started...");
    ESP_ERROR_CHECK(esp_event_handler_register_with(simple_loop_handle, TELEGRAM_BASE, ESP_EVENT_ANY_ID, telegram_event_handler, NULL));
}

module_init(telegram_ota_init);
cmd_register_static({
    {
        .name = "testfile",
        .cmd_cb = cmd_testfile,
    },
    {
        .name = "getconfig",
        .cmd_cb = cmd_configfile,
    },
    {
        .name = "getpath",
        .cmd_cb = cmd_getpath,
    },
});