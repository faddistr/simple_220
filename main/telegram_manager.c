#include <string.h>
#include <esp_log.h>
#include <esp_event.h>
#include "telegram.h"
#include "ram_var_stor.h"
#include "telegram_events.h"
#include "tlist.h"
#include "telegram_cclist.h"
#include "cmd_executor.h"
#include "module.h"

static const char *TAG="TELEGRAM_MANAGER";
static void *teleCtx;
static void *adminList;
ESP_EVENT_DEFINE_BASE(TELEGRAM_BASE);

static void telegram_new_message(void *teleCtx, telegram_update_t *info)
{
    uint32_t size_required;
    telegram_chat_message_t *msg = telegram_get_message(info);
    telegram_event_msg_t *event_msg;

    if ((msg == NULL) || (msg->text == NULL))
    {
        return;
    }
    size_required = sizeof(telegram_event_msg_t) + strlen(msg->text) + 1;

    event_msg = calloc(size_required, 1);
    if (event_msg == NULL)
    {
        ESP_LOGE(TAG, "No mem! Required size: %d", size_required);
        return;
    }

    strcpy(event_msg->text, msg->text);
    event_msg->ctx = teleCtx;
    event_msg->chat_id = telegram_get_chat_id(msg);
    event_msg->user_id = telegram_get_user_id(msg);

    if (event_msg->chat_id == -1)
    {
        return;
    }

    ESP_ERROR_CHECK(esp_event_post_to(simple_loop_handle, TELEGRAM_BASE, 
        TELEGRAM_MESSAGE, event_msg, sizeof(telegram_event_msg_t) + strlen(msg->text) + 1, portMAX_DELAY));

    free(event_msg);
}

static void telegram_new_file(void *teleCtx, telegram_update_t *info)
{
    telegram_event_file_t *event_file;
    telegram_chat_message_t *msg = telegram_get_message(info);
    telegram_document_t *file;
    uint32_t offset = 0;
    uint32_t data_size;

    if ((msg == NULL) || (msg->file == NULL))
    {
        return;
    }

    file = msg->file;
    data_size = strlen(file->id) + 1 + strlen(file->name) + 1 + strlen(file->mime_type) + 1 
        + (msg->caption?strlen(msg->caption):0) + 1;
    
    event_file = calloc(data_size + sizeof(telegram_event_file_t), 1);
    if (event_file == NULL)
    {
        ESP_LOGE(TAG, "No mem! Required size: %d", data_size + sizeof(telegram_event_file_t));
        return;
    }

    event_file->ctx = teleCtx;
    event_file->data_size = data_size;
    event_file->file_size = file->file_size;
    event_file->chat_id = telegram_get_chat_id(msg);
    event_file->user_id = telegram_get_user_id(msg);

    event_file->id_str_offset = offset;
    memcpy(&event_file->blob[offset], file->id, strlen(file->id) + 1);
    offset += strlen(file->id) + 1;


    if (file->name)
    {
        event_file->name_str_offset = offset;
        memcpy(&event_file->blob[offset], file->name, strlen(file->name) + 1);
        offset += strlen(file->name) + 1;
    }

    if (file->mime_type)
    {
        event_file->mime_type_str_offset = offset;
        memcpy(&event_file->blob[offset], file->mime_type, strlen(file->mime_type) + 1);
        offset += strlen(file->mime_type) + 1;
    }

    if (msg->caption)
    {
        event_file->caption_str_offset = offset;
        memcpy(&event_file->blob[offset], msg->caption, strlen(msg->caption) + 1);
        offset += strlen(msg->caption) + 1;

        ESP_LOGI(TAG, "caption = %s", msg->caption);
    } else
    {
        event_file->caption_str_offset = offset;  
    }

    ESP_LOGI(TAG, "File: id: %s name: %s type: %s size: %lf chat_id: %lf", 
        file->id, file->name, file->mime_type, file->file_size, event_file->chat_id);

    ESP_ERROR_CHECK(esp_event_post_to(simple_loop_handle, TELEGRAM_BASE, 
        TELEGRAM_FILE, event_file, data_size + sizeof(telegram_event_file_t), portMAX_DELAY));

    free(event_file);
}

