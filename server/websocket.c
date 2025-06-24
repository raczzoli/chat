#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "websocket.h"
#include "base64.h"

static int parse_http_header(wss_client_t *client, char *buffer);
static char *gen_resp_sec_key(char *req_key);

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
	char *wss_req_key;
	char *wss_resp_key;
	int ret = 0;

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

	ret = parse_http_header(client, buffer);

	if (ret)
		goto err;

	for (int i=0;i<client->headers_len;i++) {
		struct http_header *hdr = client->headers[i];

		if (hdr->key && strstr(hdr->key, "Sec-WebSocket-Key") != NULL) {
			wss_req_key = hdr->value;
			break;
		}
	}

	if (!wss_req_key) 
		goto err;

	wss_resp_key = gen_resp_sec_key(wss_req_key);

	return client;

err:
	if (client) {
		wss_free_client(client);
		client = NULL;
	}

	return NULL;
}

static int parse_http_header(wss_client_t *client, char *buffer)
{
	struct http_header *header;
	char *line;
	char *colon, *val;

	line = strtok(buffer, "\r\n");

	while (line != NULL) {
		colon = strchr(line, ':');

		if (colon) {
			header = malloc(sizeof(struct http_header));
			if (!header)
				return -ENOMEM;

			// key
			int key_len = colon - line;
			header->key = strndup(line, key_len);

			val = colon + 1; // ":"

			while (*val == ' ')
				val++;

			// value
			header->value = strndup(val, strlen(val));

			if (!client->headers)
				client->headers = calloc(1, sizeof(struct http_header *));
			else 
				client->headers = realloc(client->headers, (client->headers_len+1) * sizeof(struct http_header *));

			if (!client->headers) {
				free(header->key);
				free(header->value);
				free(header);

				return -ENOMEM;
			}

			client->headers[client->headers_len++] = header;
		}

		line = strtok(NULL, "\r\n");
	}

	return 0;
}

static char *gen_resp_sec_key(char *req_key)
{
	char *input = "hello world";
    char *str = base64_encode(input);
    if (str) {
        printf("base64: %s\n", str);
        free(str);
    }

	return NULL;
}

void wss_free_client(wss_client_t *client)
{
	free(client);
	// TODO: free headers;
}