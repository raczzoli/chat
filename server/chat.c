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
static int init_bot_matcher_thread(struct chat_client *chat_client);
static void *chat_client_thread(void *arg);

static int init_waiting_rooms(struct chat_context *ctx);
static int register_client(struct chat_context *ctx, struct chat_client *client);
static struct chat_client *find_match(struct chat_context *ctx, struct chat_client *client, int is_bot);
static struct chat_client *find_match_in_room(struct waiting_room *room, int is_bot);
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

static void *bot_matcher_thread(void *arg)
{
	struct chat_thread_arg *t_arg = arg;
	struct chat_client *client = t_arg->client;
	struct chat_context *ctx = client->chat_context;
	struct waiting_room *client_room = client->room;

	int seconds = (rand() % 4) + 1; // random number between 1 and 4
	sleep(seconds);

	/*
	 * here we check if while sleeping our client 
	 * got a match or not
	 */
	if (client->pair) 
		goto end;		

	/*
	 * if not, we call find_match again, but this time
	 * we look for bots, and pick the first one, and match
	 * out client with that bot
	 */
	struct chat_client *bot = find_match(ctx, client, 1);

	/*
	 * since we always have bots in waiting queues it shouldn`t happen
	 * for "bot" to be NULL, but still checking it for safety
	 */
	if (bot) {
		/*
		 * we check this only for safety, client_room shouldn`t be NULL, 
		 * since our client doesn`t yet have a match (client->room is only
		 * set to NULL when a client finds a match)
		 */
		if (client_room) { 
			struct list_node *cnode = list_get_data_node(&client_room->queue, client);
			list_remove_node(&client_room->queue, cnode);
		}
		
		match_clients(client, bot);
	}
	else {
		printf("No bot found in waiting room after sleep!\n");
	}

end:
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
			chat_cli->is_bot = strcmp(client->ip, "127.0.0.1") == 0;

			printf("%s registered - IP: %s, port: %d...\n", chat_cli->is_bot ? "BOT": "Client", chat_cli->client->ip, chat_cli->client->port);

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
	if ((matched_cli = find_match(chat_client->chat_context, chat_client, 0)) != NULL) {
		match_clients(chat_client, matched_cli);
	}
	else {
		add_client_to_waiting_room(chat_client->chat_context, chat_client);

		if (!chat_client->is_bot) {
			printf("Starting bot matcher thread...\n");
			init_bot_matcher_thread(chat_client);
		}
	}
}

static int init_bot_matcher_thread(struct chat_client *chat_client)
{
	pthread_t thread;

	struct chat_thread_arg *arg = malloc(sizeof(struct chat_thread_arg));
	if (!arg) 
		return -ENOMEM;

	arg->client = chat_client;
	
    pthread_create(&thread, NULL, bot_matcher_thread, arg);
	pthread_join(thread, NULL);

	return 0;
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

static struct chat_client *find_match(struct chat_context *ctx, struct chat_client *client, int is_bot)
{
	struct waiting_room *room = NULL;

	for (int i=0;i<ctx->waiting_rooms_len;i++) {
		struct waiting_room *r = ctx->waiting_rooms[i];
		if (r->gender == client->looking_for && r->looking_for == client->gender) {
			room = r;
			break;
		}
	}

	return find_match_in_room(room, is_bot);
}

static struct chat_client *find_match_in_room(struct waiting_room *room, int is_bot)
{
	if (!room || !room->queue)
		return NULL;

	// we have a match

	struct list_node *found_node = NULL;
	struct list_node *curr = room->queue;
	struct chat_client *item = NULL;

	do {
		item = curr->data;
		if (item->is_bot == is_bot) {
			found_node = curr;
			break;
		}

		curr = curr->next;
	}
	while (curr != room->queue);

	if (!found_node)
		return NULL;

	struct chat_client *found_cli = found_node->data;
	list_remove_node(&room->queue, found_node);

	if (found_cli) {
		found_cli->room = NULL;
		return found_cli;
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

	//printf("Client with IP: %s added to waiting room succesfuly ::: bot: %d (Gender: %d, looking for: %d)...\n", 
	//		client->client->ip, client->is_bot, room->gender, room->looking_for);

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
	if (node) 
		list_remove_node(&ctx->clients_head, node);

	if (client->room) {
		node = list_get_data_node(&client->room->queue, client);
		if (node) 
			list_remove_node(&client->room->queue, node);
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

