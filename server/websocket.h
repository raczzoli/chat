#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <arpa/inet.h>

#define LISTEN_BACKLOG 128
#define MAX_BUFFER_LEN 1024
#define WEBSOCKET_MAGIC "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

struct wss_config {
	int port;
};

typedef struct wss_ctx {
	int fd;
	struct sockaddr_in addr;
} wss_ctx_t;

typedef struct wss_client {
	int fd;
	struct sockaddr_in addr;
	socklen_t addr_len;
	char ip[INET_ADDRSTRLEN];
	int port;
	struct http_header **headers;
	int headers_len;
} wss_client_t;

struct http_header {
	char *key;
	char *value;
};

wss_ctx_t *wss_create(struct wss_config config);
struct wss_client *wss_accept(wss_ctx_t *server);

void wss_free_client(wss_client_t *client);

#endif