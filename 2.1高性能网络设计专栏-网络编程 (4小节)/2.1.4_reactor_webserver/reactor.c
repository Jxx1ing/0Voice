#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include "reactor.h"
#include "server.h"

#define CONNECTION_SIZE 1048576 // 1024 * 1024
#define INFO printf
#define MAX_PORTS 20
#define TIME_SUB_MS(tv1, tv2) ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)

// 声明函数
int accept_cb(int fd);
int recv_cb(int fd);
int send_cb(int fd);

// 第一个连接开始的时间（每1000个连接是一个循环）
struct timeval begin;

// epoll 实例（全局变量, 后续赋值为epoll_create() 返回的 fd）
int epfd = 0;

// 全局的连接数组，每个元素是conn结构体类型（对应一个fd 客户端连接epoll_ctl）。通过conn_list[fd]可以直接定位某个连接的具体信息。
struct conn conn_list[CONNECTION_SIZE] = {0};

// 监听fd的事件 epoll_ctl
// 根据事件类型对事件进行管理，分为两类——一类是添加ADD，一类是修改MODIFY
int set_event(int fd, int event, int flag)
{
    // 这里采用flag是1（把新连接加入监听ADD） 或者 0（修改监听事件的类型 读<->写）
    if (flag)
    {
        struct epoll_event ev;
        ev.events = event;
        ev.data.fd = fd;
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    }
    else
    {
        struct epoll_event ev;
        ev.events = event;
        ev.data.fd = fd;
        epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
    }
}

// 使用在accept建立连接后，作用是初始化一个连接的具体信息(当客户端与服务端建立连接后调用，即accept后)
int event_register(int fd, int event)
{
    if (fd < 0)
        return -1;

    conn_list[fd].fd = fd;
    memset(conn_list[fd].rbuffer, 0, BUFFER_LENGTH);
    conn_list[fd].rlength = 0;
    memset(conn_list[fd].wbuffer, 0, BUFFER_LENGTH);
    conn_list[fd].wlength = 0;
    // 事件只有两种类型，读事件/写事件
    conn_list[fd].r_action.recv_callback = recv_cb; // 读事件（触发条件是EPOLLIN，可能是建立连接 或者 接收消息）
    conn_list[fd].send_callback = send_cb;          // 写事件（触发条件是EPOLLOUT）
    // 连接建立后，监听可读事件（客户端发来的消息）
    set_event(fd, event, 1);
}

int accept_cb(int fd)
{
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    int client_fd = accept(fd, (struct sockaddr *)&client_addr, &len);
    if (client_fd < 0)
    {
        printf("accept errno: %d --> %s\n", errno, strerror(errno));
        return -1;
    }
    // INFO("accept finished: %d\n", client_fd);
    //  运行到这里，连接已经建立。接下来epoll监听可读事件（客户端发来的消息）
    event_register(client_fd, EPOLLIN);

    if ((client_fd % 1000) == 0)
    {

        struct timeval current;
        gettimeofday(&current, NULL);

        int time_used = TIME_SUB_MS(current, begin);
        memcpy(&begin, &current, sizeof(struct timeval));

        INFO("accept finshed: %d, time_used: %d\n", client_fd, time_used);
    }

    return 0;
}

