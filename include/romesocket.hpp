#ifndef ROMESOCKET_HPP_
#define ROMESOCKET_HPP_

#include "thread_pool.hpp"
#include <cstdint>
#include <cstring>
#include <liburing.h>
#include <memory>
#include <mutex>
#include <map>
#include <sodium.h>

#define REQUEST_TYPE_ACCEPT 1
#define REQUEST_TYPE_READ 2
#define REQUEST_TYPE_WRITE 3

constexpr unsigned SERVER_HEADER_LENGTH = 3;

struct Request {
    int type;
    int client_sock;
    char *buff;
};

struct Client {
    int connection;
    time_t last_time;
    unsigned char tx[crypto_kx_SESSIONKEYBYTES];
    unsigned char rx[crypto_kx_SESSIONKEYBYTES];
};

class Rocket {
private:
    int _sock = -1;
    uint16_t _port;
    // 缓冲区的最大长度
    size_t _max_buffer_size = 8192;

    // socket超时时间，在该时间内没有任何通信的客户将断开连接
    time_t timeout = 30;

    // io uring
    io_uring _ring;
    size_t _ring_size = 512;
    std::mutex _ring_mutex;

    int _max_connection = 300;

    // 工作线程池
    ThreadPool *pool;

    // 暂存未完成的io
    struct Buffer {
        char *buffer;
        char flag;
        uint16_t length;
    };
    // int -> client_id
    std::map<int, std::vector<Buffer>> wait_queue;

    // 保存连接信息
    // int -> client_id
    std::map<int, Client> clients;
    // 服务器密钥对
    unsigned char server_pk[crypto_kx_PUBLICKEYBYTES];
    unsigned char server_sk[crypto_kx_SECRETKEYBYTES];

    // 初始化套接字
    void Initialize(int port);
    // 提交所有任务
    void Submit();
    // 将一个接受用户的请求加入sq
    int PrepareAccept(struct sockaddr *client_addr, socklen_t *addr_len);
    // 将一个读事件的请求加入sq
    int PrepareRead(int client_sock);
    // 将一个写事件的请求加入sq
    int PrepareWrite(int client_sock, char *to_write, size_t size, bool link = false);
    // 将Request结构体清空
    void FreeRequest(Request **request);

public:
    // 初始化对象，确定要监听的端口
    Rocket(uint16_t port);
    ~Rocket();

    // 启动服务器
    void Start();

    // 注册事件
    virtual void OnRead(char *buffer, size_t size, int client_id) = 0;

    int Write(char *buffer, size_t size, int client_id, bool more);
};

#endif