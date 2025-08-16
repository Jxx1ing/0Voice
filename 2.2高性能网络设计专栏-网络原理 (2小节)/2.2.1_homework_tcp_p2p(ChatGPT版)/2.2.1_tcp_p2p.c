// tcp_p2p_epoll.c
// Build: gcc -O2 -Wall -Wextra -o tcp_p2p_epoll tcp_p2p.c
// Usage: ./tcp_p2p <local_ip> <local_port> <remote_ip> <remote_port>

#define _GNU_SOURCE      // 启用 GNU 扩展（如 localtime_r 等）
#include <arpa/inet.h>   // inet_pton/ntop 等 IP 操作
#include <errno.h>       // errno 错误码
#include <fcntl.h>       // fcntl 设置非阻塞
#include <netinet/in.h>  // sockaddr_in、htons 等
#include <netinet/tcp.h> // TCP 选项（TCP_NODELAY）
#include <signal.h>      // 信号处理
#include <stdio.h>       // printf、fprintf
#include <stdlib.h>      // atoi、malloc、realloc、free
#include <string.h>      // memset、strerror、memcpy
#include <sys/epoll.h>   // epoll_create1/epoll_ctl/epoll_wait
#include <sys/select.h>  // 仅为与 gettimeofday/struct timeval 的历史兼容
#include <sys/socket.h>  // socket/bind/listen/connect/accept
#include <sys/time.h>    // gettimeofday
#include <time.h>        // struct tm、localtime_r
#include <unistd.h>      // close/read/write

// ---------------------- 全局与工具函数 ----------------------
static volatile sig_atomic_t g_stop = 0; // 信号控制退出标志: 告诉主循环该退出了
static void on_sigint(int signo)         // 信号处理器：置退出标志
{
    (void)signo;
    g_stop = 1;
}

static int set_nonblock(int fd) // 把 fd 设为非阻塞
{
    int fl = fcntl(fd, F_GETFL, 0); // 读取原标志
    if (fl < 0)
        return -1;                              // 失败返回
    return fcntl(fd, F_SETFL, fl | O_NONBLOCK); // 加上 O_NONBLOCK
}

static int set_nodelay(int fd) // 禁用 Nagle，降低交互延迟
{
    int on = 1; // 开关值 1
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
}

static int set_reuse(int fd) // 开启地址/端口复用，便于快速重启
{
    int on = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
        return -1; // 地址复用
#ifdef SO_REUSEPORT
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on)); // 端口复用（忽略失败）
#endif
    return 0;
}

static const char *ts() // 生成“HH:MM:SS.mmm”时间戳字符串
{
    static char buf[64]; // 静态缓冲（单线程安全）
    struct timeval tv;
    gettimeofday(&tv, NULL); // 当前时间（含微秒）
    struct tm tm;
    localtime_r(&tv.tv_sec, &tm);                      // 本地时区
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03ld", // 格式化输出
             tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec / 1000);
    return buf; // 返回指针（注意非线程安全）
}

static void perrorx(const char *tag) // 打印带时间戳的错误日志
{
    fprintf(stderr, "[%s] %s: %s\n", ts(), tag, strerror(errno));
}

// 一个简单的可增长发送缓冲区结构（用于处理 send 部分写/EAGAIN）
typedef struct outbuf
{
    char *data; // 缓冲区指针
    size_t len; // 有效数据总长度
    size_t off; // 已发送偏移（[off, len) 未发送）
} outbuf_t;

static void outbuf_init(outbuf_t *b) // 初始化发送缓冲
{
    b->data = NULL;
    b->len = 0;
    b->off = 0;
}

static void outbuf_free(outbuf_t *b) // 释放缓冲
{
    free(b->data);
    b->data = NULL;
    b->len = b->off = 0;
}

static int outbuf_empty(const outbuf_t *b) // 判空：无待发送数据
{
    return b->off >= b->len;
}

static int outbuf_compact(outbuf_t *b) // 压缩：丢弃已发送的数据
{
    if (b->off == 0)
        return 0; // 无需压缩
    if (b->off >= b->len)
    {
        b->off = b->len = 0;
        return 0;
    } // 全部发送完
    memmove(b->data, b->data + b->off, b->len - b->off); // 前移剩余部分
    b->len -= b->off;
    b->off = 0; // 更新元数据
    return 0;
}
/*
void *memmove(void *dest, const void *src, size_t n);
dest：搬移目的地，这里是 b->data，也就是缓冲区最开始。
src：搬移来源，这里是 b->data + b->off，即“跳过已消费部分”的位置。
n：要搬移的字节数，这里是 b->len - b->off，也就是“还剩多少字节没处理”。
*/

