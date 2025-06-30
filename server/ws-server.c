#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <openssl/sha.h>

#include "r89.h"
#include "ws-server.h"
#include "base64.h"

static int parse_http_headers(ws_client_t *client, char *buffer);
static int handle_client_handshake(ws_client_t *client);
static int init_ssl_context(ws_server_t *server, struct ws_server_config config);

ws_server_t *ws_server_create(struct ws_server_config config)
{
	int ret = 0;
	ws_server_t *server = NULL;

	if (config.port <= 0) {
		fprintf(stderr, "Invalid server port (%d)!\n", config.port);
		return NULL;
	}

	server = (ws_server_t *)malloc(sizeof(ws_server_t));

	if (!server) {
		fprintf(stderr, "Error allocating memory for server object!\n");
		return NULL;
	}
	
	ret = init_ssl_context(server, config);

	if (ret) {
		free(server);
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
		fprintf(stderr, "Error binding to socket (%s)!\n", strerror(errno));
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

ws_client_t *ws_server_accept(ws_server_t *server)
{
	ws_client_t *client = NULL;
	int ret = 0;

	if (!server) {
		fprintf(stderr, "Invalid wss_ctx_t pointer passed to ws_server_accept()!\n");
		return NULL;
	}

	client = (ws_client_t *)malloc(sizeof(ws_client_t));

	if (!client) {
		fprintf(stderr, "Error allocating memory for websocket client!\n");
		return NULL;
	}

	pthread_mutex_init(&client->lock, NULL);

	client->ssl = NULL;
	client->ssl_error = 0;
	client->headers = NULL;
	client->headers_len = 0;
	client->addr_len = sizeof(struct sockaddr_in);
	client->status = CLIENT_CONNECTED;

	client->fd = accept(server->fd, (struct sockaddr*)&client->addr, &client->addr_len);

	if (client->fd < 0) {
		fprintf(stderr, "Error accepting websocket client!\n");
		goto err;
	}

	client->ssl = SSL_new(server->ssl_ctx);
	SSL_set_fd(client->ssl, client->fd);

	if (SSL_accept(client->ssl) <= 0) {
		// SSL handshake failed
		ERR_print_errors_fp(stderr);
		SSL_free(client->ssl);

		fprintf(stderr, "SSL handshake failed with client!\n");
		goto err;
	}

	inet_ntop(AF_INET, &(client->addr.sin_addr), client->ip, INET_ADDRSTRLEN);
	client->port = ntohs(client->addr.sin_port);

	ret = handle_client_handshake(client);

	if (ret)
		goto err;

	return client;

err:
	if (client) {
		ws_client_free(client);
		// TODO: close socket (i think)
	}

	return NULL;
}

static int init_ssl_context(ws_server_t *server, struct ws_server_config config)
{
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    server->ssl_ctx = SSL_CTX_new( TLS_server_method() );
    if (!server->ssl_ctx) {
		fprintf(stderr, "Error creating SSL context!\n");
		return -1;
    }

    if (SSL_CTX_use_certificate_file(server->ssl_ctx, config.ssl_cert_path, SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(server->ssl_ctx, config.ssl_key_path, SSL_FILETYPE_PEM) <= 0 ||
        !SSL_CTX_check_private_key(server->ssl_ctx)) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(server->ssl_ctx);

        return -1;
    }

	return 0;
}

static int handle_client_handshake(ws_client_t *client)
{
	int ret = 0;
	int bytes_read = 0;
	int bytes_sent = 0;
	char buffer[MAX_BUFFER_LEN];
	char *wss_req_key = NULL;
	char wss_resp_key[MAX_BUFFER_LEN];
	unsigned char resp_key_hash[SHA_DIGEST_LENGTH];
	char *resp_key_base64;

	memset(buffer, 0, MAX_BUFFER_LEN);
	bytes_read = SSL_read(client->ssl, buffer, MAX_BUFFER_LEN-1);

	if (bytes_read <= 0) {
		ret = SSL_get_error(client->ssl, ret);
		fprintf(stderr, "Error reading handshake request from client or the client unexpectedly closed the connection (code: %d)!\n", ret);
		ERR_print_errors_fp(stderr);
		
		return ret;
	}

	ret = parse_http_headers(client, buffer);

	if (ret) {
		fprintf(stderr, "Error parsing handshake request headers!\n");
		return -1;
	}

	for (int i=0;i<client->headers_len;i++) {
		struct http_header *hdr = client->headers[i];

		if (hdr->key && strstr(hdr->key, "Sec-WebSocket-Key") != NULL) {
			wss_req_key = hdr->value;
			break;
		}
	}

	if (!wss_req_key) {
		fprintf(stderr, "Sec-WebSocket-Key header was not sent by the client!\n");
		return -1;
	}

	/*
	 * computing Sec-WebSocket-Accept key for handshake response:
	 * Sec-WebSocket-Accept = base64(sha1(Sec-WebSocket-Key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"))
	 */
	snprintf(wss_resp_key, sizeof(wss_resp_key), "%s%s", wss_req_key, WEBSOCKET_MAGIC);
	SHA1((unsigned char *)wss_resp_key, strlen(wss_resp_key), resp_key_hash);

	resp_key_base64 = base64_encode((const unsigned char *)resp_key_hash, SHA_DIGEST_LENGTH);

	//printf("plain: %s\n", wss_resp_key);
	//printf("base64: %s\n", resp_key_base64);

	snprintf(buffer, MAX_BUFFER_LEN,
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Accept: %s\r\n"
		"\r\n", resp_key_base64);

	free(resp_key_base64);

	bytes_sent = SSL_write(client->ssl, buffer, strlen(buffer));//send(client->fd, buffer, buffer_len, 0);

	if (bytes_sent <= 0) {
		fprintf(stderr, "Error sending handshake response to client (send() return code: %d)!\n", bytes_sent);
		return -1;
	}

	return 0;
}

static int parse_http_headers(ws_client_t *client, char *buffer)
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

			// skipping any whitespaces around ":"
			while (*val == ' ')
				val++;

			// value
			header->value = strndup(val, strlen(val));

			if (client->headers_len < 1)
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

