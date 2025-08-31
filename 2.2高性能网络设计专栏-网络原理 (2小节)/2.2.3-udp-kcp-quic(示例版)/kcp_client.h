#ifndef KCP_CLIENT_H_
#define KCP_CLIENT_H_

#include <iostream>

#include "kcp_session.h"
#include "udp_socket.h"

class KcpClient {
  public:
    KcpClient(const std::string &ip, uint16_t port, KcpOpt &kcp_opt);

    bool Run();

  private:
    bool CheckRet(int len);

    static constexpr uint32_t max_body_size = 1024 * 4;

  protected:
    int Send(const void *data, uint32_t size);
    virtual void HandleMessage(const std::string &msg) = 0;
    virtual void HandleClose() = 0;

  private:
    UdpSocket::ptr socket_;
    sockaddr_in server_addr_;
    KcpSession::ptr session_;
};

#endif