static int outbuf_append(outbuf_t *b, const char *p, size_t n) // 追加数据到缓冲尾部
{
    if (n == 0)
        return 0;                   // 无需追加
    size_t avail = b->len - b->off; // 剩余未发字节数
    if (b->off > 0 && avail < b->len)
        outbuf_compact(b);                         // 若有空间浪费则压缩
    size_t used = b->len;                          // 当前有效长度
    char *nd = (char *)realloc(b->data, used + n); // 扩容
    if (!nd)
        return -1;                // 内存不足
    b->data = nd;                 // 更新指针
    memcpy(b->data + used, p, n); // 拷贝追加数据
    b->len = used + n;            // 更新总长度
    return 0;                     // 成功
}

static int outbuf_flush_fd(outbuf_t *b, int fd) // 尝试把缓冲发送到 fd
{
    while (b->off < b->len)
    {                                                               // 还有未发送数据
        ssize_t m = send(fd, b->data + b->off, b->len - b->off, 0); // 尝试发送
        if (m > 0)
        {
            b->off += (size_t)m;
        } // 前进偏移
        else
        {
            // 内核发送缓冲区已满（比如网络太慢，或者对方没及时接收） → send() 会阻塞，一直等到有空间才能返回。
            if (m < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                return 1; // 写阻塞，等待 EPOLLOUT
            if (m < 0 && errno == EINTR)
                continue; // 被信号打断，重试
            return -1;    // 其它错误
        }
    }
    if (b->off >= b->len)
    {
        b->off = b->len = 0;
    } // 全部写完，清空
    return 0; // 写完
}

// epoll 工具函数：包装 epoll_ctl 的常用用法
static int ep_add(int epfd, int fd, uint32_t ev) // epoll_ctl(ADD)
{
    struct epoll_event e;
    e.events = ev;
    e.data.fd = fd;                                // 设置事件与携带的 fd
    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &e); // 添加到 epoll
}
static int ep_mod(int epfd, int fd, uint32_t ev) // epoll_ctl(MOD)
{
    struct epoll_event e;
    e.events = ev;
    e.data.fd = fd; // 修改监听事件
    return epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &e);
}
static int ep_del(int epfd, int fd) // epoll_ctl(DEL)
{
    return epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
}

