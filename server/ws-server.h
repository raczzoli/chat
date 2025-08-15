#ifndef WS_SERVER_H
#define WS_SERVER_H

#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "ws-client.h"

#define LISTEN_BACKLOG 128

struct ws_server_config {
	int port;
	char *ssl_cert_path;
	char *ssl_key_path;
};

typedef struct ws_server {
	SSL_CTX *ssl_ctx;
	int fd;
	struct sockaddr_in addr;
} ws_server_t;

ws_server_t *ws_server_create(struct ws_server_config config);
ws_client_t *ws_server_accept(ws_server_t *server);

#endif // WS_SERVER_H