#include <string.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include "cmd_executor.h"

#define CMD_MAX_BLOCK_TIME 250U

struct cmd_list;

typedef struct cmd_list
{
	cmd_command_descr_t *descr;
	struct  cmd_list *next;
} cmd_list_t;

typedef struct
{
	SemaphoreHandle_t sem;
	cmd_list_t *cmds_head;
	bool is_inited;
} cmd_int_t;

static const char *TAG="CMD";
static cmd_int_t cmd;


static cmd_list_t *cmd_search(const char *cmd_name)
{
	cmd_list_t *iter = cmd.cmds_head;

	if (cmd_name == NULL)
	{
		return NULL;
	}

	while (iter != NULL)
	{
		if (!strcmp(cmd_name, iter->descr->name))
		{
			return iter;
		}

		iter = iter->next;
	}

	return NULL;
}

void cmd_init(void)
{
	cmd.sem = xSemaphoreCreateMutex();
	if (cmd.sem == NULL)
	{
		ESP_LOGE(TAG, "No mem!");
		return;
	}

	cmd.is_inited = true;
	ESP_LOGI(TAG, "CMD inited");
	return;
}

bool cmd_register(cmd_command_descr_t *descr)
{
	if (!cmd.is_inited)
	{
		ESP_LOGE(TAG, "Not inited");
		return false;
	}

	if ((descr == NULL) || (descr->name == NULL))
	{
		ESP_LOGW(TAG, "Wrong args");
		return false;
	}

	while(!xSemaphoreTake(cmd.sem, CMD_MAX_BLOCK_TIME))
	{
		ESP_LOGW(TAG, "cmd_register semaphore wait > CMD_MAX_BLOCK_TIME");
		vTaskDelay(pdMS_TO_TICKS(1));
	}

	if (cmd.cmds_head == NULL)
	{
		cmd.cmds_head = calloc(1, sizeof(cmd_list_t));
		if (cmd.cmds_head == NULL)
		{
			xSemaphoreGive(cmd.sem);
			ESP_LOGE(TAG, "Register - no mem");
			return false;
		}

		cmd.cmds_head->descr = descr;
	} else
	{
		cmd_list_t *iter = cmd.cmds_head->next;
		cmd_list_t *temp = calloc(1, sizeof(cmd_list_t));
		if (temp == NULL)
		{
			xSemaphoreGive(cmd.sem);
			ESP_LOGE(TAG, "Register - no mem");
			return false;
		}

		temp->descr = descr;
		while (iter->next != NULL)
		{
			iter = iter->next;
		}

		iter->next = temp;
	}

	xSemaphoreGive(cmd.sem);


	ESP_LOGI(TAG, "Register %s", descr->name);
	return true;
}

void cmd_deinit(void)
{
	cmd_list_t *temp;
	cmd_list_t *iter;

	if (!cmd.is_inited)
	{
		return;
	}

	cmd.is_inited = false;
	vSemaphoreDelete(cmd.sem);
	if (cmd.cmds_head != NULL)
	{
		iter = cmd.cmds_head->next;
		free(cmd.cmds_head);
		cmd.cmds_head = NULL;
		while (iter->next != NULL)
		{
			temp = iter;
			iter = iter->next;
			free(temp);
		}
	}
}

void cmd_execute(const char *cmd_name, void *args, cmd_additional_info_t *info)
{
	cmd_list_t *cmd_mem = NULL;
	if (!cmd.is_inited)
	{
		ESP_LOGE(TAG, "Not inited");
		return;
	}

	while(!xSemaphoreTake(cmd.sem, CMD_MAX_BLOCK_TIME))
	{
		ESP_LOGW(TAG, "cmd_execute_raw semaphore wait > CMD_MAX_BLOCK_TIME");
		vTaskDelay(pdMS_TO_TICKS(1));
	}

	cmd_mem = cmd_search(cmd_name);
	if (cmd_mem != NULL)
	{
		if (cmd_mem->descr->cmd_cb != NULL)
		{
			cmd_mem->descr->cmd_cb(cmd_name, args, info, cmd_mem->descr->private);
		}
	}
	xSemaphoreGive(cmd.sem);
}
