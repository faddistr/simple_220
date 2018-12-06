#include "plug.h"
#include "cmd_executor.h"


void *cmd_executor_init(void)
{
	plug_init();
	return NULL;
}

void cmd_executor_deinit(void *cmd_exec_ctx)
{

}

void cmd_execute_raw(void *cmd_exec_ctx, const char *cmd, const char *args,  cmd_additional_info_t *info)
{

}

void cmd_execute_telegram(void *cmd_exec_ctx, void *teleCtx, telegram_update_t *info,  cmd_additional_info_t *cmd_info)
{

}
