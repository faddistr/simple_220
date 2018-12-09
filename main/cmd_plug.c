#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "plug.h"
#include "cmd_executor.h"

static void cmd_plug_cb(const char *cmd_name, void *args, cmd_additional_info_t *info, void *private);

static cmd_command_descr_t cmd_plug_descr =
{
	.name = "plug",
	.cmd_cb = cmd_plug_cb,
};

static void cmd_plug_cb_httpb(const char *cmd_name, void *args, cmd_additional_info_t *info, void *private)
{
	uint32_t key = 0;
	uint32_t val = 0;
	char *save_ptr = NULL;
	bool is_key = false;
	bool is_val = false;
	char *token = strtok_r((char *)args, " ", &save_ptr);

	if (token == NULL)
	{
		return;
	}

	do
	{
		if (!strcmp(token, "key"))
		{
			token = strtok_r(NULL, " ", &save_ptr);
			if (token == NULL)
			{
				break;
			}

			is_key = (sscanf(token, "%d", &key) == 1);
		}

		if (!strcmp(token, "value"))
		{
			token = strtok_r(NULL, " ", &save_ptr);
			if (token == NULL)
			{
				break;
			}

			is_val = (sscanf(token, "%d", &val) == 1);
		}

		if (is_key && is_val)
		{
			break;
		}
		token = strtok_r(NULL, " ", &save_ptr);
	} while (token != NULL);

	plug_set_key(key, !!val);
}

static void cmd_plug_cb(const char *cmd_name, void *args, cmd_additional_info_t *info, void *private)
{
	switch (info->transport)
	{
		case CMD_SRC_HTTPB:
			cmd_plug_cb_httpb(cmd_name, args, info, private);
			break;

		default:
			break;
	}
}

bool cmd_plug_register(void)
{
	return cmd_register(&cmd_plug_descr);
}