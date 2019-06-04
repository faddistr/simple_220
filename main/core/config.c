#include <esp_log.h>
#include <stdio.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <string.h>
#include "config.h"
#include "module.h"
#include "ram_var_stor.h"

#define STORAGE_NAMESPACE "STORAGE"
#define CONFIG_NAME "wifi_config"
#define CONFIG_NAME_VARS "vars"


static const char *TAG="config";

typedef struct
{
    uint8_t *ptr;
    size_t cur_offset;
} config_save_vars_helper_t;


static void config_calculate_size_cb(void *ctx, char *key, char *value)
{
    size_t *total_size = (size_t *)ctx;
    *total_size = *total_size + strlen(key) + strlen(value) + 2;
    ESP_LOGI(TAG, "Save to flash: %s = %s (%d)", key, value, *total_size);
}

static void config_list_to_array_cb(void *ctx, char *key, char *value)
{
    config_save_vars_helper_t *hnd = (config_save_vars_helper_t *)ctx;
    strcpy((char *)&hnd->ptr[hnd->cur_offset], key);
    hnd->cur_offset += strlen(key) + 1;

    strcpy((char *)&hnd->ptr[hnd->cur_offset], value);
    hnd->cur_offset += strlen(value) + 1;

    ESP_LOGI(TAG, "Cpy to arr: %s = %s (%d)", key, value, hnd->cur_offset);
}

void config_load_vars_mem(uint8_t *src, size_t total_size)
{
    size_t len = 0;
    char *key;
    char *value;

    if (src == NULL)
    {
        ESP_LOGE(TAG, "Bad arguments");
        return;
    }

    while (total_size)
    {
        key = (char *)src;
        len = strlen(key) + 1;
        if (total_size <= len)
        {
            break;
        }

        total_size -= len;
        src += len;
        value = (char *)src;
        len = strlen(value) + 1;
        if (total_size < len)
        {
            break;
        }

        total_size -= len;
        src += len;

        var_add_attr(key, value, true);
    }
}

uint8_t *config_get_vars_mem(uint32_t *total_size)
{
    config_save_vars_helper_t hnd = {};

    if (total_size == NULL)
    {
        ESP_LOGE(TAG, "Bad arguments");
        return NULL;
    }

    var_save(total_size, config_calculate_size_cb);
    hnd.ptr = calloc(*total_size, sizeof(uint8_t));
    if (hnd.ptr == NULL)
    {
        ESP_LOGE(TAG, "config_get_vars no mem!");
        return NULL;
    }
    var_save(&hnd, config_list_to_array_cb);

    return hnd.ptr;
}

void config_save_vars(void)
{
    esp_err_t err;
    nvs_handle handle;
    config_save_vars_helper_t hnd = {};
    size_t total_size = 0;

    var_save(&total_size, config_calculate_size_cb);
    hnd.ptr = calloc(total_size, sizeof(uint8_t));
    if (hnd.ptr == NULL)
    {
        ESP_LOGE(TAG, "config_save_vars no mem!");
        return;
    }
    var_save(&hnd, config_list_to_array_cb);

    ESP_LOGI(TAG, "Saving vars...");
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed open storage! %d", err);
    }

    if (err == ESP_OK)
    {
        err = nvs_set_blob(handle, CONFIG_NAME_VARS, hnd.ptr, total_size);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed set blob! %d", err);
        }
    }

    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed commit vars! %d", err);
        }
    }
    
    free(hnd.ptr);
    nvs_close(handle);
    return;
}


esp_err_t config_load_vars(void)
{
    esp_err_t err = ESP_OK;
    nvs_handle handle;
    size_t saved_size = 0;
    uint8_t *ptr = NULL;

    ESP_LOGI(TAG, "Loading vars...");
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed open storage! %d", err);
        return err;
    }

    err = nvs_get_blob(handle, CONFIG_NAME_VARS, NULL, &saved_size);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed get size of vars! %d", err);
    }

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Size of saved vars = %d", saved_size);
        ptr = calloc(saved_size, sizeof(uint8_t));

        if (ptr == NULL)
        {
            ESP_LOGE(TAG, "No mem for vars!");
            err = ESP_FAIL;
        } else
        {
            err = nvs_get_blob(handle, CONFIG_NAME_VARS, ptr, &saved_size);
            if (err != ESP_OK)
            {
                ESP_LOGW(TAG, "Failed to get blob of vars! %d", err);
            }
        }
   
    }

    nvs_close(handle); 
   
    if (err == ESP_OK)
    {
        config_load_vars_mem(ptr, saved_size);
    }

    free(ptr);
    return err;
}