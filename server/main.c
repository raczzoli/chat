#include <stdio.h>
#include <jansson.h>
#include <signal.h>

#include "r89.h"
#include "chat.h"
#include "ws-server.h"
#include "ws-client.h"

struct chat_context *chat_ctx;
struct ws_server_config ws_config;

int main()
{
	//signal(SIGPIPE, SIG_IGN); 

	ws_config.port = WSS_PORT;
	ws_config.ssl_cert_path = WSS_CERT_PATH;
	ws_config.ssl_key_path = WSS_KEY_PATH;

	chat_ctx = chat_create(ws_config);

	if (!chat_ctx)
		return -1;

	printf("Chat service created successfuly...\n");
	printf("Awaiting client connections...\n");

	chat_init(chat_ctx);

	return 0;
}

