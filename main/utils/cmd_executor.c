#include <string.h>
#include <esp_log.h>
#include "cmd_executor.h"
#include "tlist.h"

extern cmd_command_descr_t _cmd_st_simple_start;
extern cmd_command_descr_t _cmd_st_simple_stop;

typedef struct
{
	const char *cmd_name;
	cmd_command_descr_t *result;
} cmd_search_helper_t;

static const char *TAG="CMD";
static void *cmd = NULL ;

static bool cmd_search_list(void *ctx, void *data, void *tlist_el)
{
	cmd_search_helper_t *hnd = (cmd_search_helper_t *)ctx;
	cmd_command_descr_t *descr = (cmd_command_descr_t *)data;

	if (!strcmp(hnd->cmd_name, descr->name))
	{
		hnd->result = descr;
		return true;
	}

	return false;
}

static cmd_command_descr_t *cmd_search(const char *cmd_name)
{
	cmd_search_helper_t hnd;
	if (cmd_name == NULL)
	{
		return NULL;
	}

	hnd.cmd_name = cmd_name;
	hnd.result = NULL;
	tlist_for_each(cmd, cmd_search_list, &hnd);
	return hnd.result;
}

static cmd_command_descr_t *cmd_search_static(const char *cmd_name)
{
	if (cmd_name == NULL)
	{
		return NULL;
	}

 	for (cmd_command_descr_t *iter = &_cmd_st_simple_start ; iter < &_cmd_st_simple_stop; ++iter)
 	{
		if (!strcmp(cmd_name, iter->name))
		{
			return iter;
		}
 	}

 	return NULL;
}

bool cmd_register(cmd_command_descr_t *descr)
{
	if ((descr == NULL) || (descr->name == NULL) || (descr->cmd_cb == NULL))
	{
		ESP_LOGW(TAG, "Wrong args");
		return false;
	}

	ESP_LOGI(TAG, "Register %s", descr->name);
	cmd = tlist_add(cmd, descr);

	return true;
}

void cmd_deinit(void)
{
	tlist_free_all(cmd, NULL, NULL);
	cmd = NULL;
}

bool cmd_execute(const char *cmd_name, cmd_additional_info_t *info)
{
	cmd_command_descr_t *cmd_mem = NULL;

	ESP_LOGI(TAG, "Looking for %s", cmd_name);

	cmd_mem = cmd_search_static(cmd_name);
	if (cmd_mem == NULL)
	{
		cmd_mem = cmd_search(cmd_name);
	}

	if (cmd_mem != NULL)
	{
		ESP_LOGI(TAG, "Executing...");
		cmd_mem->cmd_cb(cmd_name, info, cmd_mem->private);
	} else
	{
		ESP_LOGW(TAG, "Command not found");
	}

	return (cmd_mem != NULL);
}
