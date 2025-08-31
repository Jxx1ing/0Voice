#ifndef KCP_SESSION_H_
#define KCP_SESSION_H_
// 包含通用头文件
#include "ikcp.h"
#include "udp_socket.h"
#include <arpa/inet.h>
#include <iostream>
#include <memory>
#include <mutex>
#define KCP_KEEP_ALIVE_CMD "KCP_KEEP_ALIVE_CMD"

const int KCP_HEADER_SIZE = 24; // kcp头部协议占用的字节

// kcp参数设置
class KcpOpt {
  public:
    bool is_server = true;
    int64_t keep_alive_timeout = 5000; // 超时时间, 默认5000毫秒
    uint32_t conv = 0;       // 会话id，该设置对服务端有效，服务端主要解析client的conv并创建同样的会话
    int sndwnd = 32;         // 默认发送窗口
    int rcvwnd = 128;         // 默认接收窗口
    int nodelay = 0;          // 无延迟是否开启 0未开启，1/2开启
    int interval = 10;        // 调度间隔时间, 默认10毫秒
    int resend = 0;           // 快速重传指标，0未开启，1/2开启
    int nc = 0;               // 流控 0开启，1关闭

//     KcpOpt(const KcpOpt & opt) {
//       this->is_server = opt.is_server;
//       this->keep_alive_timeout = opt.keep_alive_timeout;
//       this->conv = opt.conv;
//       this->sndwnd = opt.sndwnd;
//       this->rcvwnd = opt.rcvwnd;
//       this->nodelay = opt.nodelay;
//       this->interval = opt.interval;
//       this->resend = opt.resend;
//       this->nc = opt.nc;
//     }
};

class KcpSession {
  public:
    using ptr = std::shared_ptr<KcpSession>;

    KcpSession(KcpOpt &kcp_opt, const sockaddr_in &addr,
               const UdpSocket::ptr &socket);
    ~KcpSession();

    const sockaddr_in &GetAddr() const { return addr_; }

    std::string GetAddrToString() const;

    void SetAddr(const sockaddr_in &addr) { addr_ = addr; }

    /**
     * @brief: 每隔 10 - 100ms 循环调用, 服务端可以设置小一些
     * @param[in] current 时间戳毫秒
     * @return {bool}     返回true说明客户端退出或者服务端退出;返回false正常
     */
    bool Update(int64_t current);
    /**
     * @brief 将从udp socket接收的的数据发给kcp内部处理
     * @param[in] data
     * @param[in] size
     * @return  0 : success |-1 : input data is too short | -2 : wrong conv | -3
     * : data error
     */
    int Input(void *data, std::size_t size);
    /**
     * @brief  Kcp wiil atomatically fragment data to KcpSeg
     * @param[in] data
     * @param[in] size
     * @return  0 : success | -1 : failed
     */
    int Send(const void *data, uint32_t size);
    /**
     * @brief  receive a user packet from Kcp
     * @param[in,out] data
     * @param[in]  size
     * @return  -1 : the size of buf greater data size | 0 : empty | > 0 : data
     * size
     */
    int Recv(void *buf, uint32_t size);

    int sendTo(const void *buf, std::size_t size, int flag = 0);
    uint32_t conv() { return kcp_opt_.conv; }
    void SetKeepAlive(int64_t current) { recv_latest_time = current; }

  private:
    KcpOpt kcp_opt_;
    uint32_t conv_;
    sockaddr_in addr_;
    UdpSocket::ptr socket_;
    ikcpcb *kcp_;
    int64_t recv_latest_time = 0;
    int64_t send_latest_time = 0;

  public:
    using Lock = std::unique_lock<std::mutex>;

  private:
    std::mutex mtx_;
};

#endif // KCP_SESSION_H_