int recv_cb(int fd)
{
    memset(conn_list[fd].rbuffer, 0, BUFFER_LENGTH);
    int count = recv(fd, conn_list[fd].rbuffer, BUFFER_LENGTH, 0);
    // 客户端正常关闭 quit
    if (count == 0)
    {
        INFO("client disconnect: %d\n", fd);
        close(fd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
        return 0;
    }
    // 客户端异常关闭 ctrl+c
    else if (count < 0)
    {
        INFO("count: %d, errno:%d, %s\n", count, errno, strerror(errno));
        close(fd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
        return 0;
    }
    conn_list[fd].rlength = count;

    // 打印客户端发送来的消息
    INFO("[%d]RECV: %s\n", conn_list[fd].rlength, conn_list[fd].rbuffer);

    // 服务端请求并没有真正发送，这里只是清理状态。（真正的填充wbuffer和回复客户端在http_response函数中）
    http_request(&conn_list[fd]);

    // 服务端收到客户端消息 → 切换监听到 EPOLLOUT，准备把数据发回去
    set_event(fd, EPOLLOUT, 0);
    // 一些说明
    /*
        写事件：只要 socket 发送缓冲区有空间，它就会触发（大部分时候都满足）
        如果一直监听 EPOLLOUT，epoll_wait 每次都会立刻返回这个事件 → 主循环会疯狂 wakeup → CPU 空转
        因此写完数据后，马上取消EPOLLOUT监听，避免无意义的事件触发（具体做法在send_cb中设置set_event）
    */
}

// I/O 层
// 只管事件和IO,Reactor不需要知道发送的是html文件 or png图片 or字符，这是业务层webserver.c的行为（http_response body 是什么）
int send_cb(int fd)
{
    // 构造并返回客户端的http请求（填充wbuffer）
    http_response(&conn_list[fd]);

    int count = 0;
    if (conn_list[fd].status == 1)
    {
        // 业务层的http_request已经填充好http_head(此时status = 1)，这里需要发送给客户端
        count = send(fd, conn_list[fd].wbuffer, conn_list[fd].wlength, 0);
        set_event(fd, EPOLLOUT, 0);
    }
    else if (conn_list[fd].status == 2)
    {
        set_event(fd, EPOLLOUT, 0);
    }
    else if (conn_list[fd].status == 0)
    {
        // 服务端发送成功后：切回 EPOLLIN继续监听读事件（客户端发来的消息）
        set_event(fd, EPOLLIN, 0);
    }

    return count;
    /*
在status=0时候，参考代码中加了
    if (conn_list[fd].wlength != 0)
    {
            count = send(fd, conn_list[fd].wbuffer, conn_list[fd].wlength, 0);
    }
但个人认为好像没什么用，因为status = 2时，webserver.c中将rlength=0,因此不会执行;
此外，一开始状态是status,recv_cb由于执行http_response因此填充head后状态 status=1,也不会执行ractor.c中的status == 0;

补充：经过实验，通过打印信息————发现status=0的状态程序会进入，但是if (conn_list[fd].wlength != 0) 程序不会进入。
因此可以这段代码应该不需要加入。
    */
}

int init_server(unsigned short port)
{

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
    {
        perror("socket failed\n");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        INFO("bind failed.\n");
        return -2;
    }
    if (listen(listen_fd, 5) < 0)
    {
        INFO("listen failed.\n");
        return -3;
    }
    // INFO("listenfd: %d\n", listen_fd);

    return listen_fd;
}

int main()
{
    unsigned short port = 2000;
    epfd = epoll_create1(0); // 创建 epoll 实例

    // 服务端开放多个端口，丰富五元组以此建立更多连接
    int i = 0;
    for (i = 0; i < MAX_PORTS; i++)
    {
        int listenfd = init_server(port + i); // 监听fd
        conn_list[listenfd].fd = listenfd;
        // 注意区分：这里把accept_cb的地址赋值给recv_callback；这不会触发函数执行。如果要调用accept_cb(fd);
        conn_list[listenfd].r_action.recv_callback = accept_cb;
        // 把listenfd 加入 epoll监听读事件（是否新连接）
        set_event(listenfd, EPOLLIN, 1);
    }

    // 记录第一个连接的时间
    gettimeofday(&begin, NULL);

    while (1)
    {
        struct epoll_event events[1024] = {0}; // 这一条写在while循环内或者while循环外功能上功能上没区别
        int nfds = epoll_wait(epfd, events, 1024, -1);
        for (int i = 0; i < nfds; ++i)
        {
            int connfd = events[i].data.fd;
            // 如果事件可读：建立连接，接收消息
            if (events[i].events & EPOLLIN)
            {
                // 这里传入参数，正式调用了accept_cb——先accept建立连接，然后接收消息
                conn_list[connfd].r_action.recv_callback(connfd);
            }
            // 如果事件可写：服务端发送消息给客户端
            if (events[i].events & EPOLLOUT)
            {
                // 这里传入参数，正式调用了send_cb： 发送消息
                conn_list[connfd].send_callback(connfd);
            }
            // 注意不可以写成 if-else 形式，因为读事件和写事件不是二选一，可以一起处理
        }
    }
}

/*
accept_cb 不是直接调用 recv_cb，但它会创建新的客户端连接，并为这个新连接注册一个 recv_cb 回调。
这是一种“事件注册”的过程，后续客户端发数据时，epoll_wait事件触发时会调用对应回调函数recv_cb：
* 注册阶段（accept_cb / event_register）：给 fd 绑定一个回调函数。
* 调度阶段（epoll_wait）：事件触发时调用对应回调函数（使用函数指针进行回调：conn_list[connfd].r_action.recv_callback(connfd);）

流程上可以这么理解：
1-listenfd 发生可读事件，触发 accept_cb，调用 accept() 获得 client_fd。
2-accept_cb 调用 event_register(client_fd, EPOLLIN)，在 event_register 中给 client_fd 绑定 recv_cb，并注册到 epoll 监听可读事件。
3-当客户端通过 client_fd 发送数据时，epoll_wait 返回（说明有事件触发），调用 recv_cb，进行数据读取和处理。
所以，accept_cb 是“接纳新连接 + 注册新连接的读取回调”，recv_cb 是“处理客户端数据”。
*/