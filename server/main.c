#include <stdio.h>

#include "chat.h"
#include "ws-server.h"
#include "ws-client.h"

static void client_accepted(ws_client_t *client);
static void client_read(ws_client_t *client, struct ws_data *data);
static void client_close(ws_client_t *client);

int main()
{
	struct ws_server_config config;
	ws_server_t *server = NULL;
	ws_client_t *client;

	config.port = 8081;
	server = ws_server_create(config);

	if (!server) 
		return -1;

	printf("Websocket server created successfuly...\n");
	printf("Accepting connections...\n");

	while (1) {
		client = ws_server_accept(server);

		if (client) 
			client_accepted(client);
	}

	return 0;
}

static void client_accepted(ws_client_t *client)
{
	printf("Client connected... IP: %s, port: %d\n", client->ip, client->port);

	client->ops.read = client_read;
	client->ops.close = client_close;

	ws_client_init(client);
}

static void client_read(ws_client_t *client, struct ws_data *data)
{
	printf("Data from client IP: %s... payload len: %ld, data: %s\n", client->ip, data->payload_len, data->payload);
	if (data) {
		// dummy if to silence compiler for now
	}
}

static void client_close(ws_client_t *client)
{
	printf("Client with IP: %s closed the connection...\n", client->ip);
}