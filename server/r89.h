#ifndef R89_H
#define R89_H

#include <stddef.h>

#include "ws-client.h"

#define MAX_BUFFER_LEN 1024

#define WSS_PORT 8080
#define WSS_CERT_PATH "/etc/letsencrypt/live/chateljunk.hu/cert.pem"
#define WSS_KEY_PATH "/etc/letsencrypt/live/chateljunk.hu/privkey.pem"


#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#endif // R89_H