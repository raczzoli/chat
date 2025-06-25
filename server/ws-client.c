#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "chat.h"
#include "ws-client.h"

static int parse_frame(struct ws_frame *frame, char **buffer);

int ws_client_init(ws_client_t *client)
{
	int bytes_read = 0;
	char buffer[MAX_WS_BUFFER_LEN];
	char *buffer_ref = NULL;
	struct ws_frame frame;
	struct ws_data data;
	int ret = 0;

	while (1) {
		buffer_ref = buffer;
		bytes_read = recv(client->fd, buffer, MAX_WS_BUFFER_LEN, 0);

		if (bytes_read > 0) {
			ret = parse_frame(&frame, &buffer_ref);

			if (ret) {
				// TODO: invalid frame... do something
				return ret;
			}

			switch(frame.opcode) {
				case 0x0: // continuation
				case 0x1: // text
				case 0x2: // binary

					/*
					 * TODO: In the future we will need to handle two cases here:
					 *
					 * 1. payload_len > MAX_WS_BUFFER_LEN, so we will need to do
					 * multiple reads from the socket to have the full payload data
					 * (for the moment we allow frames with max payload_len of 126 
					 * => 65535 bytes)
					 *
					 * 2. handle continuation frames
					 */
					 
					if (frame.opcode == 0x0 || frame.payload_len > MAX_TEMP_PAYLOAD_LEN) {
						/*
						 * since we don`t yet have proper handling of large payload data 
						 * if we receive payload length greater than MAX_TEMP_PAYLOAD_LEN we 
						 * return here to avoid reading further (invalid for us) data chunks
						 * from the client
						 * TODO 2 - also we should close the connection for now
						 */
						return -EMSGSIZE;
					}

					data.type = frame.opcode;
					data.is_text = data.type == 0x1;
					data.payload_len = frame.payload_len;
					data.payload = NULL;

					printf("Final frame: %d, opcode is: %d, is masked: %d, payload len: %ld bytes\n", frame.is_fin, frame.opcode, frame.is_masked, frame.payload_len);

					if (frame.payload_len > 0) {
						data.payload = (char *) malloc(data.is_text ? frame.payload_len + 1 : frame.payload_len);
						if (!data.payload) {
							fprintf(stderr, "Error allocating memory for payload!\n");
							return -ENOMEM;
						}
					}

					if (frame.is_masked > 0) {
						for (uint64_t i=0;i<frame.payload_len;i++) {
							data.payload[i] = buffer_ref[i] ^ frame.masking_key[i%4];
						}
					}
					else {
						memcpy(data.payload, buffer_ref, frame.payload_len);
					}

					if (data.is_text)
						data.payload[frame.payload_len] = '\0';

					if (frame.is_fin > 0) 
						client->ops.read(client, &data);
				
					free(data.payload);
					data.payload = NULL;
				break;

				case 0x8:
					if (client->ops.close)
						client->ops.close(client);
				break;

				case 0x9: // PING
					printf("PING\n");
				break;

				case 0xA: // PONG
					printf("PONG\n");
				break;

				default:
					fprintf(stderr, "Invalid opcode 0x%02X from client!\n", frame.opcode);
			}
		}
		else if (bytes_read == 0) {
			if (client->ops.close)
				client->ops.close(client);

			break;
		}
		else {
			// TODO: add some read error callback
			return bytes_read;
		}
	}

	return 0;
}

static int parse_frame(struct ws_frame *frame, char **buffer)
{
	int ret = 0;
	char *buff = *buffer;
	uint8_t byte = (uint8_t)buff[0];

	frame->is_fin = (byte & 0x80) > 0 ? 1 : 0;
	frame->opcode = byte & 0x0F;

	buff++;
	byte = (uint8_t)buff[0];

	frame->is_masked = (byte & 0x80) > 0 ? 1 : 0;
	frame->payload_len = 0;
	
	switch(frame->opcode) {
		case 0x0: // continuation
		case 0x1: // text
		case 0x2: // binary
			buff++;

			/*
			 * calculating payload len
			 */
			uint64_t len = byte & 0x7F;

			/*
			* Explanation:
			*
			* - in case of len = 126 the actual payload length is stored in the following 
			* 2 bytes so after adding up these 2 bytes the result will be a 16 bit int (uint16_t)
			* - in case of len = 127, the actual payload length is stored in the following 
			* 8 bytes ... so the result will be a 64 bit int (uint64_t) 
			* to sum these bytes on a single integer, we shift the first byte (or the latest 
			* computed value) with 8 bits to the left, leaving 8 zeroes behind, and after | -ing 
			* this value with the next byte, it`s value will replace those zeroes
			* 
			* ex: payload len = 300 (case 126) -> byte 1 = 0x01, byte 2 = 0x2C (this is what we
			* get from the websocket client):
			* 
			* byte 1 << 8 	      = 00000001 => 00000001 00000000 |
			* byte 2 (no shift)  = 00101100 => 00000000 00101100 =
			* --------------------------------------------------------
			*                                  00000001 00101100 = 300
			*/
			
			if (len <= 125) {
				// len is the actual payload length
			}
			else if (len == 126) {
				len = (buff[0] << 8) | (uint64_t)(uint8_t)buff[1];
				buff += 2;
			}
			else if (len == 127) { // TODO: test again this case
				len = 0;
				for (int i = 0; i < 8; i++) {
					len = (len << 8) | (uint64_t)(uint8_t)buff[0];
					buff++;
				}
			}
			
			frame->payload_len = len;

			/*
			 * extracting masking key (if is_mask = 1)
			 */
			if (frame->is_masked > 0) {
				memcpy(frame->masking_key, buff, 4);
				buff += 4;
			}

		break;
	}

	*buffer = buff;

	return ret;
}

void ws_client_free(ws_client_t *client)
{
	free(client);
	// TODO: free headers;
}