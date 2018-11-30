#ifndef TELEGRAM_H
#define TELEGRAM_H

#include <stdint.h>
#include <stdbool.h>

#define TELEGRAM_MSG_POLLING_INT 5U
#define TELEGRAM_MAX_TOKEN_LEN 	128U
#define TELEGRAM_SERVER 		"https://api.telegram.org"

typedef enum
{
	TELEGRAM_KBRD_MARKUP,
	TELEGRAM_KBRD_INLINE,
	TELEGRAM_KBRD_COUNT
}telegram_kbrd_type_t;

typedef struct 
{
	char *text;
	bool req_contact;
	bool req_loc;
} telegram_kbrd_btn_t;

typedef struct
{
	telegram_kbrd_btn_t *buttons; //Last element should be NULL
	bool resize;
	bool one_time;
	bool selective;
} telegram_kbrd_markup_t;

typedef struct 
{
	char *text;
	char callback_data[64];
} telegram_kbrd_inline_btn_t;

typedef struct
{
	telegram_kbrd_inline_btn_t *buttons; //Last element should be NULL
} telegram_kbrd_inline_t;

typedef struct 
{
	telegram_kbrd_type_t type;
	union {
		telegram_kbrd_markup_t markup;
		telegram_kbrd_inline_t inl;
	} kbrd;
} telegram_kbrd_t;

typedef void( *telegram_on_msg_cb_t)(void *teleCtx, double chat_id, const char *from, const char *text);

void telegram_kbrd(void *teleCtx_ptr, double chat_id, const char *message, telegram_kbrd_t *kbrd);
void telegram_send_text_message(void *teleCtx_ptr, double chat_id, const char *message);
void *telegram_init(const char *token, telegram_on_msg_cb_t cb);
void telegram_stop(void *teleCtx);
#endif // TELEGRAM_H