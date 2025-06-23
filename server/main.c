#include <stdio.h>
#include "websocket.h"

int main()
{
	struct wss_config config;
	wss_ctx_t *server = NULL;
	wss_client_t *client;

	config.port = 8081;
	server = wss_create(config);

	if (!server) 
		return -1;

	printf("Websocket server created successfuly...\n");
	printf("Accepting connections...\n");

	while (1) {
		client = wss_accept(server);

		if (client) {
			printf("Client connected... IP: %s, port: %d\n", client->ip, client->port);
		}
	}

	return 0;
}