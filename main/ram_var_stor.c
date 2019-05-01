#include <string.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include "ram_var_stor.h"

#define VAR_STOR_MAX_BLOCK_TIME 250U


struct var_list;

typedef struct var_list
{
	char *key;
	char *value;
	struct  var_list *next;
	bool save_to_flash;
} var_list_t;

typedef struct
{
	SemaphoreHandle_t sem;
	var_list_t *head;
	bool is_inited;
} var_int_t;

static const char *TAG="RAM_STOR";
static var_int_t var;

static var_list_t *var_search(char *var_name)
{
	var_list_t *iter = var.head;

	if (var_name == NULL)
	{
		return NULL;
	}

	while (iter != NULL)
	{
		if (!strcmp(var_name, iter->key))
		{
			return iter;
		}

		iter = iter->next;
	}

	return NULL;
} 


void var_add_attr(char *var_name, char *var_value, bool save_to_flash)
{
	if ((var_name == NULL) || (var_value == NULL))
	{
		return;
	}

	ESP_LOGI(TAG, "VAR_ADD: %s = %s (%d)", var_name, var_value, (int)save_to_flash);

	if (!var.is_inited)
	{
		ESP_LOGE(TAG, "Not inited");
		return;
	}

	while(!xSemaphoreTake(var.sem, VAR_STOR_MAX_BLOCK_TIME))
	{
		ESP_LOGW(TAG, "VAR_ADD semaphore wait > VAR_STOR_MAX_BLOCK_TIME");
		vTaskDelay(pdMS_TO_TICKS(1));
	}

	if (var.head == NULL)
	{
		var.head = calloc(1, sizeof(var_list_t));
		if (var.head == NULL)
		{
			xSemaphoreGive(var.sem);
			ESP_LOGE(TAG, "Register - no mem");
			return false;
		}

		var.head->key = strdup(var_name);
		var.head->value = strdup(var_value);
		var.head->save_to_flash = save_to_flash;
	} else
	{
		var_list_t *var_ptr = var_search(var_name);
		if (var_ptr != NULL)
		{
			free(var_ptr->value);
			var_ptr->value = strdup(var_value);

		} else
		{
			var_list_t *iter = var.head;
			var_list_t *temp = calloc(1, sizeof(var_list_t));
			if (temp == NULL)
			{
				xSemaphoreGive(var.sem);
				ESP_LOGE(TAG, "Register - no mem");
				return false;
			}

			temp->key = strdup(var_name);
			temp->value = strdup(var_value);
			temp->save_to_flash = save_to_flash;


			while (iter->next != NULL)
			{
				iter = iter->next;
			}

			iter->next = temp;
		}
	}

	xSemaphoreGive(var.sem);
}

void var_add(char *var_name, char *var_value)
{
	var_add_attr(var_name, var_value, false);
}


char *var_get(char *var_name)
{
	char *res = NULL;
	var_list_t *var_ptr;
	
	if (var_name == NULL)
	{
		return NULL;
	}

	ESP_LOGI(TAG, "VAR_GET: %s", var_name);

	if (!var.is_inited)
	{
		ESP_LOGE(TAG, "Not inited");
		return NULL;
	}

	while(!xSemaphoreTake(var.sem, VAR_STOR_MAX_BLOCK_TIME))
	{
		ESP_LOGW(TAG, "VAR_GET semaphore wait > VAR_STOR_MAX_BLOCK_TIME");
		vTaskDelay(pdMS_TO_TICKS(1));
	}

	var_ptr = var_search(var_name);
	if (var_ptr)
	{
		res = strdup(var_ptr->value);
	}


	xSemaphoreGive(var.sem);
	return res;
}

void var_init(void)
{
	var.sem = xSemaphoreCreateBinary();
	if (var.sem == NULL)
	{
		ESP_LOGE(TAG, "No mem!");
		return;
	}

	xSemaphoreGive(var.sem);


	var.is_inited = true;
	ESP_LOGI(TAG, "Inited");
	return;
}

void var_save(void *ctx, save_to_flash_cb_t cb)
{
	var_list_t *iter;
	
	if (cb == NULL)
	{
		ESP_LOGW(TAG, "VAR_SAVE bad parrams");
		return;
	}

	if (!var.is_inited)
	{
		return;
	}

	while(!xSemaphoreTake(var.sem, VAR_STOR_MAX_BLOCK_TIME))
	{
		ESP_LOGW(TAG, "VAR_GET semaphore wait > VAR_STOR_MAX_BLOCK_TIME");
		vTaskDelay(pdMS_TO_TICKS(1));
	}

	iter = var.head;

	while(iter)
	{
		if (iter->save_to_flash)
		{
			cb(ctx, iter->key, iter->value);
		}

		iter = iter->next;
	}

	xSemaphoreGive(var.sem);
}

void var_deinit(void)
{
	var_list_t *temp;
	var_list_t *iter;

	if (!var.is_inited)
	{
		return;
	}

	var.is_inited = false;
	vSemaphoreDelete(var.sem);
	if (var.head != NULL)
	{
		iter = var.head->next;
		free(var.head);
		var.head = NULL;
		if (iter != NULL)
		{
			while (iter->next != NULL)
			{
				temp = iter;
				iter = iter->next;
				free(temp->key);
				free(temp->value);
				free(temp);
			}
		}
	}
}