static void telegram_new_cbquery(void *teleCtx, telegram_update_t *info)
{
    uint32_t offset = 0;
    uint32_t data_size = 0;
    telegram_event_cb_query_t *query = NULL;
    telegram_chat_callback_t  *cb = (telegram_chat_callback_t *)info->callback_query;

    if (info->callback_query == NULL)
    {        return;
    }

    data_size = sizeof(telegram_event_cb_query_t) + strlen(cb->id) + 1 + strlen(cb->data) + 1;
    query = calloc(1, data_size);
    if (!query)
    {
        ESP_LOGE(TAG, "No mem!");
        return;
    }

    query->ctx = teleCtx;
    query->user_id = cb->from->id;

    query->id_str_offset = offset;
    memcpy(&query->blob[query->id_str_offset], cb->id, strlen(cb->id) + 1);

    offset += strlen(cb->id) + 1;
    query->data_str_offset = offset;
    memcpy(&query->blob[query->data_str_offset], cb->data, strlen(cb->data) + 1);

    ESP_ERROR_CHECK(esp_event_post_to(simple_loop_handle, TELEGRAM_BASE, 
        TELEGRAM_CBQUERY, query, data_size, portMAX_DELAY));

    free(query);
}

static void telegram_new_obj(void *teleCtx, telegram_update_t *info)
{
    /* telegram_update_t type is not compatible with esp_event_loop */
    if ((info == NULL) || (teleCtx == NULL))
    {
        return;
    }

    if (adminList)
    {
        telegram_cclist_search_helper_t hlp = {};

        hlp.id = telegram_get_user_id_update(info);
        telegram_cclist_search(adminList, &hlp);
        if (!hlp.present)
        {
            ESP_LOGW(TAG, "REJECTED FROM: %.0lf", hlp.id);
            return;
        }
    }

    telegram_new_message(teleCtx, info);
    telegram_new_file(teleCtx, info);
    telegram_new_cbquery(teleCtx, info);
    
}

static void ip_event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch(event_id) {
        case IP_EVENT_STA_GOT_IP:
            if (teleCtx == NULL)
            {
                uint32_t telegram_message_limit = 0;
                esp_err_t err = ESP_ERR_INVALID_ARG;
                char *telegram_token = var_get("TELEGRAM_TOKEN");
                char *telegram_message_limit_str = var_get("TELEGRAM_MESSAGE_LIMIT");

                if (telegram_token == NULL)
                {
                    ESP_LOGE(TAG, "Failed to start telegram: Bad config!");
                    ESP_ERROR_CHECK(esp_event_post_to(simple_loop_handle, TELEGRAM_BASE, 
                        TELEGRAM_ERROR, &err, sizeof(err), portMAX_DELAY));
                    return;
                }

                if (telegram_message_limit_str != NULL)
                {
                    sscanf(telegram_message_limit_str, "%d", &telegram_message_limit);
                    free(telegram_message_limit_str);
                    ESP_LOGI(TAG, "Message limit: %d", telegram_message_limit);
                }
                teleCtx = telegram_init(telegram_token, telegram_message_limit, telegram_new_obj);
                free(telegram_token);

                if (teleCtx == NULL)
                {
                    err = ESP_ERR_NO_MEM;
                    ESP_LOGE(TAG, "Failed to start telegram: No memory!");
                    ESP_ERROR_CHECK(esp_event_post_to(simple_loop_handle, TELEGRAM_BASE, 
                        TELEGRAM_ERROR, &err, sizeof(err), portMAX_DELAY));
                    return;
                }

                adminList = telegram_cclist_load(adminList, "TELEGRAM_ADMIN_LIST");
                
                ESP_LOGI(TAG, "Telegram started! %X", (uint32_t)teleCtx);

                ESP_ERROR_CHECK(esp_event_post_to(simple_loop_handle, TELEGRAM_BASE, 
                        TELEGRAM_STARTED, &teleCtx, sizeof(teleCtx), portMAX_DELAY));

            }
        break;

        case IP_EVENT_STA_LOST_IP:
            ESP_ERROR_CHECK(esp_event_post_to(simple_loop_handle, TELEGRAM_BASE, 
                TELEGRAM_STOP, &teleCtx, sizeof(teleCtx), portMAX_DELAY));
            telegram_stop(teleCtx);
            teleCtx = NULL;
            telegram_cclist_free(adminList);
            adminList = NULL;
            break;

        default:
            ESP_LOGI(TAG, "IP_EVENT_ID = %d", event_id);
            break;
    }
}

