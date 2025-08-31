#include "kcp_client.h"
#include "kcp_util.h"

KcpClient::KcpClient(const std::string &ip, uint16_t port, KcpOpt &kcp_opt) {
    server_addr_.sin_family = AF_INET;
    server_addr_.sin_port = htons(port);
    server_addr_.sin_addr.s_addr = inet_addr(ip.c_str());

    socket_ = std::make_shared<UdpSocket>(ip.c_str(), port, AF_INET);
    socket_->SetNoblock();

    session_ = std::make_shared<KcpSession>(kcp_opt, server_addr_, socket_);
}

bool KcpClient::Run() {
    //每个客户端只有一个kcpsession
    if (session_->Update(iclock64())) {
        HandleClose();
        return false;
    }

    std::string buf(1024 * 4, 'a');

    sockaddr_in addr;

    int len =
        socket_->RecvFrom(const_cast<char *>(buf.data()), buf.length(), addr);

    if (addr.sin_port != server_addr_.sin_port ||
        addr.sin_addr.s_addr != server_addr_.sin_addr.s_addr) {
        if (len != -1)
            TRACE("run ret:", len);
        return true;
    }
    if (!CheckRet(len))
        return true;

    buf.resize(len);

    len = session_->Input(const_cast<char *>(buf.data()), len);

    if (len != 0) {

        TRACE("input error = ", len);
        return false;
    }

    std::string body(max_body_size, 'a');

    do {
        len = session_->Recv(const_cast<char *>(body.data()), max_body_size);

        if (len == -3) {
            TRACE("body_size too small");
            exit(-1);
        }

        if (len <= 0)
            break;

        body.resize(len);

        HandleMessage(body); // 调用子类，处理业务
    } while (1);

    return true;
}

int KcpClient::Send(const void *data, uint32_t size) {
    int ret = session_->Send(data, size);
    return ret;
}

bool KcpClient::CheckRet(int len) {

    if (len == -1) {
        if (errno != EAGAIN)
            perror("recvform error!");

        return false;
    }

    if (len < KCP_HEADER_SIZE)
        return false;

    return true;
}