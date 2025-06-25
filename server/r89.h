#ifndef R89_H
#define R89_H

#include <stddef.h>

#include "ws-client.h"

#define MAX_BUFFER_LEN 1024
#define WS_PORT 8080

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#endif // R89_H