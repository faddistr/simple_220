#include <string.h>
#include <stdio.h>
#include <esp_log.h>
#include "telegram_cclist.h"
#include "ram_var_stor.h"
#include "tlist.h"

const char *cclist_fmt_str = "%lf ";

static const char *TAG="cclist";

static bool telegram_cclist_save_one(void *ctx, void *data, void *tlist_el)
{
	char **ptr = (char **)ctx;
	char *str = *ptr;
	telegram_int_t *val = (telegram_int_t *)data;

	sprintf(str, cclist_fmt_str, *val);
	*ptr += strlen(str);

	return false;
}

static bool telegram_cclist_count_strlen(void *ctx, void *data, void *tlist_el)
{
	char tmp[64];
	uint32_t *len = (uint32_t *)ctx;
	telegram_int_t *val = (telegram_int_t *)data;

	snprintf(tmp, 64, cclist_fmt_str, *val);
	*len = (strlen(tmp) + *len);

	return false;
}

static bool telegram_cclist_search_cb(void *ctx, void *data, void *tlist_el)
{
	telegram_cclist_search_helper_t *hlp = (telegram_cclist_search_helper_t *)ctx;
	telegram_int_t *val = (telegram_int_t *)data;

	hlp->present = (*val == hlp->id);

	if (hlp->present)
	{
		hlp->tlist_el = tlist_el;
	}

	return hlp->present;
}

void *telegram_cclist_load(void *teleNList, char *var_name)
{
	char *cclist;

	if (var_name == NULL)
	{
		ESP_LOGE(TAG, "Bad args!");
		return NULL;
	}

	cclist = var_get(var_name);
	if (cclist != NULL)
	{
		telegram_int_t *member_telegram_int;
		char *saveptr = NULL;
		char *member = strtok_r(cclist, " ", &saveptr);
		uint32_t count_rows = 0;
		
		while ((member != NULL) && (strlen(member) != 0))
		{
			member_telegram_int = calloc(1, sizeof(telegram_int_t));
			if (member_telegram_int == NULL)
			{
				ESP_LOGE(TAG, "No mem!");
				break;
			}

			sscanf(member, "%lf", member_telegram_int);
			ESP_LOGI(TAG, "%s = %lf", member, *member_telegram_int);

			teleNList = tlist_add(teleNList, member_telegram_int);
			count_rows++;
			member = strtok_r(NULL, " ", &saveptr);	
		}

		ESP_LOGI(TAG, "Loaded %d rows", count_rows);
	}

	free(cclist);
	return teleNList;
}

void telegram_cclist_save(void *teleNList, char *var_name, bool attr)
{
	uint32_t len = 0;
	char *full_str = NULL;
	char *full_str_cp = NULL;

	if ((teleNList == NULL) || (var_name == NULL))
	{
		ESP_LOGI(TAG, "Nothing to save :(");
		return;
	}

	tlist_for_each(teleNList, telegram_cclist_count_strlen, &len);
	ESP_LOGI(TAG, "Len = %d", len);
	if (len == 0)
	{
		ESP_LOGE(TAG, "List not NULL, but zero string length return.");
		return;
	}

	full_str = calloc(sizeof(char), len);
	if (full_str == NULL)
	{
		ESP_LOGE(TAG, "No mem!");
		return;
	}

	full_str_cp = full_str;
	tlist_for_each(teleNList, telegram_cclist_save_one, &full_str_cp);
	full_str[strlen(full_str) - 1] = '\0';
	var_add_attr(var_name, full_str, attr);
	free(full_str);
}

void telegram_cclist_search(void *teleNList, telegram_cclist_search_helper_t *hlp)
{
	if (hlp == NULL)
	{
		ESP_LOGE(TAG, "Bad args!");
		return;
	}

	hlp->present = false;
	if (teleNList != NULL)
	{
		tlist_for_each(teleNList, telegram_cclist_search_cb, hlp);
	}
}
