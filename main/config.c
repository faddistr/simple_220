#include <esp_log.h>
#include <stdio.h>
#include <nvs_flash.h>
#include <nvs.h>
#include "config.h"

#define STORAGE_NAMESPACE "STORAGE"
#define CONFIG_NAME "wifi_config"


static const char *TAG="config";



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
	    }
	} while(0);

    nvs_close(handle);
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