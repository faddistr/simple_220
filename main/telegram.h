#ifndef TELEGRAM_H
#define TELEGRAM_H

#include <stdint.h>
#include <stdbool.h>

#define TELEGRAM_MSG_POLLING_INT 5U
#define TELEGRAM_MAX_TOKEN_LEN 	128U
#define TELEGRAM_SERVER 		"https://api.telegram.org"

/** Telegram int type, 52 bits, uses double for compatibility with cjson lib*/
typedef double telegram_int_t;

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
	telegram_kbrd_btn_t *buttons; /** Button array. Last element should be NULL */
	bool resize;
	bool one_time;
	bool selective;
} telegram_kbrd_markup_t;

typedef struct 
{
	char *text;
	char callback_data[64 + 1];
} telegram_kbrd_inline_btn_t;

typedef struct
{
	telegram_kbrd_inline_btn_t *buttons; /** Button array. Last element should be NULL */
} telegram_kbrd_inline_t;

typedef union
{
	telegram_kbrd_markup_t markup;
	telegram_kbrd_inline_t inl;
} telegram_kbrd_descr_t;

typedef struct 
{
	telegram_kbrd_type_t type;
	telegram_kbrd_descr_t kbrd;
} telegram_kbrd_t;

/** This object represents a Telegram user or bot */
typedef struct
{
	telegram_int_t id;             /** Unique identifier for this user or bot */
	bool is_bot;                   /** True, if this user is a bot */    
	const char *first_name;        /** User‘s or bot’s first name */
	const char *last_name;         /** Opt. User‘s or bot’s last name */
	const char *username;          /** Opt. User‘s or bot’s username */
	const char *language_code;     /** Opt. IETF language tag of the user's language */
} telegram_user_t;

/** Type of the chat possible values */
typedef enum
{
	TELEGRAM_CHAT_TYPE_UNIMPL,
	TELEGRAM_CHAT_TYPE_PRIVATE,
	TELEGRAM_CHAT_TYPE_CHAN,
	TELEGRAM_CHAT_TYPE_GROUP,
	TELEGRAM_CHAT_TYPE_SUPERGROUP,
	TELEGRAM_CHAT_TYPE_COUNT
} telegram_chat_type_t;

/** This object represents a chat. */
typedef struct
{
	telegram_int_t id; /** 	Unique identifier for this chat. */
	const char *title; /**  Opt. Title of the chat  */ 
	telegram_chat_type_t type; /** Type of the chat */
} telegram_chat_t;

/** This object represents a message. */
typedef struct
{
	telegram_int_t   id;        /** Unique message identifier inside this chat */
	telegram_user_t  *from;     /** NULL in case of channel posts */
	const char       *text;     /** Text of the message */
	telegram_int_t   timestamp; /** Unix timestamp of the message */
	telegram_chat_t  *chat;     /** Conversation the message belongs to */
	telegram_user_t  *forward_from; /** Opt. For forwarded messages, sender of the original message */
	telegram_chat_t  *forward_from_chat; /** Opt. For forwarded messages, chat of the original message */
} telegram_chat_message_t;

/** This object represents an incoming callback query from a callback button in an inline keyboard. */
typedef struct
{
	const char                   *id;         /** Unique identifier for this query */
	telegram_user_t              *from;       /** Sender */
	const char      			 *data;       /** Opt. Data associated with the callback button. */
	telegram_chat_message_t      *message;    /** Opt. Message that was send to user */
} telegram_chat_callback_t;

/** This object represents an incoming update. */
typedef struct
{
	telegram_int_t               id;                   /** The update‘s unique identifier. */
	telegram_chat_message_t      *message;             /** Opt. New incoming message */
	telegram_chat_message_t      *edited_message;      /** Opt. New version of a message that is known to the bot and was edited */
	telegram_chat_message_t      *channel_post;        /** Opt. Message that was send to user */
	telegram_chat_message_t		 *edited_channel_post; /** Opt. New version of a channel post that is known to the bot and was edited */
	telegram_chat_callback_t     *callback_query;      /** New incoming callback query */
} telegram_update_t;

typedef void(*telegram_on_msg_cb_t)(void *teleCtx, telegram_update_t *info);

void telegram_kbrd(void *teleCtx_ptr, telegram_int_t chat_id, const char *message, telegram_kbrd_t *kbrd);
void telegram_send_text_message(void *teleCtx_ptr, telegram_int_t chat_id, const char *message);
void *telegram_init(const char *token, telegram_on_msg_cb_t cb);
void telegram_stop(void *teleCtx);
#endif // TELEGRAM_H