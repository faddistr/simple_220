#include <stdint.h>
#include <string.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include "tlist.h"

#define TLIST_MAX_BLOCK_TIME (250u)

static const char *TAG="TLIST";


struct tlist_el;

typedef struct tlist_el
{
	void *data_ptr;
	struct tlist_el *next;
	struct tlist_el *prev;
} tlist_el_t;

typedef struct
{
	SemaphoreHandle_t sem;
	tlist_el_t elements;
} tlist_head_t;

typedef struct
{
	tlist_free_cb_t data_free_cb;
	void *user_ctx;
}tlist_free_helper_struct;

static void *tlist_init(void *data_ptr)
{
	tlist_head_t *tlist_head = NULL;

	ESP_LOGI(TAG, "tlist_init: Creating new list...");
	tlist_head = calloc(1, sizeof(tlist_head_t));
	if (tlist_head == NULL)
	{
		ESP_LOGE(TAG, "tlist_init: No mem(1)!!!");
		return NULL;
	}

	tlist_head->elements.data_ptr = data_ptr;
	tlist_head->sem = xSemaphoreCreateBinary();
	if (tlist_head->sem == NULL)
	{
		ESP_LOGE(TAG, "tlist_init: No mem(2)!!!");
		free(tlist_head);
		return NULL;
	}


	xSemaphoreGive(tlist_head->sem);
	return tlist_head;
}

static void tlist_wait_mutex(tlist_head_t *tlist_head)
{
	while(!xSemaphoreTake(tlist_head->sem, TLIST_MAX_BLOCK_TIME))
	{
		ESP_LOGW(TAG, "TLIST semaphore wait > TLIST_MAX_BLOCK_TIME");
		vTaskDelay(pdMS_TO_TICKS(1));
	}
}

static bool tlist_clean(void *ctx, void *data, void *tlist_el)
{
	tlist_free_helper_struct *clean = (tlist_free_helper_struct *)ctx;

	if (clean->data_free_cb)
	{
		clean->data_free_cb(data, clean->user_ctx);
	}

	tlist_del_one(tlist_el);
	return false;
}

void *tlist_add(void *tlist_head, void *data_ptr)
{
	tlist_head_t *tlist_head_i = (tlist_head_t *)tlist_head;
	tlist_el_t *iter;

	if (data_ptr == NULL)
	{
		ESP_LOGE(TAG, "tlist_add: No data! Wrong params!");
		return NULL;
	}

	if (tlist_head == NULL)
	{
		return tlist_init(data_ptr);
	}

	tlist_wait_mutex(tlist_head_i);
	iter = &tlist_head_i->elements;

	if (!iter)
	{
		ESP_LOGE(TAG, "tlist_add: no elements!");
	} else
	{
		uint32_t count = 0;

		while (iter->next)
		{
			iter = iter->next;
			count++;
		}
		
		iter->next = calloc(1, sizeof(tlist_el_t));
		if (iter->next == NULL)
		{
			ESP_LOGE(TAG, "tlist_add: no mem!");
		} else
		{
			tlist_el_t *new_container = iter->next;

			new_container->prev = iter;
			new_container->data_ptr = data_ptr;
		}

		ESP_LOGI(TAG, "tlist_add: Elements count = %d", count);
	}

	xSemaphoreGive(tlist_head_i->sem);
	return tlist_head;
}

void tlist_for_each(void *tlist_head, tlist_cb_t cb, void *ctx)
{
	tlist_head_t *tlist_head_i = (tlist_head_t *)tlist_head;
	tlist_el_t *iter;

	if ((tlist_head == NULL) || (cb == NULL))
	{
		ESP_LOGE(TAG, "tlist_for_each: Wrong params!");
		return;
	}

	tlist_wait_mutex(tlist_head_i);
	iter = &tlist_head_i->elements;

	if (!iter)
	{
		ESP_LOGE(TAG, "tlist_for_each: no elements!");
	} else
	{
		bool res = false;
		tlist_el_t *temp;

		while (iter)
		{
			temp = iter->next;
			xSemaphoreGive(tlist_head_i->sem);
			res = cb(ctx, iter->data_ptr, iter);
			tlist_wait_mutex(tlist_head_i);

			iter = temp;
			if (res)
			{
				break;
			}
		}
	}


	xSemaphoreGive(tlist_head_i->sem);
}

void tlist_del_one(void *tlist_el)
{
	tlist_el_t *element = (tlist_el_t *)tlist_el;

	ESP_LOGI(TAG, "tlist_del_one");
	if (tlist_el == NULL)
	{
		ESP_LOGE(TAG, "tlist_del_one: Wrong param!");
		return;
	}

	if (element->prev == NULL)
	{
		if (element->next != NULL)
		{
			tlist_el_t *next = element->next;
			memcpy(element, next, sizeof(tlist_el_t));
			element->prev = NULL;
			free(next);
		} else
		{
			element->data_ptr = NULL;
		}

	} else
	{
		tlist_el_t *prev = element->prev;

		prev->next = element->next;
		free(element);
	}
}

void tlist_free_head(void *tlist_head)
{
	tlist_head_t *head = (tlist_head_t *)tlist_head;

	ESP_LOGI(TAG, "tlist_free_head");
	if (tlist_head == NULL)
	{
		ESP_LOGE(TAG, "tlist_free_head: Wrong param!");
		return;		
	}
	
	tlist_wait_mutex(head);
	vSemaphoreDelete(head->sem);
	free(head);
}

void tlist_free_all(void *tlist_head, tlist_free_cb_t data_free_cb, void *ctx)
{
	tlist_free_helper_struct *clean;

	ESP_LOGI(TAG, "tlist_free_all");
	if (tlist_head == NULL)
	{
		ESP_LOGE(TAG, "tlist_free_all: Wrong param!");
		return;	
	}

	clean = calloc(1, sizeof(tlist_free_helper_struct));
	if (clean == NULL)
	{
		ESP_LOGE(TAG, "tlist_free_all: No mem!");
		return;		
	}

	clean->user_ctx = ctx;
	clean->data_free_cb = data_free_cb;

	tlist_for_each(tlist_head, tlist_clean, clean);

	free(clean);
	tlist_free_head(tlist_head);
}