#ifndef CMD_EXECUTOR_H
#define CMD_EXECUTOR_H

#include <stdbool.h>
#include "config.h"
#include "telegram.h"

typedef enum
{
	CMD_SRC_HTTPB,
	CMD_SRC_TELEGRAM,
} cmd_src_tansport_t;

typedef void(*send_cb_t)(void *arg, void *buff, uint32_t buff_len);

typedef struct
{
	cmd_src_tansport_t transport;
	send_cb_t send_cb;
	void *arg; 
 	void *user_ses;
 	config_t *sys_config;
 	bool sys_config_changed;
} cmd_additional_info_t;

void *cmd_executor_init(void);
void cmd_executor_deinit(void *cmd_exec_ctx);
void cmd_execute_raw(void *cmd_exec_ctx, const char *cmd, const char *args,  cmd_additional_info_t *info);
void cmd_execute_telegram(void *cmd_exec_ctx, void *teleCtx, telegram_update_t *info, cmd_additional_info_t *cmd_info);

#endif