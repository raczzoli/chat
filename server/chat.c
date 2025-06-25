#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "r89.h"
#include "chat.h"

static int register_client(struct chat_context *ctx, struct chat_client *client);

// websocket client callbacks
static void client_read(ws_client_t *client, struct ws_data *data);
static void client_close(ws_client_t *client);


struct chat_context *chat_init()
{
	struct ws_server_config config;
	struct chat_context *ctx = NULL;
	
	ctx = (struct chat_context *)malloc(sizeof(struct chat_context));

	if (!ctx) {
		fprintf(stderr, "Error allocating memory for chat_context struct!\n");
		return NULL;
	}

	ctx->clients = NULL;
	ctx->clients_len = 0;

	config.port = WS_PORT;
	ctx->server = ws_server_create(config);

	if (!ctx->server) {
		chat_free(ctx);
		return NULL;
	}

	return ctx;
}

void chat_accept_connections(struct chat_context *ctx)
{
	int ret = 0;
	ws_client_t *client = NULL;

	while (1) {
		client = ws_server_accept(ctx->server);

		if (client) {
			struct chat_client *c = (struct chat_client *)malloc(sizeof(struct chat_client));

			if (!c) {
				// TODO close client connection
				fprintf(stderr, "Error allocating memory for chat_client struct!\n");
				return;
			}

			ret = register_client(ctx, c);
			if (ret) {
				// TODO close client connection
				free(c);
				return;
			}

			printf("Client registered - IP: %s, port: %d...\n", client->ip, client->port);

			c->client = client;
			c->chat_context = ctx;

			client->owner = (void *)c;
			client->ops.read = client_read;
			client->ops.close = client_close;

			ws_client_handle(client);
		}
	}
}

static void client_read(ws_client_t *client, struct ws_data *data)
{
	printf("Data from client IP: %s... payload len: %ld, data: %s\n", client->ip, data->payload_len, data->payload);

	// test
	struct chat_client *cc = (struct chat_client *)client->owner;

	//json_error_t error;
    //json_t *root = json_loads(json_text, 0, &error);

	if (data) {
		// dummy if to silence compiler for now
	}
}

static int register_client(struct chat_context *ctx, struct chat_client *client)
{
	if (!ctx->clients) {
		ctx->clients = calloc(CLIENTS_ALLOC_CHUNK, sizeof(struct chat_client *));
	}
	else if (ctx->clients_len % CLIENTS_ALLOC_CHUNK == 0) {
		int size = ctx->clients_len + CLIENTS_ALLOC_CHUNK;
		ctx->clients = realloc(ctx->clients, size * sizeof(struct chat_client *));
	}

	if (!ctx->clients) {
		fprintf(stderr, "Error allocating memory for clients array!\n");
		return -ENOMEM;
	}

	ctx->clients[ctx->clients_len] = client;
	ctx->clients_len++;

	return 0;
}

static void client_close(ws_client_t *client)
{
	printf("Client with IP: %s closed the connection...\n", client->ip);
}

void chat_free(struct chat_context *ctx)
{
	free(ctx);
}

