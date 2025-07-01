#ifndef CHAT_H
#define CHAT_H

#include <stdbool.h>

#include "ws-client.h"
#include "ws-server.h"

#define CLIENTS_ALLOC_CHUNK 100

enum genders {
	GENDER_MALE=1,
	GENDER_FEMALE
};

struct chat_client {
	ws_client_t *client;
	struct chat_context *chat_context;

	bool registered;
	bool is_bot;
	
	int gender;
	int looking_for;

	struct waiting_room *room;
	struct chat_client *pair;
};

struct chat_context {
	ws_server_t *server;

	struct list_node *clients_head;
	struct waiting_room **waiting_rooms;
	int waiting_rooms_len;
};

struct waiting_room {
	int gender;
	int looking_for;

	struct list_node *queue;
};

struct chat_thread_arg {
	struct chat_client *client;
	struct chat_context *chat_context;
};

struct chat_context *chat_create(struct ws_server_config config);
void chat_init(struct chat_context *ctx);

void chat_free(struct chat_context *ctx);

#endif // CHAT_H