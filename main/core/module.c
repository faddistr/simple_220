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
	uint32_t lb;
	uint32_t usage;
	uint32_t prev = 0;
	int32_t count = 0;
	ESP_LOGI(TAG, "Loading modules...");

 	for (initcall_t *iter = &_initcall_simple_start ; iter < &_initcall_simple_stop; ++iter)
 	{
 		prev = heap_caps_get_free_size(MALLOC_CAP_8BIT);
 		lb  = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
		ESP_LOGI(TAG, "Free RAM: %d", prev);
		ESP_LOGI(TAG, "Largest block: %d", lb);
		ESP_LOGI(TAG, "Starting stuff from %s ADDR: %X", iter->file, (uint32_t)iter->ptr);
 		(*iter->ptr)();
 		usage = heap_caps_get_free_size(MALLOC_CAP_8BIT);
 		ESP_LOGI(TAG, "Mem used: %d", prev - usage);
 		count++;
 	}
 	prev = heap_caps_get_free_size(MALLOC_CAP_8BIT);
 	lb  = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
	ESP_LOGI(TAG, "FREE RAM: %d", prev);
	ESP_LOGI(TAG, "Largest block: %d", lb);
 	ESP_LOGI(TAG, "Loaded modules: %d", count);

 	ESP_ERROR_CHECK(esp_event_post_to(simple_loop_handle, MODULE_BASE, MODULE_EVENT_DONE, &count, sizeof(count), portMAX_DELAY));
}
