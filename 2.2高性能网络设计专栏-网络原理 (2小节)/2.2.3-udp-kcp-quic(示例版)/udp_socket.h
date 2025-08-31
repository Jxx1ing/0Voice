#ifndef UDP_SOCKET_H_
#define UDP_SOCKET_H_
// 包含通用头文件
 
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <errno.h>
#include <memory>
#include "trace.h"
class UdpSocket // udp的封装
{
public:
        using ptr = std::shared_ptr<UdpSocket>;

        UdpSocket(const std::string &ip,uint16_t port,int family);

        ~UdpSocket();

        int SendTo(const void *buf,std::size_t size,sockaddr_in &addr,int flag = 0);

        int RecvFrom(void *buf,std::size_t size,sockaddr_in &addr,int flag = 0);

        bool Bind();

        bool SetNoblock();
        std::string GetAddrToString() const;
private:
        std::string ip_;
        uint16_t    port_;
        int         fd_;
        int         family_;
        sockaddr_in addr_;
};
#endif // UDP_SOCKET_H_