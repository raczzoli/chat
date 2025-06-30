#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include "r89.h"
#include "chat.h"
#include "list.h"
#include "jansson.h"

static void start_client_thread(struct chat_context *ctx, struct chat_client *client);
static void *chat_client_thread(void *arg);

static int init_waiting_rooms(struct chat_context *ctx);
static int register_client(struct chat_context *ctx, struct chat_client *client);
static struct chat_client *find_match(struct chat_context *ctx, struct chat_client *client);
static int add_client_to_waiting_room(struct chat_context *ctx, struct chat_client *client);
static int gender_string_to_int(const char *g);

// websocket client callbacks
static void client_read(ws_client_t *client, struct ws_data *data);
static void client_close(ws_client_t *client);
static void remove_client_from_stacks(struct chat_context *ctx, struct chat_client *client);
static void free_client(struct chat_client *client);


struct chat_context *chat_create(struct ws_server_config config)
{
	struct chat_context *ctx = NULL;
	
	ctx = malloc(sizeof(struct chat_context));

	if (!ctx) {
		fprintf(stderr, "Error allocating memory for chat_context struct!\n");
		return NULL;
	}

	ctx->clients_head = NULL;
	ctx->waiting_rooms = NULL;
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
	room->queue = NULL;
	ctx->waiting_rooms[ctx->waiting_rooms_len++] = room;

	// MALE : FEMALE
	room = malloc(sizeof(struct waiting_room));
	if (!room) {
		ret = -ENOMEM;
		goto err;
	}
	room->gender = GENDER_MALE;
	room->looking_for = GENDER_FEMALE;
	room->queue = NULL;
	ctx->waiting_rooms[ctx->waiting_rooms_len++] = room;

	// FEMALE : FEMALE
	room = malloc(sizeof(struct waiting_room));
	if (!room) {
		ret = -ENOMEM;
		goto err;
	}
	room->gender = GENDER_FEMALE;
	room->looking_for = GENDER_FEMALE;
	room->queue = NULL;
	ctx->waiting_rooms[ctx->waiting_rooms_len++] = room;

	// FEMALE : MALE
	room = malloc(sizeof(struct waiting_room));
	if (!room) {
		ret = -ENOMEM;
		goto err;
	}
	room->gender = GENDER_FEMALE;
	room->looking_for = GENDER_MALE;
	room->queue = NULL;
	ctx->waiting_rooms[ctx->waiting_rooms_len++] = room;

	goto end;

err:
	if (ctx->waiting_rooms) 
		free(ctx->waiting_rooms);

	ctx->waiting_rooms_len = 0;

end:	
	return ret;
}

static void start_client_thread(struct chat_context *ctx, struct chat_client *client)
{
	pthread_t *thread = (pthread_t *) malloc(sizeof(pthread_t));

	if (!thread) {
		// TODO: free client, close connection etc
		return;
	}

	struct chat_thread_arg *arg = malloc(sizeof(struct chat_thread_arg));
	if (!arg) {
		// TODO: free client, close connection etc
		return;
	}

	arg->client = client;
	arg->chat_context = ctx;

    pthread_create(thread, NULL, chat_client_thread, arg);
}

static void *chat_client_thread(void *arg)
{
	struct chat_thread_arg *t_arg = arg;

	ws_client_handle(t_arg->client->client);

	free(t_arg);

	return NULL;
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

			chat_cli->registered = 0;
			chat_cli->client = client;
			chat_cli->chat_context = ctx;
			chat_cli->pair = NULL;

			client->owner = (void *)chat_cli;
			client->ops.read = client_read;
			client->ops.close = client_close;

			ret = register_client(ctx, chat_cli);
			if (ret) {
				// TODO close client connection
				free(chat_cli);
				return;
			}

			start_client_thread(ctx, chat_cli);
		}
	}
}

