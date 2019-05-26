#ifndef TELEGRAM_CCLIST_H
#define TELEGRAM_CCLIST_H
#include "telegram.h"

typedef struct
{
	telegram_int_t id;
	void *tlist_el;
	bool present;
	void *ptr;
} telegram_cclist_search_helper_t;

void *telegram_cclist_load(void *teleNList, char *var_name);
void telegram_cclist_save(void *teleNList, char *var_name, bool attr);
void telegram_cclist_search(void *teleNList, telegram_cclist_search_helper_t *hlp);
bool telegram_cclist_del(void *teleNList, telegram_int_t chat_id);
void telegram_cclist_free(void *teleNList);

#endif