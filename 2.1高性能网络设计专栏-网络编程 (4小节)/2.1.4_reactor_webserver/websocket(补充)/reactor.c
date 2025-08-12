

#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <poll.h>
#include <sys/epoll.h>
#include <errno.h>
#include <sys/time.h>

#include "server.h"

#define CONNECTION_SIZE 1048576 // 1024 * 1024

#define MAX_PORTS 20

#define TIME_SUB_MS(tv1, tv2) ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)

int accept_cb(int fd);
int recv_cb(int fd);
int send_cb(int fd);

int epfd = 0;
struct timeval begin;

struct conn conn_list[CONNECTION_SIZE] = {0};
// fd

int set_event(int fd, int event, int flag)
{

	if (flag)
	{ // non-zero add

		struct epoll_event ev;
		ev.events = event;
		ev.data.fd = fd;
		epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
	}
	else
	{ // zero mod

		struct epoll_event ev;
		ev.events = event;
		ev.data.fd = fd;
		epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
	}
}

int event_register(int fd, int event)
{

	if (fd < 0)
		return -1;

	conn_list[fd].fd = fd;
	conn_list[fd].r_action.recv_callback = recv_cb;
	conn_list[fd].send_callback = send_cb;
	conn_list[fd].status = 0; // 参考代码和我的代码都没写，这里是补充上去的（虽然不写默认status也是0）

	memset(conn_list[fd].rbuffer, 0, BUFFER_LENGTH);
	conn_list[fd].rlength = 0;

	memset(conn_list[fd].wbuffer, 0, BUFFER_LENGTH);
	conn_list[fd].wlength = 0;

	set_event(fd, event, 1);
}

// listenfd(sockfd) --> EPOLLIN --> accept_cb
int accept_cb(int fd)
{

	struct sockaddr_in clientaddr;
	socklen_t len = sizeof(clientaddr);

	int clientfd = accept(fd, (struct sockaddr *)&clientaddr, &len);
	// printf("accept finshed: %d\n", clientfd);
	if (clientfd < 0)
	{
		printf("accept errno: %d --> %s\n", errno, strerror(errno));
		return -1;
	}

	event_register(clientfd, EPOLLIN); // | EPOLLET

	if ((clientfd % 1000) == 0)
	{

		struct timeval current;
		gettimeofday(&current, NULL);

		int time_used = TIME_SUB_MS(current, begin);
		memcpy(&begin, &current, sizeof(struct timeval));

		printf("accept finshed: %d, time_used: %d\n", clientfd, time_used);
	}

	return 0;
}

int recv_cb(int fd)
{

	memset(conn_list[fd].rbuffer, 0, BUFFER_LENGTH);
	int count = recv(fd, conn_list[fd].rbuffer, BUFFER_LENGTH, 0);
	if (count == 0)
	{ // disconnect
		printf("client disconnect: %d\n", fd);
		close(fd);

		epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL); // unfinished

		return 0;
	}
	else if (count < 0)
	{ //

		printf("count: %d, errno: %d, %s\n", count, errno, strerror(errno));
		close(fd);
		epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);

		return 0;
	}

	conn_list[fd].rlength = count;
	// printf("RECV: %s\n", conn_list[fd].rbuffer);

	ws_request(&conn_list[fd]);

	set_event(fd, EPOLLOUT, 0);

	return count;
}

int send_cb(int fd)
{

	ws_response(&conn_list[fd]);

	int count = 0;

	if (conn_list[fd].wlength != 0)
	{
		count = send(fd, conn_list[fd].wbuffer, conn_list[fd].wlength, 0);
	}

	set_event(fd, EPOLLIN, 0);

	return count;
}

int init_server(unsigned short port)
{

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in servaddr;
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // 0.0.0.0
	servaddr.sin_port = htons(port);			  // 0-1023,

	if (-1 == bind(sockfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr)))
	{
		printf("bind failed: %s\n", strerror(errno));
	}

	listen(sockfd, 10);
	// printf("listen finshed: %d\n", sockfd); // 3

	return sockfd;
}

int main()
{

	unsigned short port = 2000;

	epfd = epoll_create(1);

	int i = 0;

	for (i = 0; i < MAX_PORTS; i++)
	{

		int sockfd = init_server(port + i);

		conn_list[sockfd].fd = sockfd;
		conn_list[sockfd].r_action.recv_callback = accept_cb;

		set_event(sockfd, EPOLLIN, 1);
	}

	gettimeofday(&begin, NULL);

	while (1)
	{ // mainloop

		struct epoll_event events[1024] = {0};
		int nready = epoll_wait(epfd, events, 1024, -1);

		int i = 0;
		for (i = 0; i < nready; i++)
		{

			int connfd = events[i].data.fd;

			if (events[i].events & EPOLLIN)
			{
				conn_list[connfd].r_action.recv_callback(connfd);
			}

			if (events[i].events & EPOLLOUT)
			{
				conn_list[connfd].send_callback(connfd);
			}
		}
	}
}

/*
ws_request() 是服务端收到数据时调用的：
如果 status == 0：先做 WebSocket 握手 (handshake)。
如果 status == 1：解析客户端数据帧 (decode_packet)，然后重新打包 (encode_packet) 回发。

ws_response() 应该是：
由服务端主动推送数据给客户端时调用（而不是等待客户端发数据）。
它可能会构造一个 WebSocket 帧，然后把数据写入 c->wbuffer，并把 c->wlength 设置好，让主循环或 reactor 发送。
*/
