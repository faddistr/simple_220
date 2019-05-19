#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "module.h"
static const char *TAG="MODULE_LOADER";

extern initcall_t _initcall_simple_start;
extern initcall_t _initcall_simple_stop;

ESP_EVENT_DEFINE_BASE(MODULE_BASE);

void module_init_all(void)
{
	int32_t count = 0;
	ESP_LOGI(TAG, "Loading modules...");

 	for (initcall_t *iter = &_initcall_simple_start ; iter < &_initcall_simple_stop; ++iter)
 	{
 		(*iter)();
 		count++;
 	}

 	ESP_LOGI(TAG, "Loaded modules: %d", count);

 	ESP_ERROR_CHECK(esp_event_post_to(simple_loop_handle, MODULE_BASE, MODULE_EVENT_DONE, &count, sizeof(count), portMAX_DELAY));
}
