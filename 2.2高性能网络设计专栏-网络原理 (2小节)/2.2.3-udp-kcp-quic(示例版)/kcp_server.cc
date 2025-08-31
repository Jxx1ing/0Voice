#include "kcp_server.h"
#include <iostream>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
/* get system time */
static inline void itimeofday(long *sec, long *usec) {
    struct timeval time;
    gettimeofday(&time, NULL);
    if (sec)
        *sec = time.tv_sec;
    if (usec)
        *usec = time.tv_usec;
}

/* get clock in millisecond 64 */
static inline int64_t iclock64() {
    long s, u;
    int64_t value;
    itimeofday(&s, &u);
    value = ((int64_t)s) * 1000 + (u / 1000);
    return value;
}

KcpServer::KcpServer(KcpOpt &kcp_opt, const std::string &ip, uint16_t port)
    : kcp_opt_(kcp_opt),
      socket_(std::make_shared<UdpSocket>(ip, port, AF_INET)), buf_(body_size) {
    if (!socket_->Bind()) {
        exit(-1);
    }
    //  socket_->SetNoblock();
}

void KcpServer::Update() {
    Lock lock(mtx_);   // session的管理
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (!it->second->Update(iclock64())) {
            ++it;
            continue;
        }
        HandleClose(it->second);
        it = sessions_.erase(it);
    }
}

bool KcpServer::Check(int ret) {
    if (ret == -1) {
        if (errno != EAGAIN)
            perror("recvform error!");

        return false;
    }

    if(ret < 24) {
        return false;
    }

    return true;
}

KcpSession::ptr KcpServer::GetSession(uint32_t conv, const sockaddr_in &addr) {
    auto it = sessions_.find(conv);

    if (it == sessions_.end())
        sessions_[conv] = NewSession(conv, addr);

    KcpSession::ptr session = sessions_[conv];

    const sockaddr_in &session_addr = session->GetAddr();

    if (session_addr.sin_port != addr.sin_port ||
        session_addr.sin_addr.s_addr != addr.sin_addr.s_addr) {
        session->SetAddr(addr);  // 保存的客户端的地址 ip + port
    }

    return session;
}

KcpSession::ptr KcpServer::NewSession(uint32_t conv, const sockaddr_in &addr) {
    KcpOpt opt = kcp_opt_;
    opt.conv = conv;
    //  opt.is_server = kcp_opt_.is_server;
    //  opt.keep_alive_timeout = kcp_opt_.keep_alive_timeout;
    KcpSession::ptr session = std::make_shared<KcpSession>(opt, addr, socket_);

    HandleConnection(session);

    return session;
}

uint32_t KcpServer::GetConv(const void *buf) { return *(uint32_t *)(buf); }
void KcpServer::Run() {
    // fix me，目前这个数没有写退出机制
    std::thread t([this]() {
        while (true) {
            usleep(10);
            Update();
        }
    });

    do {
        sockaddr_in addr;
        int len = socket_->RecvFrom(buf_.data(), body_size, addr); // 采用block的方式 读取到udp
                                                             //  if (len != -1)
        // trace("recvfrom:len = ", len);
        if (!Check(len)) // 检测是否有数据可以读取 header 24字节
            continue;

        uint32_t conv = GetConv(buf_.data());
        {
            Lock lock(mtx_); // 加锁
            //   TRACE("conv:", conv);
            KcpSession::ptr session = GetSession(conv, addr);

            HandleSession(session, len);
        }

    } while (1);
    t.join(); // 等待线程退出
}

void KcpServer::HandleSession(const KcpSession::ptr &session, int length) {
    int ret = session->Input(buf_.data(), length);

    if (ret != 0) {
        TRACE("Input error = ", ret);
        return;
    }

    do {
        int len = session->Recv(buf_.data(), body_size); // 读取用户数据

        if (len == -3) {
            TRACE("body size too small");
            exit(-1);
        }

        if (len <= 0)
            break;

        // 先检测是不是保活命令
        if (memcmp(buf_.data(), KCP_KEEP_ALIVE_CMD,
                   sizeof(KCP_KEEP_ALIVE_CMD)) == 0) {
            // TRACE("recv conv: ", session->conv(), " keep alive cmd");
            session->SetKeepAlive(iclock64()); // 通过带宽换cpu效率
            continue;
        }

        std::string msg(buf_.data(), buf_.data() + len);
        //   TRACE(msg);
        HandleMessage(session, msg); // 当前线程读取， 调用业务子类处理业务
    } while (1);
}
