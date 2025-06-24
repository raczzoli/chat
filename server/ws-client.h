#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include <arpa/inet.h>

typedef struct ws_client ws_client_t;

struct http_header {
	char *key;
	char *value;
};

struct ws_client_ops {
	void (*read)(ws_client_t *client, char *data);
	void (*close)(ws_client_t *client);
};

struct ws_client {
	int fd;
	struct sockaddr_in addr;
	socklen_t addr_len;
	char ip[INET_ADDRSTRLEN];
	int port;
	struct http_header **headers;
	int headers_len;
	struct ws_client_ops ops;
};

void ws_client_init(ws_client_t *client);
void ws_client_free(ws_client_t *client);


#endif // WS_CLIENT_H