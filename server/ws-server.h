#ifndef WS_SERVER_H
#define WS_SERVER_H

#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "ws-client.h"

#define LISTEN_BACKLOG 128
#define WEBSOCKET_MAGIC "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

struct ws_server_config {
	int port;
	char *ssl_cert_path;
	char *ssl_key_path;
};

typedef struct ws_server {
	SSL_CTX *ctx;
	int fd;
	struct sockaddr_in addr;
} ws_server_t;

ws_server_t *ws_server_create(struct ws_server_config config);
ws_client_t *ws_server_accept(ws_server_t *server);

#endif // WS_SERVER_H