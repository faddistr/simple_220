#ifndef CMD_EXECUTOR_H
#define CMD_EXECUTOR_H

#include <stdbool.h>
#include "config.h"
#include "telegram.h"

typedef enum
{
	CMD_SRC_HTTPB,
	CMD_SRC_TELEGRAM,
	CMD_SRC_CNT,
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


typedef void(* cmd_cb_raw_t)(const char *cmd, const char *args,  cmd_additional_info_t *info, 
	void *private);

typedef void(* cmd_cb_tele_t)(void *teleCtx, telegram_update_t *info, cmd_additional_info_t *cmd_info,
	void *private);

typedef struct
{
	const char 		   *name;
	cmd_cb_raw_t		raw_cb;
	cmd_cb_tele_t		tele_cb;
	void			    *private;
} cmd_command_descr_t;

void cmd_init(void);
bool cmd_register(cmd_command_descr_t *descr);
void cmd_execute_raw(const char *cmd, const char *args,  cmd_additional_info_t *info);
void cmd_execute_telegram(void *teleCtx, telegram_update_t *info, cmd_additional_info_t *cmd_info);

#endif