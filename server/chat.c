#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
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
static struct chat_client *find_match_in_room(struct waiting_room *room);
static void handle_client_match(struct chat_client *chat_client);
static void match_clients(struct chat_client *client1, struct chat_client *client2);
static int add_client_to_waiting_room(struct chat_context *ctx, struct chat_client *client);
static int gender_string_to_int(const char *g);

// websocket client callbacks
static void client_read(ws_client_t *client, struct ws_data *data);
static void client_close(ws_client_t *client);
// end websocket client callbacks

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

	// init clients lock
	pthread_mutex_init(&ctx->clients_lock, NULL);

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

	/*
	 * 1. MALE : MALE
	 */
	room = malloc(sizeof(struct waiting_room));
	if (!room) {
		ret = -ENOMEM;
		goto err;
	}
	room->gender = GENDER_MALE;
	room->looking_for = GENDER_MALE;
	room->queue = NULL;
	// queue lock
	pthread_mutex_init(&room->queue_lock, NULL);

	ctx->waiting_rooms[ctx->waiting_rooms_len++] = room;

	/*
	 * 2. MALE : FEMALE
	 */
	room = malloc(sizeof(struct waiting_room));
	if (!room) {
		ret = -ENOMEM;
		goto err;
	}
	room->gender = GENDER_MALE;
	room->looking_for = GENDER_FEMALE;
	room->queue = NULL;
	// queue lock
	pthread_mutex_init(&room->queue_lock, NULL);

	ctx->waiting_rooms[ctx->waiting_rooms_len++] = room;

	/*
	 * 3. FEMALE : FEMALE
	 */
	room = malloc(sizeof(struct waiting_room));
	if (!room) {
		ret = -ENOMEM;
		goto err;
	}
	room->gender = GENDER_FEMALE;
	room->looking_for = GENDER_FEMALE;
	room->queue = NULL;
	// queue lock
	pthread_mutex_init(&room->queue_lock, NULL);

	ctx->waiting_rooms[ctx->waiting_rooms_len++] = room;

	/*
	 * 4. FEMALE : MALE
	 */
	room = malloc(sizeof(struct waiting_room));
	if (!room) {
		ret = -ENOMEM;
		goto err;
	}
	room->gender = GENDER_FEMALE;
	room->looking_for = GENDER_MALE;
	room->queue = NULL;
	// queue lock
	pthread_mutex_init(&room->queue_lock, NULL);

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
	pthread_t thread;

	struct chat_thread_arg *arg = malloc(sizeof(struct chat_thread_arg));
	if (!arg) {
		// TODO: free client, close connection etc
		return;
	}

	arg->client = client;
	arg->chat_context = ctx;

    pthread_create(&thread, NULL, chat_client_thread, arg);
	pthread_join(thread, NULL);
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

			chat_cli->registered = 0;
			chat_cli->client = client;
			chat_cli->chat_context = ctx;
			chat_cli->pair = NULL;
			chat_cli->room = NULL;

			printf("Client registered - IP: %s, port: %d...\n", chat_cli->client->ip, chat_cli->client->port);

			client->owner = (void *)chat_cli;
			client->ops.read = client_read;
			client->ops.close = client_close;

			ret = register_client(ctx, chat_cli);

			if (ret) {
				ws_client_close(client);
				/*
				 * in this case we can call ws_client_free
				 * immediatly after close because we don`t have 
				 * and SSL_read ops in progress
				 */
				ws_client_free(client);
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

			handle_client_match(chat_client);
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
					printf("Msg from %s: %s\n", chat_client->client->ip, text_str);
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

static void handle_client_match(struct chat_client *chat_client)
{
	struct chat_client *matched_cli = NULL;
	if ((matched_cli = find_match(chat_client->chat_context, chat_client)) != NULL) {
		match_clients(chat_client, matched_cli);
	}
	else {
		add_client_to_waiting_room(chat_client->chat_context, chat_client);
	}
}

static void match_clients(struct chat_client *client1, struct chat_client *client2)
{
	client2->pair = client1;
	client1->pair = client2;

	printf("Match found between clients with IP-s: %s <=> %s...\n", client2->client->ip, client1->client->ip);

	char *resp_str = "{\"command\":\"match-found\"}";
	int resp_str_len = strlen(resp_str);

	ws_client_write_text(client1->client, resp_str, resp_str_len);
	ws_client_write_text(client2->client, resp_str, resp_str_len);
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

	return find_match_in_room(room);
}

static struct chat_client *find_match_in_room(struct waiting_room *room)
{
	struct chat_client *found_cli = NULL;
	struct list_node *found_node = NULL;

	if (!room) 
		goto end;

	pthread_mutex_lock(&room->queue_lock);

	if (!room->queue)
		goto end;

	// here we pick the first node (aka the first client in waiting queue)
	found_node = room->queue;
	found_cli = found_node->data;
	list_remove_node(&room->queue, found_node);

	if (found_cli) {
		found_cli->room = NULL;
	}

end:
	pthread_mutex_unlock(&room->queue_lock);
	return found_cli;
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

	pthread_mutex_lock(&room->queue_lock);
	struct list_node *node = list_add_node(&room->queue, client);
	pthread_mutex_unlock(&room->queue_lock);

	if (!node) {
		fprintf(stderr, "Error adding client with IP: %s to room (Gender: %d, looking for: %d)!\n", 
			client->client->ip, room->gender, room->looking_for);
		return -1;
	}

	client->room = room;

	return 0;
}

static int register_client(struct chat_context *ctx, struct chat_client *client)
{
	pthread_mutex_lock(&ctx->clients_lock);
	struct list_node *ret = list_add_node(&ctx->clients_head, client);
	pthread_mutex_unlock(&ctx->clients_lock);

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


	// removing client from main clients stack
	pthread_mutex_lock(&ctx->clients_lock);

	node = list_get_data_node(&ctx->clients_head, client);
	if (node) 
		list_remove_node(&ctx->clients_head, node);

	pthread_mutex_unlock(&ctx->clients_lock);

	// removing client from waiting room (if it is in one)
	if (client->room) {
		pthread_mutex_lock(&client->room->queue_lock);

		node = list_get_data_node(&client->room->queue, client);
		if (node) 
			list_remove_node(&client->room->queue, node);

		pthread_mutex_unlock(&client->room->queue_lock);
	}
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

