#ifndef TELEGRAM_H
#define TELEGRAM_H

#define TELEGRAM_MSG_POLLING_INT 5U
#define TELEGRAM_MAX_TOKEN_LEN 	128U
#define TELEGRAM_SERVER 		"https://api.telegram.org"

typedef void( *telegram_on_msg_cb_t)(void *teleCtx, double chat_id, const char *from, const char *text);

void telegram_send_text_message(void *teleCtx_ptr, double chat_id, const char *message);
void *telegram_init(const char *token, telegram_on_msg_cb_t cb);
void telegram_stop(void *teleCtx);
#endif // TELEGRAM_H