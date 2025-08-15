#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <openssl/sha.h>

#include "r89.h"
#include "ws-server.h"

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

	client->port = 0;
	client->ssl = NULL;
	client->ssl_error = 0;
	client->headers = NULL;
	client->headers_len = 0;
	client->addr_len = sizeof(struct sockaddr_in);
	client->status = CLIENT_CONNECTED;

	client->fd = accept(server->fd, (struct sockaddr*)&client->addr, &client->addr_len);

	if (client->fd < 0) {
		fprintf(stderr, "Error accepting websocket client (Code: %d, Error: %s)!\n", errno, strerror(errno));
		goto err;
	}

	inet_ntop(AF_INET, &(client->addr.sin_addr), client->ip, INET_ADDRSTRLEN);
	client->port = ntohs(client->addr.sin_port);

	client->ssl = SSL_new(server->ssl_ctx);
	SSL_set_fd(client->ssl, client->fd);

	if (SSL_accept(client->ssl) <= 0) {
		// SSL handshake failed
		ERR_print_errors_fp(stderr);

		fprintf(stderr, "SSL handshake failed with client!\n");
		goto err;
	}

	return client;

err:
	if (client) 
		ws_client_free(client);

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


