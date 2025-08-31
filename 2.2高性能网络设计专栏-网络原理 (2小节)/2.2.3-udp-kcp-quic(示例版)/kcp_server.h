#ifndef KCP_SERVER_H_
#define KCP_SERVER_H_
// 包含通用头文件
#include <arpa/inet.h>

#include "kcp_session.h"
#include "udp_socket.h"
#include <iostream>
#include <unordered_map>
#include <vector>

class KcpServer {
  public:
    KcpServer(KcpOpt &kcp_opt, const std::string &ip, uint16_t port);

    void Run();

  private:
    void Update();
    bool Check(int ret);
    void HandleSession(const KcpSession::ptr &session, int length);
    uint32_t GetConv(const void *data);
    KcpSession::ptr GetSession(uint32_t conv, const sockaddr_in &addr);
    KcpSession::ptr NewSession(uint32_t conv, const sockaddr_in &addr);

    static constexpr uint32_t body_size = 1024 * 4;

  protected:
    virtual void HandleClose(const KcpSession::ptr &session) = 0;
    virtual void HandleMessage(const KcpSession::ptr &session,
                               const std::string &msg) = 0;
    virtual void HandleConnection(const KcpSession::ptr &session) = 0;

  private:    // KcpSession绑定了kcp实例
    using SessionMap = std::unordered_map<uint32_t, KcpSession::ptr>;

  private:
    KcpOpt kcp_opt_;
    UdpSocket::ptr socket_;

    SessionMap sessions_;
    std::vector<char> buf_;

  public:
    using Lock = std::unique_lock<std::mutex>;

  private:
    std::mutex mtx_;
};

#endif // KCP_SERVER_H_