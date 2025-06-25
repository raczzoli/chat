#include <stdio.h>
#include <jansson.h>

#include "r89.h"
#include "chat.h"
#include "ws-server.h"
#include "ws-client.h"

struct chat_context *chat_ctx;

int main()
{

	chat_ctx = chat_init();

	if (!chat_ctx)
		return -1;

	printf("Chat service created successfuly...\n");
	printf("Awaiting client connections...\n");

	chat_accept_connections(chat_ctx);

	return 0;
}