// --------------------------- 主程序 ---------------------------
int main(int argc, char **argv)
{
    if (argc != 5)
    { // 参数检查
        fprintf(stderr, "Usage: %s <local_ip> <local_port> <remote_ip> <remote_port>\n", argv[0]);
        return 1; // 参数不足
    }

    const char *local_ip = argv[1];  // 本地 IP
    int local_port = atoi(argv[2]);  // 本地端口
    const char *remote_ip = argv[3]; // 远端 IP
    int remote_port = atoi(argv[4]); // 远端端口

    signal(SIGINT, on_sigint);  // 注册 SIGINT 处理器（Ctrl+C）
    signal(SIGTERM, on_sigint); // 注册 SIGTERM 处理器

    // ---------- 构造监听 socket：lfd ----------
    // 绑定到本地地址
    int lfd = socket(AF_INET, SOCK_STREAM, 0); // 创建 IPv4 TCP 套接字（监听用）
    if (lfd < 0)
    {
        perrorx("socket(lfd)");
        return 2;
    } // 创建失败
    if (set_reuse(lfd) < 0)
        perrorx("setsockopt(SO_REUSE*)"); // 地址复用

    struct sockaddr_in laddr;
    memset(&laddr, 0, sizeof(laddr));                       // 准备本地地址
    laddr.sin_family = AF_INET;                             // IPv4
    laddr.sin_port = htons(local_port);                     // 端口转网络序
    if (inet_pton(AF_INET, local_ip, &laddr.sin_addr) != 1) // 把 local_ip 转换成 IPv4 二进制地址，存到 laddr.sin_addr 里
    {                                                       // 解析本地 IP
        fprintf(stderr, "Invalid local_ip: %s\n", local_ip);
        return 2;
    }
    // ①第一次bind
    if (bind(lfd, (struct sockaddr *)&laddr, sizeof(laddr)) < 0)
    { // 绑定
        perrorx("bind(lfd)");
        return 2;
    }
    if (listen(lfd, 16) < 0)
    {
        perrorx("listen(lfd)");
        return 2;
    } // 监听
    if (set_nonblock(lfd) < 0)
        perrorx("set_nonblock(lfd)"); // 非阻塞

    printf("[%s] Listening on %s:%d\n", ts(), local_ip, local_port); // 打印监听地址

    // ---------- 构造主动连接 socket：cfd ----------
    // 尝试连接远端地址
    int cfd = socket(AF_INET, SOCK_STREAM, 0); // 创建 IPv4 TCP 套接字（主动连接）
    if (cfd < 0)
    {
        perrorx("socket(cfd)");
        close(lfd);
        return 2;
    } // 失败清理
    if (set_reuse(cfd) < 0)
        perrorx("setsockopt(SO_REUSE*)"); // 地址复用
    // ②第二次bind
    if (bind(cfd, (struct sockaddr *)&laddr, sizeof(laddr)) < 0)
    {                         // 绑定相同本地 addr/port（失败可忽略）
        perrorx("bind(cfd)"); // 打印但不中止
    }
    if (set_nonblock(cfd) < 0)
        perrorx("set_nonblock(cfd)"); // 非阻塞 connect

    struct sockaddr_in raddr;
    memset(&raddr, 0, sizeof(raddr)); // 远端地址
    raddr.sin_family = AF_INET;
    raddr.sin_port = htons(remote_port);
    if (inet_pton(AF_INET, remote_ip, &raddr.sin_addr) != 1)
    { // 解析远端 IP
        fprintf(stderr, "Invalid remote_ip: %s\n", remote_ip);
        close(lfd);
        close(cfd);
        return 2;
    }

    int conn_inflight = 0; // 是否处于连接中
    if (connect(cfd, (struct sockaddr *)&raddr, sizeof(raddr)) == 0)
    {                      // 非阻塞下也可能立刻连上（本机/快路径）
        conn_inflight = 0; // 已经连接成功
        printf("[%s] Outgoing connect() succeeded immediately.\n", ts());
    }
    else
    {
        if (errno == EINPROGRESS)
        {                      // 典型：返回进行中
            conn_inflight = 1; // 标记进行中
            printf("[%s] Outgoing connect() in progress to %s:%d ...\n", ts(), remote_ip, remote_port);
        }
        else
        { // 其它错误
            perrorx("connect(cfd)");
            conn_inflight = 0; // 连接失败
        }
    }

    /*
    （1）2个fd的作用
        fd	作用	                    方向	            需要绑定的理由
        lfd	监听（accept）	             被动	            让对端可以 主动连进来；必须在 listen() 之前 bind()
        cfd	发起连接（connect）	         主动	            让本端发出去的 SYN 拥有 与 STUN 检测一致的源端口，否则 NAT 会映射成新端口
    （2）为什么需要bind两次
        一般情况下，connect() 不需要手动 bind()，操作系统会随机分配一个临时端口（ephemeral port）。
        但在 P2P 场景下，你必须保证 主动连出去时使用的源端口 = 监听端口。
        因为 NAT 打洞靠的就是 固定端口映射，对端只能打到你这个端口，如果主动连接时换了端口，对方就联系不到你了。
-----------------------------------------------------------------------------------------------------------------------------------
        NAT 打洞逻辑（结合 STUN/P2P）
        lfd 负责接收：当对方主动 connect 过来时，你能 accept。
        cfd 负责发起：当你主动 connect 对方时，源端口必须等于 lfd 的端口，这样 NAT 设备的映射才一致，对方打过来的数据包也能找到你。
    (3)为什么需要 SO_REUSEADDR / SO_REUSEPORT
        因为 (local_ip, local_port) 已经被 lfd 绑定过了。
        必须开复用，才能让两个 fd 共享这个端口：
    */

    // ---------- 创建 epoll，并注册初始关注项 ----------
    int epfd = epoll_create1(0); // 创建 epoll 实例
    if (epfd < 0)
    {
        perrorx("epoll_create1");
        close(lfd);
        if (cfd >= 0)
            close(cfd);
        return 2;
    }

    // 关注：监听 socket 的读事件（有新连接可 accept）
    if (ep_add(epfd, lfd, EPOLLIN | EPOLLRDHUP) < 0)
        perrorx("epoll_ctl ADD lfd");

    // 关注：若 connect 正在进行，则关注 cfd 的写事件（连接完成信号）及错误/挂断
    if (conn_inflight)
    {
        if (ep_add(epfd, cfd, EPOLLOUT | EPOLLERR | EPOLLHUP) < 0)
            perrorx("epoll_ctl ADD cfd(OUT)");
    }

    // 关注：标准输入（键盘）可读，用于发送消息
    set_nonblock(STDIN_FILENO); // 把 stdin 也设为非阻塞
    if (ep_add(epfd, STDIN_FILENO, EPOLLIN) < 0)
        perrorx("epoll_ctl ADD stdin");

    int established_fd = -1; // 真正用来聊天的连接 fd
    int active_path = 0;     // 1=accept; 2=connect

    struct epoll_event evs[16]; // 事件数组（足够小 demo）

    // ---------- 第一阶段：等待 accept 或 connect 完成 ----------
    while (!g_stop && established_fd < 0)
    {                                                                            // 尚未打通 P2P
        int n = epoll_wait(epfd, evs, (int)(sizeof(evs) / sizeof(evs[0])), 200); // 200ms 轮询
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            perrorx("epoll_wait(stage1)");
            break;
        }
        for (int i = 0; i < n; ++i)
        { // 遍历事件
            int fd = evs[i].data.fd;
            uint32_t e = evs[i].events; // 取出 fd 与事件位

            if (fd == lfd && (e & EPOLLIN))
            { // 监听 fd 有新连接
                struct sockaddr_in peer;
                socklen_t plen = sizeof(peer);
                int afd = accept(lfd, (struct sockaddr *)&peer, &plen); // 非阻塞 accept
                if (afd >= 0)
                {
                    char ip[64];
                    inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
                    printf("[%s] accept() from %s:%d\n", ts(), ip, ntohs(peer.sin_port));
                    set_nonblock(afd);
                    set_nodelay(afd); // 新连接也设非阻塞+无 Nagle
                    established_fd = afd;
                    active_path = 1; // 使用 accept 路径
                    // 既然已建立，后面阶段会重新注册需要的事件；先跳出本轮
                }
            }

            // 这一段代码：确定刚才非阻塞 connect() 最终到底 成功 还是 失败
            if (conn_inflight && fd == cfd && (e & (EPOLLOUT | EPOLLERR | EPOLLHUP)))
            { // 连接结果
                int err = 0;
                socklen_t len = sizeof(err);
                if (getsockopt(cfd, SOL_SOCKET, SO_ERROR, &err, &len) < 0) // 把某个 socket 的内核级选项值取出来(这里是 SO_ERROR )
                {
                    perrorx("getsockopt(SO_ERROR)");
                    conn_inflight = 0; // 失败，停止等待
                    ep_del(epfd, cfd); // 从 epoll 移除
                }
                else if (err == 0)
                { // 连接成功
                    printf("[%s] Outgoing connect() completed to %s:%d\n", ts(), remote_ip, remote_port);
                    set_nodelay(cfd); // 禁用 Nagle
                    established_fd = cfd;
                    cfd = -1;
                    active_path = 2; // 使用 connect 路径
                }
                else
                { // 连接失败
                    fprintf(stderr, "[%s] connect failed: %s\n", ts(), strerror(err));
                    conn_inflight = 0;
                    ep_del(epfd, fd); // 移除并等待对方接入
                }
            }
        }
        if (established_fd >= 0)
            break; // 成功即跳出
    }

    // 清理不再需要的 fd：cfd/lfd
    if (cfd >= 0)
    {
        ep_del(epfd, cfd);
        close(cfd);
    } // 仍在 epoll 的话移除
    if (lfd >= 0)
    {
        ep_del(epfd, lfd);
        close(lfd);
    }

    if (established_fd < 0)
    { // 两条路径都没成功
        fprintf(stderr, "P2P connection not established.\n");
        close(epfd);
        return 3; // 退出
    }

    printf("[%s] P2P established via %s path. You can now type messages.\n",
           ts(), (active_path == 1 ? "ACCEPT" : "CONNECT")); // 提示路径

    // ---------- 第二阶段：已连接的聊天循环（epoll 驱动） ----------
    // 注册已建立连接的读事件；写事件按需开启（当有待发送数据时）
    if (ep_add(epfd, established_fd, EPOLLIN | EPOLLRDHUP) < 0)
        perrorx("epoll_ctl ADD established_fd");

    outbuf_t ob;
    outbuf_init(&ob); // 发送缓冲

    while (!g_stop)
    {                                                                            // 聊天主循环
        int n = epoll_wait(epfd, evs, (int)(sizeof(evs) / sizeof(evs[0])), 200); // 200ms 轮询
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            perrorx("epoll_wait(stage2)");
            break;
        }

        for (int i = 0; i < n; ++i)
        { // 处理事件
            int fd = evs[i].data.fd;
            uint32_t e = evs[i].events;

            // 1) 标准输入可读：读取用户输入，追加到发送缓冲，并打开 EPOLLOUT
            if (fd == STDIN_FILENO && (e & EPOLLIN))
            {
                char ibuf[4096];                                    // 输入缓冲
                ssize_t m = read(STDIN_FILENO, ibuf, sizeof(ibuf)); // 读取键盘输入
                if (m > 0)
                {
                    if (outbuf_append(&ob, ibuf, (size_t)m) < 0)
                    { // 追加到发送缓冲
                        fprintf(stderr, "[%s] outbuf_append: no memory\n", ts());
                        g_stop = 1;
                        break;
                    }
                    // 注册/打开写事件（若尚未打开）
                    uint32_t ev = EPOLLIN | EPOLLRDHUP; // 默认情况下，我们只需要监听 读事件（EPOLLIN）和 对端关闭事件（EPOLLRDHUP）
                    if (!outbuf_empty(&ob))             // 如果缓冲区里有数据没发完（说明 send() 上次返回了 EAGAIN），就必须让 epoll 同时关注 写事件（EPOLLOUT）。
                        ev |= EPOLLOUT;                 // 有待发送 => 关注写
                    if (ep_mod(epfd, established_fd, ev) < 0)
                        perrorx("epoll_ctl MOD +EPOLLOUT");
                }
                else if (m == 0)
                {
                    // stdin 关闭（例如 Ctrl+D），可选择半关闭写端(我这边不会再发数据了，但我还能收你的数据)；此处简单记日志
                    printf("[%s] STDIN closed (EOF).\n", ts());
                    // 不立即退出，允许继续接收对方消息；若想退出可设置 g_stop=1
                }
                else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                {
                    perrorx("read(stdin)");
                    g_stop = 1;
                    break; // 输入错误
                }
            }

            // 2) 已建立连接的对端可读/挂断：接收数据或处理关闭
            if (fd == established_fd && (e & (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR))) // e & 0x2219:当e中任意一个标志位被设置时条件成立
            {
                if (e & (EPOLLRDHUP | EPOLLHUP))
                { // 对端半关闭/挂断
                    printf("[%s] Peer closed (HUP/RDHUP).\n", ts());
                    g_stop = 1;
                    break; // 退出循环
                }
                if (e & EPOLLIN)
                {                                                                // 可读
                    char rbuf[4096];                                             // 接收缓冲
                    ssize_t m = recv(established_fd, rbuf, sizeof(rbuf) - 1, 0); // 读取
                    if (m > 0)
                    {
                        rbuf[m] = '\0';
                        printf("[%s] RECV: %s", ts(), rbuf);
                    } // 打印收到内容
                    else if (m == 0)
                    {
                        printf("[%s] Peer closed.\n", ts());
                        g_stop = 1;
                        break;
                    } // 正常关闭
                    else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                    { // 真正错误
                        perrorx("recv");
                        g_stop = 1;
                        break; // 退出
                    }
                }
            }

            // 3) 套接字可写：把缓冲中未发送完的数据尽量发送出去（这里的数据来自stdin, 不是对端）
            if (fd == established_fd && (e & EPOLLOUT))
            {
                int r = outbuf_flush_fd(&ob, established_fd); // 冲刷发送缓冲
                if (r < 0)
                {
                    perrorx("send");
                    g_stop = 1;
                    break;
                } // 发送错误
                if (r == 0)
                {                                                               // 全部写完
                    if (ep_mod(epfd, established_fd, EPOLLIN | EPOLLRDHUP) < 0) // 关闭写事件关注（去掉 EPOLLOUT，只保留：EPOLLIN：对端可读事件 & EPOLLRDHUP：对端半关闭 / 挂断）
                        perrorx("epoll_ctl MOD -EPOLLOUT");
                }
            }
            /*
                1.用户输入
            在终端输入了一段消息，read(STDIN_FILENO) 把数据放进 ob。
                2.尝试发送
            调用 send() 把数据发到 established_fd。
            如果内核发送缓冲区满了，send() 会返回 EAGAIN/EWOULDBLOCK，表示还没发完。
                3.在ob中有未发送完的数据
            未发送的数据会留在 outbuf 里（off < len）。
            这时候必须关注 EPOLLOUT，等套接字可写，再继续发送。
                4.发送完成
            outbuf_flush_fd() 成功把剩余数据全部写出后，outbuf 会被清空（off = len = 0）。
            然后程序就会用 ep_mod() 关闭写事件关注，只继续监听读事件。
            */
        }
    }

    // ---------- 清理资源 ----------
    outbuf_free(&ob);             // 释放发送缓冲
    ep_del(epfd, established_fd); // 从 epoll 移除
    close(established_fd);        // 关闭连接
    ep_del(epfd, STDIN_FILENO);   // 可省略（内核会回收）
    close(epfd);                  // 关闭 epoll

    printf("[%s] Exit.\n", ts()); // 结束日志
    return 0;                     // 正常结束
}
