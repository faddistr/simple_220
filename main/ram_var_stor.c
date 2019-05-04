#include <string.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include "tlist.h"

#include "ram_var_stor.h"

#define VAR_STOR_MAX_BLOCK_TIME 250U

typedef struct
{
	char *key;
	char *value;
	bool save_to_flash;
} var_elem_t;

typedef struct
{
	SemaphoreHandle_t sem;
	void *list;
	bool is_inited;
} var_int_t;

static const char *TAG="RAM_STOR";
static var_int_t var;

typedef struct
{
	char *var_name;
	var_elem_t *result;
	bool del;
} var_search_helper_t;

typedef struct
{
	void *ctx;
	save_to_flash_cb_t cb;
} var_save_helper_t;

static void var_free(void *ctx, void *data)
{
	var_elem_t *elem = (var_elem_t *)data;


	free(elem->key);
	free(elem->value);
	free(elem);
}

static bool var_save_to_flash(void *ctx, void *data, void *tlist_el)
{
	var_elem_t *elem = (var_elem_t *)data;
	var_save_helper_t *hnd = (var_save_helper_t *)ctx;

	if (elem->save_to_flash)
	{
		hnd->cb(hnd->ctx, elem->key, elem->value);
	}

	return false;	
}

static bool var_search_list(void *ctx, void *data, void *tlist_el)
{
	var_elem_t *elem = (var_elem_t *)data;
	var_search_helper_t *hnd = (var_search_helper_t *)ctx;

	if (!strcmp(hnd->var_name, elem->key))
	{
		hnd->result = elem;
		if (hnd->del)
		{
			var_free(elem, NULL);
			tlist_del_one(tlist_el);
		}
		return true;
	}

	return false;
}

static var_elem_t *var_search_full(char *var_name, bool del)
{
	var_search_helper_t hnd = {};

	if (var.list == NULL) 
	{
		return NULL;
	}

	hnd.var_name = var_name;
	hnd.del = del;
	tlist_for_each(var.list, var_search_list, &hnd);
	return hnd.result;
} 

static var_elem_t *var_search(char *var_name)
{
	return var_search_full(var_name, false);
}

static void var_wait_for_sem(void)
{
	while(!xSemaphoreTake(var.sem, VAR_STOR_MAX_BLOCK_TIME))
	{
		ESP_LOGW(TAG, "semaphore wait > VAR_STOR_MAX_BLOCK_TIME");
		vTaskDelay(pdMS_TO_TICKS(1));
	}
}

void var_add_attr(char *var_name, char *var_value, bool save_to_flash)
{
	var_elem_t *elem;

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

	var_wait_for_sem();
	elem = var_search(var_name);
	if (elem == NULL)
	{
		elem = calloc(1, sizeof(var_elem_t));
		if (elem == NULL)
		{
			ESP_LOGE(TAG, "var_add_attr: no mem!!!");
		} else
		{
			elem->key = strdup(var_name);
			elem->save_to_flash = save_to_flash;
			var.list = tlist_add(var.list, elem);

			if (var.list == NULL)
			{
				ESP_LOGE(TAG, "Failed while adding value");
			}
		}
	} else
	{
		free(elem->value);
	}

	if (elem != NULL)
	{
		elem->value = strdup(var_value);
	}
	xSemaphoreGive(var.sem);
}

void var_del(char *var_name)
{
	if (var_name == NULL)
	{
		return;
	}

	ESP_LOGI(TAG, "VAR_DEL: %s", var_name);

	if (!var.is_inited)
	{
		ESP_LOGE(TAG, "Not inited");
		return;
	}	

	var_wait_for_sem();
	var_search_full(var_name, true);
	xSemaphoreGive(var.sem);
}

void var_add(char *var_name, char *var_value)
{
	var_add_attr(var_name, var_value, false);
}

char *var_get(char *var_name)
{
	var_elem_t *var_ptr = NULL;
	char *res = NULL;

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

	var_wait_for_sem();
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
	var.list = NULL;

	var.is_inited = true;
	ESP_LOGI(TAG, "Inited");
	return;
}

void var_save(void *ctx, save_to_flash_cb_t cb)
{
	var_save_helper_t hnd; 

	if (cb == NULL)
	{
		ESP_LOGW(TAG, "VAR_SAVE bad parrams");
		return;
	}

	if (!var.is_inited)
	{
		return;
	}


	var_wait_for_sem();
	tlist_for_each(var.list, var_save_to_flash, &hnd);
	xSemaphoreGive(var.sem);
}

void var_deinit(void)
{
	if (!var.is_inited)
	{
		return;
	}

	var_wait_for_sem();
	tlist_free_all(var.list, var_free, NULL);
	var.list = NULL;
	var.is_inited = false;
	vSemaphoreDelete(var.sem);
}