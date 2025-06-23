#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "websocket.h"

wss_ctx_t *wss_create(struct wss_config config)
{
	wss_ctx_t *server = NULL;

	if (config.port <= 0) {
		fprintf(stderr, "Invalid server port (%d)!\n", config.port);
		return NULL;
	}

	server = (wss_ctx_t *)malloc(sizeof(wss_ctx_t));

	if (!server) {
		fprintf(stderr, "Error allocating memory for server object!\n");
		return NULL;
	}
	
    // Creating socket
    server->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->fd < 0) {
		fprintf(stderr, "Error creating socket!\n");
        goto err1;
    }

    // Set up address structure
    server->addr.sin_family = AF_INET;
    server->addr.sin_addr.s_addr = INADDR_ANY; // listening on all interfaces
    server->addr.sin_port = htons(config.port);

	int yes = 1;
	if (setsockopt(server->fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
		fprintf(stderr, "Error reusing the socket port!\n");
        goto err1;
	}

    // Bind to socket
    if (bind(server->fd, (struct sockaddr*)&server->addr, sizeof(server->addr)) < 0) {
		fprintf(stderr, "Error binding to socket!\n");
        goto err2;
    }

    // Start listening on socket
    if (listen(server->fd, LISTEN_BACKLOG) < 0) {
		fprintf(stderr, "Error listening on socket!\n");
        goto err2;
    }

	return server;

err2:
	close(server->fd);

err1:
	free(server);
	server = NULL;

	return NULL;
}

struct wss_client *wss_accept(wss_ctx_t *server)
{
	wss_client_t *client = NULL;
	char buffer[MAX_BUFFER_LEN];
	int bytes_read = 0;

	if (!server) {
		fprintf(stderr, "Invalid wss_ctx_t pointer passed to wss_accept()!\n");
		return NULL;
	}

	client = (wss_client_t *)malloc(sizeof(wss_client_t));

	if (!client) {
		fprintf(stderr, "Error allocating memory for websocket client!\n");
		return NULL;
	}

	client->addr_len = sizeof(struct sockaddr_in);
    client->fd = accept(server->fd, (struct sockaddr*)&client->addr, &client->addr_len);

    if (client->fd < 0) {
		fprintf(stderr, "Error accepting websocket client!\n");
		goto err;
    }

	inet_ntop(AF_INET, &(client->addr.sin_addr), client->ip, INET_ADDRSTRLEN);
    client->port = ntohs(client->addr.sin_port);

	bytes_read = recv(client->fd, buffer, MAX_BUFFER_LEN - 1, 0);

	if (bytes_read <= 0) {
		fprintf(stderr, "Error reading handshake request from client or the client unexpectedly closed the connection (recv return value: %d)!\n", bytes_read);
		goto err;
	}

	printf("Handshake request: \n\n%s", buffer);

	return client;

err:
	if (client) {
		free(client);
		client = NULL;
	}

	return NULL;
}

