#include <stdio.h>
#include <liburing.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>

#define ENTRIES_LENGTH 1024 // 队列大小
#define BUFFER_LENGTH 1024
#define EVENT_ACCEPT 0 // 自定义事件类型
#define EVENT_READ 1
#define EVENT_WRITE 2
#define INFO printf

struct conn_info
{
    int fd;    // 文件描述符
    int event; // 事件类型
}; // 8字节

// 初始化服务器
int init_server(unsigned short port)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sockfd)
    {
        perror("socket error");
        return -1;
    }

    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(struct sockaddr_in));

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(port);

    if (-1 == bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(struct sockaddr)))
    {
        perror("bind error");
        return -1;
    }

    if (-1 == listen(sockfd, 10))
    {
        perror("listen error");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

/*io_uring和sockfd的含义不同
 io_uring管理sq/cq队列的指针；
 sq队列的单位是sqe（提交队列条目），sqe中包含sockfd。
*/
/*核心函数
io_uring_prep_recv：填充sqe结构体，准备1个recv操作（告诉内核要recv。这一步把buffer地址填入sqe，最终接收的数据保存在buffer中），
io_uring_submit： 把recv请求发给内核（告诉内核去执行recv），
io_uring_wait_cqe: 阻塞等待内核（至少）完成1个请求
io_uring_peek_batch_cqe: 获取所有已经完成的cqe（完成队列条目），使用cqe->res确认结果。
*/
// 封装accept/recv/send（调用io_uring_prep_accept/recv/send）
int set_event_accept(struct io_uring *ring, int sockfd, struct sockaddr *addr,
                     socklen_t *addrlen, int flags)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    struct conn_info accept_info = {
        .fd = sockfd,
        .event = EVENT_ACCEPT,
    }; // 栈上临时变量，用来拷贝给sqe结构体中的user_data字段（8字节，注意conn_info也正好是8字节（4+4））
    io_uring_prep_accept(sqe, sockfd, (struct sockaddr *)addr, addrlen, flags);
    memcpy(&sqe->user_data, &accept_info, sizeof(struct conn_info));
}

int set_event_recv(struct io_uring *ring, int sockfd,
                   void *buf, size_t len, int flags)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    struct conn_info recv_info = {
        .fd = sockfd,
        .event = EVENT_READ,
    };
    io_uring_prep_recv(sqe, sockfd, buf, len, flags);
    memcpy(&sqe->user_data, &recv_info, sizeof(struct conn_info));
}

int set_event_send(struct io_uring *ring, int sockfd,
                   void *buf, size_t len, int flags)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    struct conn_info send_info = {
        .fd = sockfd,
        .event = EVENT_WRITE,
    };
    io_uring_prep_send(sqe, sockfd, buf, len, flags);
    memcpy(&sqe->user_data, &send_info, sizeof(struct conn_info));
}

int main(int argc, char *argv[])
{
    // 初始化服务器
    unsigned short port = 9999;
    int socket = init_server(port); // 监听9999端口
    // 初始化io_uring结构
    struct io_uring_params params;
    memset(&params, 0, sizeof(params));
    struct io_uring ring;
    io_uring_queue_init_params(ENTRIES_LENGTH, &ring, &params);
    /*io_uring_queue_init_params：
     调用了io_uirng_setup
     mmap sq/cq共享内存
    */

    // 提交accept请求
    struct sockaddr_in clientaddr;
    socklen_t len = sizeof(clientaddr);
    set_event_accept(&ring, socket, (struct sockaddr *)&clientaddr, &len, 0);

    /* 事件循环
     1.等待内核完成请求
     2.获取完成的cqe，判断事件类型（accept/read/write）
     3.根据事件类型处理事件（如果是accept，提交recv请求；如果是read，提交send请求；如果是write，提交recv请求）
    */
    char buffer[BUFFER_LENGTH] = {0};
    while (1)
    {
        // 1-提交SQ
        /*io_uring_submit
         更新SQ tail(提交新请求)
         调用了io_uring_enter，通知内核有新请求
         内核读取SQE
        */
        io_uring_submit(&ring);

        // 2-等待完成
        struct io_uring_cqe *cqe;
        io_uring_wait_cqe(&ring, &cqe); // 阻塞等待，知道至少有一个请求完成，然后返回第一个完成的CQE(类似epoll_wait)
        // 3-批量取完成事件
        struct io_uring_cqe *cqes[ENTRIES_LENGTH * 2];                         // CQ队列长度一般是SQ队列长度的2倍（示例中使用了128大小）
        int nready = io_uring_peek_batch_cqe(&ring, cqes, ENTRIES_LENGTH * 2); // 从共享内存读取CQ
        // 4-处理完成事件（依次处理每个完成队列条目cqe，根据每个cqe的user_data的event进行对应处理）
        for (int i = 0; i < nready; i++)
        {
            struct io_uring_cqe *entries = cqes[i];
            // 读取每个队列条目cqe的user_data（fd,event），判断事件类型
            struct conn_info result;
            memcpy(&result, &entries->user_data, sizeof(struct conn_info));
            // 如果是accept事件，继续提交accept请求（保持监听），同时提交recv请求（立即接收客户端发来的数据）
            if (result.event == EVENT_ACCEPT)
            {
                INFO("set_event_accept\n");
                //  再次提交accept请求，保持监听（为下次客户端请求做准备）
                set_event_accept(&ring, socket, (struct sockaddr *)&clientaddr, &len, 0);
                // 立即提交recv请求,立即接收客户端发来的数据
                int connfd = entries->res;
                set_event_recv(&ring, connfd, buffer, BUFFER_LENGTH, 0);
            }
            else if (result.event == EVENT_READ)
            {
                // 如果是read事件，提交send请求（这里采用把收到的数据发回客户端）
                int ret = entries->res;
                INFO("set_event_READ: %d,%s\n", ret, buffer);
                if (ret <= 0)
                {
                    close(result.fd);
                }
                else
                {
                    // ret > 0
                    set_event_send(&ring, result.fd, buffer, ret, 0);
                }
            }
            else if (result.event == EVENT_WRITE)
            {
                // 如果是write事件，继续提交recv请求（继续接收客户端发来的数据）
                int ret = entries->res;
                INFO("set_event_write: %d,%s\n", ret, buffer);
                set_event_recv(&ring, result.fd, buffer, BUFFER_LENGTH, 0);
            }
        }
        // 批量处理已经完成的事件后，更新CQ head（告诉内核"这些CQE已经用完了，空间可以回收"）
        io_uring_cq_advance(&ring, nready);
    }
}

/*io_uring_cqe结构体：
struct io_uring_cqe
{
    __u64 user_data; // 提交时设置的标识数据（从 SQE 复制过来）
    __s32 res;       // 操作结果（accept: 新 fd；recv: 收到字节数；错误: -errno）
    __u32 flags;     // 完成标志
};
*/
