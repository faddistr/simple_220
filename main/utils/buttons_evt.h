#ifndef BUTTONS_EVT_H
#define BUTTONS_EVT_H
#include <esp_event.h>

#define NO_BUTTONS 1

#if NO_BUTTONS == 0
enum
{
	BUTTON_PRESS,
	BUTTON_UNPRESS
};

ESP_EVENT_DECLARE_BASE(BUTTONS_BASE);
#endif /* #if NO_BUTTONS == 0 */
#endif /* BUTTONS_EVT_H */