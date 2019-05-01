#ifndef RAM_VAR_STOR_H
#define RAM_VAR_STOR_H

typedef void(*save_to_flash_cb_t)(void *ctx, char *var_name, char *var_value);

void var_add(char *var_name, char *var_value);
void var_add_attr(char *var_name, char *var_value, bool save_to_flash);
void var_save(void *ctx, save_to_flash_cb_t cb);
char *var_get(char *var_name);

void var_init(void);

void var_deinit(void);

#endif