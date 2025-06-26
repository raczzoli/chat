#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "r89.h"
#include "chat.h"
#include "list.h"

static int register_client(struct chat_context *ctx, struct chat_client *client);

// websocket client callbacks
static void client_read(ws_client_t *client, struct ws_data *data);
static void client_close(ws_client_t *client);


struct chat_context *chat_create()
{
	struct ws_server_config config;
	struct chat_context *ctx = NULL;
	
	ctx = malloc(sizeof(struct chat_context));

	if (!ctx) {
		fprintf(stderr, "Error allocating memory for chat_context struct!\n");
		return NULL;
	}

	config.port = WS_PORT;
	ctx->server = ws_server_create(config);

	if (!ctx->server) {
		chat_free(ctx);
		return NULL;
	}

	return ctx;
}

void chat_init(struct chat_context *ctx)
{
	int ret = 0;
	ws_client_t *client = NULL;

	while (1) {
		client = ws_server_accept(ctx->server);

		if (client) {
			struct chat_client *chat_cli = malloc(sizeof(struct chat_client));

			if (!chat_cli) {
				// TODO close client connection
				fprintf(stderr, "Error allocating memory for chat_client struct!\n");
				return;
			}

			printf("Client registered - IP: %s, port: %d...\n", client->ip, client->port);

			chat_cli->client = client;
			chat_cli->chat_context = ctx;

			client->owner = (void *)chat_cli;
			client->ops.read = client_read;
			client->ops.close = client_close;

			ret = register_client(ctx, chat_cli);
			if (ret) {
				// TODO close client connection
				free(chat_cli);
				return;
			}

			ws_client_handle(client);
		}
	}
}

static void client_read(ws_client_t *client, struct ws_data *data)
{
	printf("Data from client IP: %s... payload len: %ld, data: %s\n", client->ip, data->payload_len, data->payload);

	// test
	//struct chat_client *chat_cli = (struct chat_client *)client->owner;

	//json_error_t error;
    //json_t *root = json_loads(json_text, 0, &error);

	if (data) {
		// dummy if to silence compiler for now
	}
}

static int register_client(struct chat_context *ctx, struct chat_client *client)
{
	struct list_node *ret = list_add(&ctx->clients_head, client);

	if (!ret) {
		// TODO - maybe close the connection
		fprintf(stderr, "Error adding client with IP: %s to clients list!\n", client->client->ip);
		return -1;
	}

	printf("Client with IP: %s added to clients list...\n", client->client->ip);

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

