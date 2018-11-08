#ifndef TELEGRAM_H
#define TELEGRAM_H

#define TELEGRAM_MSG_POLLING_INT 5U
#define TELEGRAM_MAX_TOKEN_LEN 	128U
#define TELEGRAM_SERVER 		"https://api.telegram.org"


void telegram_init(const char *token);
void telegram_stop(void);
#endif // TELEGRAM_H