#include <stdio.h>
#include <stdlib.h>

#include "chat.h"
#include "ws-client.h"

void ws_client_init(ws_client_t *client)
{
	int bytes_read = 0;
	char buffer[MAX_BUFFER_LEN];

	//client->ops.read(client, NULL);

	while (1) {
		bytes_read = recv(client->fd, buffer, MAX_BUFFER_LEN, 0);

		if (bytes_read > 0) {
			printf("Data received: %s\n", buffer);
		}
		else if (bytes_read == 0) {
			if (client->ops.close)
				client->ops.close(client);

			break;
		}
		else {
			// TODO: add some read error callback
		}
	}
}

void ws_client_free(ws_client_t *client)
{
	free(client);
	// TODO: free headers;
}