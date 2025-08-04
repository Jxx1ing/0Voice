#ifndef __SERVER_H__
#define __SERVER_H__

#include "reactor.h"

#define INFO printf

int http_request(struct conn *c);
int http_response(struct conn *c);

#endif