static void telegram_manager_init(void)
{
	char *telegram_disable = var_get("TELEGRAM_DISABLE");


	if (telegram_disable != NULL)
	{
		if (!strcmp(telegram_disable, "1"))
		{
			free(telegram_disable);
			ESP_LOGI(TAG, "Telegram module disabled...");
			return;
		}
	}
	ESP_LOGI(TAG, "Telegram module init...");
	free(telegram_disable);

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, ip_event_handler, NULL));
}

static void cmd_telegram_alist_add(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
    char tmp[32];
    telegram_int_t *chat_id_mem;
    telegram_event_msg_t *evt = ((telegram_event_msg_t *)info->cmd_data);
    telegram_cclist_search_helper_t hlp = {.id = evt->user_id,};
    char *chat_id = (char *)&evt->text[strlen(cmd_name)];

    if (info->transport != CMD_SRC_TELEGRAM)
    {
        return;
    }

    if (*chat_id != '\0')
    {
        sscanf(chat_id + 1, "%lf", &hlp.id);
    }

    hlp.present = false;
    if (adminList != NULL)
    {
        telegram_cclist_search(adminList, &hlp);
        if (hlp.present)
        {
            snprintf(tmp, 32, "FAIL, Present: %.0f", hlp.id);
            telegram_send_text_message(evt->ctx, evt->chat_id, tmp);    
            return;
        }
    } 

    chat_id_mem = calloc(sizeof(telegram_int_t), 1);
    if (chat_id_mem == NULL)
    {
        ESP_LOGE(TAG, "No mem!");
        return;
    }

    *chat_id_mem = hlp.id;
    adminList = tlist_add(adminList, chat_id_mem);
    telegram_cclist_save(adminList, "TELEGRAM_ADMIN_LIST", true);
    snprintf(tmp, 32, "OK, Added: %.0f", hlp.id);
    telegram_send_text_message(evt->ctx, evt->chat_id, tmp);    
}

static void cmd_telegram_alist_show(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
    char *cclist = NULL;
    telegram_event_msg_t *evt = ((telegram_event_msg_t *)info->cmd_data);

    if (info->transport != CMD_SRC_TELEGRAM)
    {
        return;
    }

    cclist = var_get("TELEGRAM_ADMIN_LIST");
    if (cclist != NULL)
    {
        telegram_send_text_message(evt->ctx, evt->chat_id, cclist);
    } else
    {
        telegram_send_text_message(evt->ctx, evt->chat_id, "NULL");
    }

    free(cclist);
}

static void cmd_telegram_alist_del(const char *cmd_name, cmd_additional_info_t *info, void *private)
{
    char tmp[32];
    telegram_int_t chat_id_mem;
    telegram_event_msg_t *evt = ((telegram_event_msg_t *)info->cmd_data);
    char *chat_id = (char *)&evt->text[strlen(cmd_name)];

    if (info->transport != CMD_SRC_TELEGRAM)
    {
        return;
    }

    if (*chat_id == '\0')
    {
        telegram_send_text_message(evt->ctx, evt->chat_id, "Usage: alistd $chat_id");    
        return;
    }

    if (adminList != NULL)
    {
        sscanf(chat_id, "%lf", &chat_id_mem);
        if (!telegram_cclist_del(adminList, chat_id_mem))
        {
            telegram_send_text_message(evt->ctx, evt->chat_id, "FAIL:Not found!");    
            return;
        }

        snprintf(tmp, 32, "OK, Deleted: %s", chat_id);
        telegram_send_text_message(evt->ctx, evt->chat_id, tmp);    
        return;
        
    } else
    {
        telegram_send_text_message(evt->ctx, evt->chat_id, "FAIL list is NULL"); 
    }
}


module_init(telegram_manager_init);
cmd_register_static({
    {
        .name = "alista",
        .cmd_cb = cmd_telegram_alist_add,
    },
    {
        .name = "alistd",
        .cmd_cb = cmd_telegram_alist_del,
    },
    {
        .name = "alists",
        .cmd_cb = cmd_telegram_alist_show,
    },
});
