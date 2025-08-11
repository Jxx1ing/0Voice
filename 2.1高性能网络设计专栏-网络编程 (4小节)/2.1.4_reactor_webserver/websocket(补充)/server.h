#ifndef __SERVER_H__
#define __SERVER_H__

#define BUFFER_LENGTH 1024

typedef int (*RCALLBACK)(int fd);

struct conn
{
	int fd;

	char rbuffer[BUFFER_LENGTH];
	int rlength;

	char wbuffer[BUFFER_LENGTH];
	int wlength;

	RCALLBACK send_callback;

	union
	{
		RCALLBACK recv_callback;
		RCALLBACK accept_callback;
	} r_action;

	int status;
#if 1 // websocket
	char *payload;
	char mask[4];
#endif
};

int ws_request(struct conn *c);
int ws_response(struct conn *c);

#endif
