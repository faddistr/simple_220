#ifndef CMD_EXECUTOR_H
#define CMD_EXECUTOR_H
#include <stdbool.h>
#include "config.h"

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
	void *arg; 
 	void *user_ses;
 	void *cmd_data;
} cmd_additional_info_t;

typedef void(* cmde_cb_t)(const char *cmd_name, cmd_additional_info_t *info, void *private);

typedef struct
{
	const char 		    *name;
	cmde_cb_t			cmd_cb;
	void			    *private;
} cmd_command_descr_t;

void cmd_init(void);
bool cmd_register(cmd_command_descr_t *descr);
bool cmd_register_many(cmd_command_descr_t *descr, uint32_t size);
bool cmd_execute(const char *cmd_name, cmd_additional_info_t *info);

#define cmd_register_static(...) \
	 static const cmd_command_descr_t __cmd_descr_stat[]  __attribute__((used)) \
	__attribute__((__section__(".cmd_st_simple"))) = __VA_ARGS__

#endif