static void client_read(ws_client_t *client, struct ws_data *data)
{
	json_t *root;
	json_error_t error;

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
				goto register_err;
			}

			chat_client->gender = gender_string_to_int( json_string_value(gender) );
			chat_client->looking_for = gender_string_to_int( json_string_value(looking_for) );
			
			if (chat_client->gender < 1 || chat_client->looking_for < 1) {
				fprintf(stderr, "Invalid gender and/or looking_for values in client data!\n");
				goto register_err;
			}

			char *resp_str = "{\"command\":\"register-ok\"}";
			ws_client_write_text(client, resp_str, strlen(resp_str));

			struct chat_client *matched_cli = NULL;
			if ((matched_cli = find_match(chat_client->chat_context, chat_client)) != NULL) {
				matched_cli->pair = chat_client;
				chat_client->pair = matched_cli;

				printf("Match found between clients with IP-s: %s <=> %s...\n", matched_cli->client->ip, chat_client->client->ip);

				char *resp_str = "{\"command\":\"match-found\"}";
				int resp_str_len = strlen(resp_str);

				ws_client_write_text(chat_client->client, resp_str, resp_str_len);
				ws_client_write_text(matched_cli->client, resp_str, resp_str_len);
			}
			else {
				add_client_to_waiting_room(chat_client->chat_context, chat_client);
			}
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

			/*
			 * message from client
			 */
			if (strcmp(command_str, "message") == 0) {
				json_t *text = json_object_get(params, "text");
				if ( (!text || !json_is_string(text))) {

					fprintf(stderr, "Invalid message!\n");
					goto end;
				}

				if (chat_client->pair) {
					const char *text_str = json_string_value(text);
					int resp_len = strlen(text_str) + 100;
					char *resp_str = malloc(resp_len);

					if (!resp_str)
						goto end;

					snprintf(resp_str, resp_len, "{\"command\":\"message\", \"params\":{\"source\":\"partner\", \"text\":\"%s\"}}", text_str);

					ws_client_write_text(chat_client->pair->client, resp_str, strlen(resp_str));
					free(resp_str);
				}
			}
		}
	}

	goto end;

register_err: {
	char *resp_str = "{\"command\":\"register-err\"}";
	ws_client_write_text(client, resp_str, strlen(resp_str));
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

static struct chat_client *find_match(struct chat_context *ctx, struct chat_client *client)
{
	struct waiting_room *room = NULL;

	for (int i=0;i<ctx->waiting_rooms_len;i++) {
		struct waiting_room *r = ctx->waiting_rooms[i];
		if (r->gender == client->looking_for && r->looking_for == client->gender) {
			room = r;
			break;
		}
	}

	if (!room || !room->queue)
		return NULL;

	// we have a match
	struct list_node *first_node = room->queue;
	struct chat_client *first_cli = first_node->data;
	list_remove_node(&room->queue, first_node);

	if (first_cli) {
		first_cli->room = NULL;
		return first_cli;
	}

	return NULL;
}

static int add_client_to_waiting_room(struct chat_context *ctx, struct chat_client *client)
{
	struct waiting_room *room = NULL;

	for (int i=0;i<ctx->waiting_rooms_len;i++) {
		struct waiting_room *r = ctx->waiting_rooms[i];
		if (r->gender == client->gender && r->looking_for == client->looking_for) {
			room = r;
			break;
		}
	}

	if (!room)
		return -1;

	struct list_node *node = list_add_node(&room->queue, client);

	if (!node) {
		fprintf(stderr, "Error adding client with IP: %s to room (Gender: %d, looking for: %d)!\n", 
			client->client->ip, room->gender, room->looking_for);
		return -1;
	}

	client->room = room;

	//printf("Client with IP: %s added to waiting room succesfuly (Gender: %d, looking for: %d)...\n", 
	//		client->client->ip, room->gender, room->looking_for);

	return 0;
}

static int register_client(struct chat_context *ctx, struct chat_client *client)
{
	struct list_node *ret = list_add_node(&ctx->clients_head, client);

	if (!ret) {
		// TODO - maybe close the connection
		fprintf(stderr, "Error adding client with IP: %s to clients list!\n", client->client->ip);
		return -1;
	}

	//printf("Client with IP: %s added to clients list...\n", client->client->ip);
	return 0;
}

static void client_close(ws_client_t *client)
{
	struct chat_client *chat_client = client->owner;
	struct chat_context *ctx = chat_client->chat_context;

	if (chat_client->pair) {
		struct chat_client *pair = chat_client->pair;

		char *resp_str = "{\"command\":\"match-left-chat\"}";
		int resp_str_len = strlen(resp_str);

		// here we delete the pairing between the two clients
		chat_client->pair = NULL;
		pair->pair = NULL;

		ws_client_write_text(pair->client, resp_str, resp_str_len);
		ws_client_close(pair->client);
	}

	printf("Client with IP: %s closed the connection...\n", client->ip);

	remove_client_from_stacks(ctx, chat_client);
	free_client(chat_client);
}

static void remove_client_from_stacks(struct chat_context *ctx, struct chat_client *client)
{
	struct list_node *node = NULL;

	node = list_get_data_node(&ctx->clients_head, client);
	if (node) {
		list_remove_node(&ctx->clients_head, node);
		printf("Client with IP: %s removed from clients stack...\n", client->client->ip);
	}

	if (client->room) {
		node = list_get_data_node(&client->room->queue, client);
		if (node) {
			list_remove_node(&client->room->queue, node);
			printf("Client with IP: %s removed from waiting room...\n", client->client->ip);
		}
	}

	//ctx->waiting_rooms = NULL;
}

static void free_client(struct chat_client *client)
{
	if (client)
		free(client);
}

void chat_free(struct chat_context *ctx)
{
	free(ctx);
}

