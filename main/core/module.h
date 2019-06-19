#ifndef MODULE_H
#define MODULE_H
#include <esp_event.h>

extern esp_event_loop_handle_t simple_loop_handle;

typedef struct
{
	void(*ptr)(void);
	const char *file;
} initcall_t;

#define module_init(fn) \
	 static const initcall_t __initcall_##fn  __attribute__((used)) \
	__attribute__((__section__(".initcall_simple"))) = { \
		.ptr = fn, \
		.file = __FILE__, \
	}

void module_init_all(void);

enum {                                      
    MODULE_EVENT_DONE,
    MODULE_EVENT_OTA,                     
};

ESP_EVENT_DECLARE_BASE(MODULE_BASE);
#endif