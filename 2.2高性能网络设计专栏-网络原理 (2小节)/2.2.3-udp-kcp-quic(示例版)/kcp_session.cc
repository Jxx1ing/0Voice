#include "kcp_session.h"
#include "kcp_util.h"
#include <sstream>
int udp_output(const char *buf, int len, ikcpcb *kcp, void *user) {
    KcpSession *ptr = (KcpSession *)user;

    // TRACE("send len = ", len);
    int ret = ptr->sendTo((const void *)buf, len);
    return ret;
}

KcpSession::KcpSession(KcpOpt &kcp_opt, const sockaddr_in &addr,
                       const UdpSocket::ptr &socket)
    : kcp_opt_(kcp_opt), addr_(addr), socket_(socket) {

    kcp_ = ikcp_create(kcp_opt_.conv, this);
    kcp_->output = udp_output;
    TRACE("addr:", GetAddrToString());
    TRACE("kcp_opt.conv:", kcp_opt.conv);
    TRACE("kcp_opt.is_server:", kcp_opt.is_server);
    TRACE("kcp_opt.keep_alive_timeout: ", kcp_opt_.keep_alive_timeout);
    TRACE("kcp_opt.sndwnd:", kcp_opt.sndwnd);
    TRACE("kcp_opt.rcvwnd:", kcp_opt.rcvwnd);
    TRACE("kcp_opt.nodelay:", kcp_opt.nodelay);
    TRACE("kcp_opt.resend:", kcp_opt.resend);
    TRACE("kcp_opt.nc:", kcp_opt.nc);
    // ikcp_wndsize(kcp_, 128, 128);
    // ikcp_nodelay(kcp_, 0, 10, 0, 0);
    // 配置窗口大小：平均延迟200ms，每20ms发送一个包，
	// 而考虑到丢包重发，设置最大收发窗口为128
	ikcp_wndsize(kcp_, kcp_opt.sndwnd, kcp_opt.rcvwnd);
    // 第二个参数 nodelay-启用以后若干常规加速将启动
    // 第三个参数 interval为内部处理时钟，默认设置为 10ms
    // 第四个参数 resend为快速重传指标， 
    // 第五个参数 为是否禁用常规流控， 
    ikcp_nodelay(kcp_, kcp_opt.nodelay, kcp_opt.interval, kcp_opt.resend, kcp_opt.nc);
}

KcpSession::~KcpSession() {
    if (kcp_) {
        ikcp_release(kcp_);
        kcp_ = NULL;
    }
}

std::string KcpSession::GetAddrToString() const {
    std::stringstream ss;
    ss << inet_ntoa(addr_.sin_addr) << ":" << ntohs(addr_.sin_port);
    return ss.str();
}

/**
 * @brief: call it every 10 - 100ms repeatedly
 * @param[in] current timestamp in millsec
 * @return {bool}     whether peer connection offline
 */
bool KcpSession::Update(int64_t current) {
    {
        Lock lock(mtx_);
        ikcp_update(kcp_, (uint32_t)current);
    }
    // TRACE("current:", current , "recv_latest_time:", recv_latest_time,
    // "timeout:", kcp_opt_.keep_alive_timeout); TRACE("current -
    // recv_latest_time: ", current - recv_latest_time); 服务端才去检测是否超时
    if (recv_latest_time == 0)
        recv_latest_time = current;
    if (kcp_opt_.is_server &&  // 客户端的心跳检测，比如服务端 5000ms， 客户端1000ms发送一次
        (current - recv_latest_time > kcp_opt_.keep_alive_timeout)) {

        TRACE("conv: ", kcp_opt_.conv, " timeout");
        return true;
    }
    // 客户端检测到长时间没有发送数据则发送保活数据
    if (!kcp_opt_.is_server &&
        (current - send_latest_time > kcp_opt_.keep_alive_timeout)) {
        // TRACE("client keep alive: ", current/1000);
        send_latest_time = current; // 独立发送保活信息
        int ret = Send(KCP_KEEP_ALIVE_CMD, sizeof(KCP_KEEP_ALIVE_CMD));
    }

    return false;
}
/**
 * @brief input data parser to KcpSeg
 * @param[in] data
 * @param[in] size
 * @return  0 : success |-1 : 输入数据太短,比如都不够kcp的header大小 | -2 :
 * wrong conv | -3 : data error
 */
int KcpSession::Input(void *data, std::size_t size) {
    Lock lock(mtx_);
    int ret = ikcp_input(kcp_, (const char *)data, size);
    // TRACE("ikcp_input ret:", ret);
    return ret;
}
/**
 * @brief  用户数据发送给kcp内部处理，由kcp内部调用sendto发送给对端
 * @param[in] data
 * @param[in] size
 * @return  0 : success | -1 : failed
 */
int KcpSession::Send(const void *data, uint32_t size) {
    Lock lock(mtx_);
    int ret = ikcp_send(kcp_, (const char *)data, size);
    // TRACE("ikcp_send ret:", ret);
    return ret;
}
/**
 * @brief 从 Kcp接收已经对端发送过来的用户数据
 * @param[in,out] data
 * @param[in]  size
 * @return  -3: 输入buffer小于要读取的数据大小| -2 : 没有数据可以读取 | -1 :
 * 队列为空 |  > 0 : data size
 */
int KcpSession::Recv(void *buf, uint32_t size) {
    Lock lock(mtx_);
    int ret = ikcp_recv(kcp_, (char *)buf, size);
    // TRACE("ikcp_recv ret:", ret);
    return ret;
}

// 调用socket发送数据，这个在session内部使用
int KcpSession::sendTo(const void *buf, std::size_t size, int flag) {
    int ret = socket_->SendTo(buf, size, addr_, flag);
    return ret;
}
