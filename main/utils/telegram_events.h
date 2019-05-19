#ifndef TELEGRAM_EVENTS_H
#define TELEGRAM_EVENTS_H
#include <esp_event.h>
#include "telegram.h"

enum {                                      
    TELEGRAM_STARTED,
    TELEGRAM_STOP,
    TELEGRAM_ERROR,
    TELEGRAM_MESSAGE,
    TELEGRAM_FILE,
};

typedef struct
{
	void 			 *ctx;
	telegram_int_t   chat_id;
	telegram_int_t   user_id;
	char       		 text[]; /* This member should be last */
} telegram_event_msg_t;


typedef struct
{
	void 			 *ctx;
	uint32_t		 id_str_offset;;
	uint32_t 		 name_str_offset; 
	uint32_t 		 mime_type_str_offset;
	uint32_t 		 caption_str_offset;
	uint32_t		 data_size;
	telegram_int_t   chat_id;
	telegram_int_t   user_id;
	telegram_int_t   file_size;
	char			 blob[]; /* This member should be last */
} telegram_event_file_t;

ESP_EVENT_DECLARE_BASE(TELEGRAM_BASE);

#endif /* TELEGRAM_EVENTS_H */