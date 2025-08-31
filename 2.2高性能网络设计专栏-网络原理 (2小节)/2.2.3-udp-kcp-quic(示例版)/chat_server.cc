#include "kcp_server.h"
#include <memory>
#include <mutex>
#include <sstream>
#include <unordered_set>
// 基于KcpServer封装业务
class ChatServer : public KcpServer {
  public:
    using KcpServer::KcpServer;
    // 接收客户端发送过来的消息
    virtual void HandleMessage(const KcpSession::ptr &session,
                               const std::string &msg) override {
        TRACE(msg);
        std::stringstream ss;
        ss << "user from [" << session->GetAddrToString() << "] : " << msg;
        TRACE(ss.str());
        Notify(ss.str());
    }
    void HandleConnection(const KcpSession::ptr &session) {
        {   // 注意锁的粒度
            Lock lock(mtx_);
            users_.insert(session);
        }

        std::stringstream ss;
        ss << "user from [" << session->GetAddrToString()
           << "] join the ChatRoom!";
        Notify(ss.str());   // 通知房间的所有人
    }
    void HandleClose(const KcpSession::ptr &session) {
        TRACE("close ", session->GetAddrToString());
        {
            Lock lock(mtx_);
            users_.erase(session);
        }
        std::stringstream ss;
        ss << "user from [" << session->GetAddrToString()
           << "] left the ChatRoom!";
        Notify(ss.str());
    }
    void Notify(const std::string &str) {
        Lock lock(mtx_);

        for (auto &user : users_)
            user->Send(str.data(), str.size());
    }

  public:
    using Lock = std::unique_lock<std::mutex>;

  private:
    std::unordered_set<KcpSession::ptr> users_;  // 业务保存
    std::mutex mtx_;
};

int main(int argc, char *argv[]) {
    // arg : ip + port
    if (argc != 3) {
        TRACE("args error please input [ip] [port]");
        exit(-1);
    }

    std::string ip = argv[1];
    uint16_t port = atoi(argv[2]);
    KcpOpt opt;
    opt.conv = 0;
    opt.is_server = true;
    opt.keep_alive_timeout = 5000; // 超过5秒没有收到客户端的信息则认为其断开
    ChatServer server(opt, ip, port);

    server.Run();

    return 0;
}
