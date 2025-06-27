#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "r89.h"
#include "chat.h"
#include "list.h"
#include "jansson.h"

static int init_waiting_rooms(struct chat_context *ctx);
static int register_client(struct chat_context *ctx, struct chat_client *client);
static int find_match_or_add_to_waiting_queue(struct chat_client *client);
static int add_client_to_waiting_room(struct chat_client *client, struct waiting_room *room);
static int gender_string_to_int(const char *g);

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

	init_waiting_rooms(ctx);

	return ctx;
}

static int init_waiting_rooms(struct chat_context *ctx)
{
	int ret = 0;
	struct waiting_room *room;

	if (ctx->waiting_rooms) 
		return 0;

	ctx->waiting_rooms_len = 0;
	ctx->waiting_rooms = calloc(4, sizeof(struct waiting_room *));

	// MALE : MALE
	room = malloc(sizeof(struct waiting_room));
	if (!room) {
		ret = -ENOMEM;
		goto err;
	}
	room->gender = GENDER_MALE;
	room->looking_for = GENDER_MALE;
	ctx->waiting_rooms[ctx->waiting_rooms_len++] = room;

	// MALE : FEMALE
	room = malloc(sizeof(struct waiting_room));
	if (!room) {
		ret = -ENOMEM;
		goto err;
	}
	room->gender = GENDER_MALE;
	room->looking_for = GENDER_FEMALE;
	ctx->waiting_rooms[ctx->waiting_rooms_len++] = room;

	// FEMALE : FEMALE
	room = malloc(sizeof(struct waiting_room));
	if (!room) {
		ret = -ENOMEM;
		goto err;
	}
	room->gender = GENDER_FEMALE;
	room->looking_for = GENDER_FEMALE;
	ctx->waiting_rooms[ctx->waiting_rooms_len++] = room;

	// FEMALE : MALE
	room = malloc(sizeof(struct waiting_room));
	if (!room) {
		ret = -ENOMEM;
		goto err;
	}
	room->gender = GENDER_FEMALE;
	room->looking_for = GENDER_MALE;
	ctx->waiting_rooms[ctx->waiting_rooms_len++] = room;

	goto end;

err:
	if (ctx->waiting_rooms) 
		free(ctx->waiting_rooms);

	ctx->waiting_rooms_len = 0;

end:	
	return ret;
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
	json_t *root;
	json_error_t error;

	//printf("Data from client IP: %s... payload len: %ld, data: %s\n", client->ip, data->payload_len, data->payload);

	struct chat_client *chat_client = client->owner;

	/*
	 * if the client is not yet registered (sent the register command with the 
	 * criterias the client is looking for) and the received ws data is not text
	 * we ignore the message
	 */	
	if (!chat_client->registered && !data->is_text) {
		/*
		 * TODO: we should see what to do when this happens:
		 * disconnect the client or just ignore the message, or 
		 * maybe send back an error
		 */
		return;
	}

	if (data->is_text) {
		root = json_loads(data->payload, 0, &error);

		if (!root) {
			fprintf(stderr, "Invalid json from client!\n");
			return;
		}

		json_t *command = json_object_get(root, "command");
		json_t *params = json_object_get(root, "params");
		const char *command_str = NULL;

		if (!command || !json_is_string(command) || !params) {
			fprintf(stderr, "Invalid \"command\" and/or \"params\" JSON keys!\n");
			goto end;
		}

		command_str = json_string_value(command);
		if (strcmp(command_str, "register") == 0) {
			if (chat_client->registered)
				goto end;
		
			chat_client->registered = 1;

			json_t *gender = json_object_get(params, "gender");
    		json_t *looking_for = json_object_get(params, "looking_for");

			if ( (!gender || !json_is_string(gender)) ||
				!looking_for || !json_is_string(looking_for) ) {

				fprintf(stderr, "Invalid \"gender\" and/or \"looking_for\" JSON keys!\n");
				goto end;
			}

			chat_client->gender = gender_string_to_int( json_string_value(gender) );
			chat_client->looking_for = gender_string_to_int( json_string_value(looking_for) );

			if (chat_client->gender < 1 || chat_client->looking_for < 1) {
				fprintf(stderr, "Invalid gender and/or looking_for values in client data!\n");
				goto end;
			}

			find_match_or_add_to_waiting_queue(chat_client);
		}
		else {
			if (!chat_client->registered) {
				/*
				 * TODO: same case like at the beginning, but this time the 
				 * payload is text, but since our client was not yet registered
				 * we expected the "register" command to be sent
				 */
				goto end;
			}
		}
	}

end:
	if (root)
		json_decref(root);

	return;
}

static int gender_string_to_int(const char *g)
{
	if (g) {
		if (strcmp(g, "M") == 0)
			return GENDER_MALE;
		if (strcmp(g, "F") == 0)
			return GENDER_FEMALE;
	}

	return 0;
}

static int find_match_or_add_to_waiting_queue(struct chat_client *client)
{
	struct chat_context *ctx = client->chat_context;
	struct waiting_room *room = NULL;

	for (int i=0;i<ctx->waiting_rooms_len;i++) {
		struct waiting_room *r = ctx->waiting_rooms[i];
		if (r->gender == client->gender && r->looking_for == client->looking_for) {
			room = r;
			break;
		}
	}

	if (!room) {
		fprintf(stderr, "No waiting room for client criteria!\n");
		return -1;
	}

	if (!room->queue) 
		return add_client_to_waiting_room(client, room);

	// search in queue

	return 0;
}

static int add_client_to_waiting_room(struct chat_client *client, struct waiting_room *room)
{
	struct list_node *node = list_add(&room->queue, client);

	if (!node) {
		fprintf(stderr, "Error adding client with IP: %s to room (Gender: %d, looking for: %d)!\n", 
			client->client->ip, room->gender, room->looking_for);
		return -1;
	}

	printf("Client with IP: %s added to waiting room succesfuly (Gender: %d, looking for: %d)...\n", 
			client->client->ip, room->gender, room->looking_for);

	return 0;
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

