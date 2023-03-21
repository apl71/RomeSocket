#ifndef ROMESOCKET_HPP_
#define ROMESOCKET_HPP_

#include "thread_pool.hpp"
#include <cstdint>
#include <cstring>
#include <liburing.h>
#include <memory>
#include <mutex>
#include <map>
#include "layer.h"
#include <sodium.h>
#include <fstream>

#define REQUEST_TYPE_ACCEPT 1
#define REQUEST_TYPE_READ 2
#define REQUEST_TYPE_WRITE 3

constexpr unsigned SERVER_HEADER_LENGTH = 3;

// 输出颜色
const std::string yellow = "\033[0;33m";
const std::string green  = "\033[0;32m";
const std::string red    = "\033[0;31m";
const std::string reset  = "\033[0m";

enum LogLevel {
    INFO = 0,
    WARNING = 1,
    ERROR = 2
};

struct Request {
    int type;
    int client_sock;
    char *buff;
    // 仅当使用register buffer且type为REQUEST_TYPE_READ时有效
    int buffer_index = -1;
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
    uint16_t _port = 9999;
    // 缓冲区的最大长度
    size_t _max_buffer_size = BLOCK_LENGTH;

    // socket超时时间，在该时间内没有任何通信的客户将断开连接
    time_t timeout = 30;

    // io uring
    io_uring _ring;
    size_t _ring_size = 32;
    std::mutex _ring_mutex;

    int _max_connection = 300;

    // 工作线程池
    ThreadPool *pool;

    // 日志文件
    std::string log_file = "";
    bool colored;

    // 暂存未完成的读入
    struct ReadBuffer {
        Buffer incomplete;
        int offset;
    };
    std::map<int, ReadBuffer> read_queue;

    // 暂存未完成的io
    std::map<int, std::vector<Buffer>> wait_queue;

    // 保存连接信息
    // int -> client_id
    std::map<int, Client> clients;
    // 服务器密钥对
    unsigned char server_pk[crypto_kx_PUBLICKEYBYTES];
    unsigned char server_sk[crypto_kx_SECRETKEYBYTES];

    // 日志锁
    std::mutex log_lock;

    // 控制是否使用register buffer
    bool register_buffer = false;
    // 当前可用的index
    int register_buffer_index = 0;
    iovec *piov = nullptr;

    // 初始化套接字
    void Initialize(int port);
    // 提交所有任务
    void Submit();
    // 将一个接受用户的请求加入sq
    int PrepareAccept(struct sockaddr *client_addr, socklen_t *addr_len);
    // 将一个读事件的请求加入sq
    int PrepareRead(int client_sock, int size);
    // 将一个写事件的请求加入sq
    int PrepareWrite(int client_sock, char *to_write, size_t size, bool link = false);
    // 将Request结构体清空
    void FreeRequest(Request **request);
    // 查看读入新块后，是否有新块可以处理，如有，返回该块
    Buffer CheckFullBlock(int client_sock, char *new_buffer, int read_size);
    // 将字符串计入日志 level: 0=绿色info 1=黄色warning 2=红色error
    void Log(const std::string &data, int level);
    // 返回当前可用的buffer index
    int GetValidIndex();

public:
    // 初始化对象
    Rocket();
    // 初始化对象，确定要监听的端口
    Rocket(uint16_t port);
    ~Rocket();

    // 设置端口
    void SetPort(uint16_t port);

    // 设置io_uring队列长度，在调用Start()前调用
    void SetRingSize(size_t size);

    // 启动事件
    virtual void OnStart() {}

    // 启动服务器
    void Start();

    // 周期事件，每次循环都会执行（约0.5s一次）
    virtual void ChronicTask(time_t now) {}

    // 注册事件
    virtual void OnRead(char *buffer, size_t size, int client_id) = 0;

    int Write(char *buffer, size_t size, int client_id, bool more);

    // 不需要写回信息，但需要继续读的情况，可以调用Pass
    void Pass(int client_id);

    // 设定日志信息
    void SetLogFile(std::string log, bool color);

    // 开关register buffer，必须在调用Start()前调用，且Start()后不能再更改
    void SetRegisterBuffer(bool on);
};

#endif