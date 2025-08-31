#include "udp_socket.h"
#include <sstream>
UdpSocket::UdpSocket(const std::string &ip, uint16_t port, int family)
    : ip_(ip), port_(port), family_(family) {
    addr_.sin_family = family_;
    addr_.sin_port = htons(port_);
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());
    TRACE("addr:", GetAddrToString());
    fd_ = socket(family_, SOCK_DGRAM, IPPROTO_UDP);

    if (fd_ == -1)
        throw strerror(errno);
}
//返回ip:port
std::string UdpSocket::GetAddrToString() const {
    std::stringstream ss;
    ss << inet_ntoa(addr_.sin_addr) << ":" << ntohs(addr_.sin_port);
    return ss.str();
}

UdpSocket::~UdpSocket() { 
    TRACE("close fd");
    close(fd_); 
}

bool UdpSocket::SetNoblock() {
    int flag = fcntl(fd_, F_GETFL);
    return fcntl(fd_, F_SETFL, flag | O_NONBLOCK) == 0;
}

// 服务端绑定
bool UdpSocket::Bind() {
    int ret = bind(fd_, (sockaddr *)&addr_, sizeof(addr_));

    if (ret == -1) {
        perror("bind error");
        return false;
    }

    return true;
}
// 接收数据 addr获取发送端的ip和端口
int UdpSocket::RecvFrom(void *buf, std::size_t size, sockaddr_in &addr,
                        int flag) {
    socklen_t len = sizeof(addr);
    return recvfrom(fd_, buf, size, flag, (sockaddr *)&addr, &len);
}
// 发送数据 addr指定接收者的ip和端口
int UdpSocket::SendTo(const void *buf, std::size_t size, sockaddr_in &addr,
                      int flag) {
    return sendto(fd_, buf, size, flag, (sockaddr *)&addr, sizeof(addr));
}