#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

/*
 * TODO: for the moment our max read buffer
 * can be up to 70KB (payload len 126 = 65535 bytes)
 */
#define MAX_WS_BUFFER_LEN 71680 
#define MAX_WS_PAYLOAD_LEN 65535

#define WEBSOCKET_MAGIC "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

typedef struct ws_client ws_client_t;

enum ws_client_statuses {
	CLIENT_CONNECTED=1,
	CLIENT_DISCONNECTED
};

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
	SSL *ssl;
	int ssl_error;
	struct sockaddr_in addr;
	socklen_t addr_len;
	char ip[INET_ADDRSTRLEN];
	int port;
	enum ws_client_statuses status;
	struct http_header **headers;
	int headers_len;
	struct ws_client_ops ops;
	void *owner;
	pthread_mutex_t lock;
};

struct ws_frame {
	int is_fin;
	int opcode;
	int is_masked;
	uint64_t payload_len;
	uint8_t masking_key[4];
};

int ws_client_handle(ws_client_t *client);
int ws_client_do_handshake(ws_client_t *client);

int ws_client_write_text(ws_client_t *client, char *text, uint64_t len);

int ws_client_close(ws_client_t *client);
void ws_client_free(ws_client_t *client);


#endif // WS_CLIENT_H