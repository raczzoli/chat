#ifndef CHAT_H
#define CHAT_H

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

	int connected :1;
	int accepted :1;
	int gender;
	int looking_for;
	int min_age;
	int max_age;
};

struct chat_context {
	ws_server_t *server;

	struct chat_client **clients;
	int clients_len;
};

struct chat_context *chat_create();
void chat_init(struct chat_context *ctx);

void chat_free(struct chat_context *ctx);

#endif // CHAT_H