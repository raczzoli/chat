#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <arpa/inet.h>

#define LISTEN_BACKLOG 128
#define MAX_BUFFER_LEN 1024

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
} wss_client_t;

wss_ctx_t *wss_create(struct wss_config config);
struct wss_client *wss_accept(wss_ctx_t *server);

#endif