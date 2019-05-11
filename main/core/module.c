#include <esp_log.h>
#include "module.h"
static const char *TAG="MAIN_MODULE";

extern initcall_t _initcall_simple_start;
extern initcall_t _initcall_simple_stop;

void module_init_all(void)
{
	ESP_LOGI(TAG, "Loading modules...");

 	for (initcall_t *iter = &_initcall_simple_start ; iter < &_initcall_simple_stop; ++iter)
 	{
 		(*iter)();
 	}
}
