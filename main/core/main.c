#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <tcpip_adapter.h>
#include <esp_smartconfig.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "ota.h"

#include "telegram.h"
#include "config.h"
#include "cmd_executor.h"
#include "ram_var_stor.h"
#include "module.h"

static const char *TAG="SIMPLE";
esp_event_loop_handle_t simple_loop_handle;

static void event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    char tmp[8];
    ESP_LOGI(TAG, "MODULE LOADER event_id = %d, data = %d", event_id, *((int32_t *)event_data));

    if (event_id == MODULE_EVENT_DONE)
    {
        snprintf(tmp, 8, "%d", *((int32_t *)event_data));
        var_add("SYS_MODULE_AMOUNT", tmp);
    }
}


void simple_start(void)
{
    esp_event_loop_args_t loop_args = {
        .queue_size = CONFIG_SYSTEM_EVENT_QUEUE_SIZE,
        .task_name = "simple_evt_loop",
        .task_priority = ESP_TASKD_EVENT_STACK,
        .task_stack_size = 4096,
        .task_core_id = 0
    };

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_loop_create(&loop_args, &simple_loop_handle));
    var_init();
    if (config_load_vars() != ESP_OK)
    {
        ESP_LOGE(TAG, "Config missed!");
    }
    ESP_ERROR_CHECK(esp_event_handler_register_with(simple_loop_handle, MODULE_BASE, ESP_EVENT_ANY_ID, event_handler, NULL));

    module_init_all();
}
