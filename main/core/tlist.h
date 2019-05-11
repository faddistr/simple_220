#ifndef T_LIST_H
#define T_LIST_H
typedef bool(* tlist_cb_t)(void *ctx, void *data, void *tlist_el);
typedef void(* tlist_free_cb_t)(void *ctx, void *data);


void *tlist_add(void *tlist_head, void *data_ptr);
void tlist_for_each(void *tlist_head, tlist_cb_t cb, void *ctx);

void tlist_del_one(void *tlist_el);
void tlist_free_head(void *tlist_head);
void tlist_free_all(void *tlist_head, tlist_free_cb_t data_free_cb, void *ctx);

#endif /* T_LIST_H */