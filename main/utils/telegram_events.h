#ifndef TELEGRAM_EVENTS_H
#define TELEGRAM_EVENTS_H
#include <esp_event.h>
#include "telegram.h"

enum {                                      
    TELEGRAM_STARTED,
    TELEGRAM_STOP,
    TELEGRAM_ERROR,
    TELEGRAM_MESSAGE,
};

typedef struct
{
	void *ctx;
	telegram_update_t *info;
} telegram_event_msg_t;

ESP_EVENT_DECLARE_BASE(TELEGRAM_BASE);

#endif /* TELEGRAM_EVENTS_H */