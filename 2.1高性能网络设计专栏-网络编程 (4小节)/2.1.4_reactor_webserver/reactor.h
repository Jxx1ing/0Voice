#ifndef __REACTOR_H__
#define __REACTOR_H__

#define BUFFER_LENGTH 1024

typedef int (*RCALLBACK)(int fd); // io - evnet - callback

// 每个连接独立的conn结构体
struct conn
{
    int fd;
    // 读
    char rbuffer[BUFFER_LENGTH];
    int rlength;
    // 写
    char wbuffer[BUFFER_LENGTH];
    int wlength;

    // event：EPOLLOUT ->发送消息
    RCALLBACK send_callback;
    // event：EPOLLIN -> 接受连接 or 收取消息
    union
    {
        RCALLBACK recv_callback;
        RCALLBACK accept_callback; // 没用到这个数据结构accept_callback
    } r_action;
    // ps:代码中没用到accept_callback，实际上可以直接去掉（写成如下形式）
    /*
    RCALLBACK send_callback;
    RCALLBACK recv_callback;
    */

    int status; // 状态机：0=初始，1=发送头，2=发送body
};

#endif