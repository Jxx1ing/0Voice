
#include "kcp_client.h"
#include "trace.h"
#include <thread>


// 基于KcpClient封装业务
class ChatClient : KcpClient {
  public:
    using KcpClient::KcpClient;

    void Start() {
        std::thread t([this]() {
            while (true) {
                usleep(10);     // 客户端

                if (!Run()) {
                    TRACE("error ouccur");
                    break;
                }
            }
        });

        while (true) {
            std::string msg;
            std::getline(std::cin, msg);        // 读取用户输入数据
            Send(msg.data(), msg.length());     // 发送消息
        }
        t.join();   // 等待线程退出
    }
    // 服务端发送过来的消息
    virtual void HandleMessage(const std::string &msg) override {
        std::cout << msg << std::endl;
    }
    virtual void HandleClose() override {
        std::cout << "close kcp connection!" << std::endl;
        exit(-1);
    }
};

int main(int argc, char *argv[]) {
    // arg : ip + port
    if (argc != 3) {
        TRACE("args error please input [ip] [port]");
        exit(-1);
    }

    std::string ip = argv[1];
    uint16_t port = atoi(argv[2]);

    srand(time(NULL));

    KcpOpt opt;
    opt.conv = rand() * rand(); // uuid的函数
    opt.is_server = false;
    opt.keep_alive_timeout = 1000; // 保活心跳包间隔，单位ms
    TRACE("conv = ", opt.conv); 
    ChatClient client(ip, port, opt);

    client.Start();

    return 0;
}