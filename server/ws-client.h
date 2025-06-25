#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include <arpa/inet.h>

/*
 * TODO: for the moment our max read buffer
 * can be up to 70KB (payload len 126 = 65535 bytes)
 */
#define MAX_WS_BUFFER_LEN 71680 
#define MAX_TEMP_PAYLOAD_LEN 65535


typedef struct ws_client ws_client_t;

struct http_header {
	char *key;
	char *value;
};

struct ws_data {
	int type;
	int is_text :1;
	char *payload;
	uint64_t payload_len;
};

struct ws_client_ops {
	void (*read)(ws_client_t *client, struct ws_data *data);
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
	void *owner;
};

struct ws_frame {
	int is_fin;
	int opcode;
	int is_masked;
	uint64_t payload_len;
	uint8_t masking_key[4];
};

int ws_client_handle(ws_client_t *client);
void ws_client_free(ws_client_t *client);


#endif // WS_CLIENT_H