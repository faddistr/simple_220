#include <esp_log.h>
#include <stdio.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <string.h>
#include "config.h"
#include "ram_var_stor.h"

#define STORAGE_NAMESPACE "STORAGE"
#define CONFIG_NAME "wifi_config"
#define CONFIG_NAME_VARS "vars"


static const char *TAG="config";
#if 0
struct cfg_var_list;

typedef struct cfg_var_list
{
    char *key;
    char *value;
    struct  cfg_var_list *next;
} cfg_var_list_t;

static void config_save_vars_cb(void *ctx, char *key, char *value)
{
}

esp_err_t config_save_vars(void)
{
    esp_err_t err;
    nvs_handle handle;

    ESP_LOGI(TAG, "Saving vars...");
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed open storage! %d", err);
        return;
    }

    do
    {
        err = nvs_set_blob(handle, CONFIG_NAME_VARS, config, sizeof(config_t));
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed save vars! %d", err);
            break;
        }

        err = nvs_commit(handle);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed commit vars! %d", err);
        }
    } while(0);

    nvs_close(handle);

}
#endif

esp_err_t config_load(config_t *config)
{
	esp_err_t err;
    nvs_handle handle;
    size_t saved_size = 0;

    ESP_LOGI(TAG, "Loading config...");

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
    	ESP_LOGW(TAG, "Failed open storage! %d", err);
    	return err;
    }

    do
    {
	    err = nvs_get_blob(handle, CONFIG_NAME, NULL, &saved_size);
	    if (err != ESP_OK)
	    {
	    	ESP_LOGW(TAG, "Failed open config! %d", err);
	    	break;
	    }

	    if (saved_size != sizeof(config_t))
		{
			ESP_LOGW(TAG, "Incorrect size! %u", saved_size);
			break;
		}

		err = nvs_get_blob(handle, CONFIG_NAME, config, &saved_size);
	    if (err != ESP_OK)
	    {
	       	ESP_LOGW(TAG, "Failed read config! %d", err);
	    } else
        {
            char tmp[16] = {0};

            var_add_attr("WIFI_SSID", config->ssid, true);
            var_add_attr("WIFI_PASSWORD", config->password, true);
            var_add_attr("HTTP_PASSWORD", config->user_pass, true);
            var_add_attr("TELEGRAM_TOKEN", config->telegram_token, true);
            sprintf(tmp, "%u", saved_size);
            var_add_attr("FLASH_CONFIG_SIZE", tmp, false);
        }
	} while(0);

	nvs_close(handle);
	if (strlen(config->ssid) == 0)
	{
		err = ESP_FAIL;
	}
    return err;
}

void config_save(config_t *config)
{
	esp_err_t err;
    nvs_handle handle;

	ESP_LOGI(TAG, "Saving config...");
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
    	ESP_LOGW(TAG, "Failed open storage! %d", err);
    	return;
    }

    do
    {
		err = nvs_set_blob(handle, CONFIG_NAME, config, sizeof(config_t));
    	if (err != ESP_OK)
    	{
       		ESP_LOGW(TAG, "Failed save config! %d", err);
       		break;
    	}

    	err = nvs_commit(handle);
    	if (err != ESP_OK)
    	{
       		ESP_LOGW(TAG, "Failed commit config! %d", err);
    	}
    } while(0);

    nvs_close(handle);
}