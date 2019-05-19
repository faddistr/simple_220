#ifndef TELEGRAM_CCLIST_H
#define TELEGRAM_CCLIST_H
#include "telegram.h"

typedef struct
{
	telegram_int_t id;
	void *tlist_el;
	bool present;
} telegram_cclist_search_helper_t;

void *telegram_cclist_load(void *teleNList, char *var_name);
void telegram_cclist_save(void *teleNList, char *var_name, bool attr);
void telegram_cclist_search(void *teleNList, telegram_cclist_search_helper_t *hlp);

#endif