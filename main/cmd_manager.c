#include <esp_log.h>
#include "cmd_manager.h"
#include "cmd_executor.h"


static const char *TAG="MAIN";


bool cmd_plug_register(void);

void cmd_manager_register_all(void)
{
	if (!cmd_plug_register())
	{
		ESP_LOGW(TAG, "Failed to register plug cmd!");
	}
}