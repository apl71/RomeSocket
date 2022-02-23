/*
    跨平台Socket接口
    by apl71
    2021.09.20
*/

#ifndef ROMESOCKET_H_
#define ROMESOCKET_H_

#include <string>
#include <functional>
#include <thread>
#include <vector>
#include <shared_mutex>
#include <openssl/ssl.h>

#include <iostream>

enum SOCKET_TYPE { TCP, UDP };
enum IP_VERSION { IPv4, IPv6 };
enum ROLE { SERVER, CLIENT, UNKNOWN };

#if defined(__linux__)
typedef int SocketHandle;
#elif defined(_WIN32)
typedef unsigned SocketHandle;
#endif

struct Thread
{
    std::thread::id tid;
    int conn = -1;
    bool is_main_thread = false;
    SSL *ssl;
};

class Socket
{
private:
    // 保存socket套接字句柄
    SocketHandle sock;
    // 由于各个平台的不可用套接字的值不同，需要该变量来表示套接字的可用性
    bool valid = false;
    // 保存线程号和连接
    std::vector<Thread> threads;
    // 保护threads用的锁
    std::shared_mutex thread_lock;
    // 指定客户端或服务端
    ROLE role = UNKNOWN;
    // 保存结构体
    void *addr = nullptr;
    // 结构体大小
    unsigned addr_size = 0;
    // 最大连接数量，不设置默认为200
    unsigned max_conn = 200;
    bool use_ssl = false;
    SSL_CTX *ctx = nullptr;
    SSL *client_ssl = nullptr; // 仅客户端使用
    BIO *bio; // 仅客户端使用
    std::string host;
    unsigned target_port;

    /*
        通过当前的线程号获得对应的连接句柄
        输入
            tid -- 当前的线程号
        输出
            整数，表示连接句柄，如果没有对应连接或连接不可用，返回-1
    */
    int GetConnection(std::thread::id tid);

    SSL *GetSSLConnection(std::thread::id tid);

    void ClearClosedConnection();

public:
    /*
        For debug
    */
    std::mutex print_mutex;
    void PrintThreadTable()
    {
        print_mutex.lock();
        std::cout << "my tid = " << std::this_thread::get_id() << std::endl;
        std::cout << "tid\tconn\tssl" << std::endl;
        for (auto i : threads)
        {
            std::cout << i.tid << "\t" << i.conn << "\t" << i.ssl << std::endl;
        }
        print_mutex.unlock();
    }


    Socket();

    ~Socket() { CloseSocket(); }

    /*
        创建套接字
        输入
            ip_version 使用的IP协议族，可选的值有IPv4和IPv6
            protocol 传输层协议，可选的值有TCP和UDP
        输出
            整数表示执行结果
            1   = 成功
            0   = 失败，该对象已经创建了一个可用的套接字
            -1  = 失败，IP类型不正确
            -2  = 失败，传输层协议不正确
            -3  = 失败，inet_pton错误
            -4  = 失败，未能绑定套接字
            -5  = 失败，未能监听端口
            -6  = 失败，指定CS类型错误
            -7  = 失败，未能创建套接字
            -8  = 失败，未能创建SSL环境
            -9  = 失败，未能生成私钥
            -10 = 失败，未能生成证书
    */
    int CreateSocket(IP_VERSION ip_version, SOCKET_TYPE protocol, ROLE r, unsigned port, std::string ip = "", bool ssl = true);

    /*
        检查当前套接字是否可用
        输出
            布尔值，表示当前套接字的可用性
    */
    bool IsValid() const;

    /*
        设置套接字的发送缓冲区长度
        输入
            bytes -- 套接字的缓冲长度
    */
    int SetSocketSendBuff(int bytes);

    /*
        设置套接字的接收缓冲区长度
        输入
            bytes -- 套接字的缓冲长度
    */
    int SetSocketReceiveBuff(int bytes);

    /*
        设置套接字接收超时时间，单位为秒
    */
    void SetReceiveTimeout(int seconds);

    /*
        开始接收客户端消息
        输出
            整数表示执行结果
            1  = 成功
            0  = 失败，非服务器端不能使用此函数
            -1 = 失败，已经开始接收客户端消息
            -2 = 失败，未能成功接收客户端请求
            -3 = 失败，套接字不可用
            -4 = 失败，SSL接收失败
    */
    int AcceptClient();

    /*
        连接到服务端
        输出
            整数表示执行结果
            1  = 成功
            0  = 失败，未能连接到服务器
            -1 = 失败，套接字不可用
            -2 = 失败，加密握手失败
    */
   int ConnectServer();

    /*
        发送消息
        输入
            send_buff 要发送的消息的缓冲区
            length 要发送的真实字节数
        输出
            已发送的字节数，若为负数，参阅以下错误代码
            -1 = 失败，套接字不可用
            -2 = 发送数据失败
            -3 = 未知错误
    */
    int SendData(unsigned char *send_buff, unsigned length);

    /*
        发送消息，此函数阻塞，持续发送直至指定长度的消息全部发送完毕
        输入
            send_buff 要发送的消息的缓冲区
            length 要发送的真实字节数
        输出
            已发送的字节数，若为负数，参阅以下错误代码
            -1 = 失败，套接字不可用
            -2 = 发送数据失败
            -3 = 未知错误
    */
    int SendDataFix(unsigned char *send_buff, unsigned length);
    int SendDataFix(char *send_buff, unsigned length)
    {
        return SendDataFix((unsigned char *)send_buff, length);
    }

    /*
        接收消息，此函数是阻塞的
        输入
            recv_buff 接收数据的缓冲区
            length 可用的缓冲区大小
        输出
            实际接收到的数据大小，若为负数，参阅以下错误代码
            -1 = 失败，套接字不可用
            -2 = 接收数据失败
            -3 = 失败，连接不可用
            -4 = 失败，未知错误
    */
    int ReceiveData(unsigned char *recv_buff, unsigned length);

    /*
        接收消息，与ReceiveData不同的是，该函数直到缓冲区被填满才会返回
        输入
            recv_buff 接收数据的缓冲区
            length 可用的缓冲区大小
        输出
            实际接收到的数据大小，若为负数，参阅以下错误代码
            -1 = 失败，套接字不可用
            -2 = 接收数据失败
            -3 = 未知错误
    */
    int ReceiveDataFix(unsigned char *recv_buff, unsigned length);
    int ReceiveDataFix(char *recv_buff, unsigned length)
    {
        return ReceiveDataFix((unsigned char *)recv_buff, length);
    }

    /*
        发送任意长度的字符串
    */
    int SendString(const std::string &str);

    /*
        接收任意长度的字符串
    */
    int ReceiveString(std::string &str);

    /*
        设置接收超时时间，单位为秒
    */
    int SetTimeout(int seconds);

    /*
        设置发送超时时间，单位为秒
    */

    /*
        重置连接
    */
    void ResetConnection();

    /*
        关闭连接
        输出
            整数表示执行结果
            1  = 成功
            0  = 失败，仅有服务器可以断开连接
            -1 = 失败，连接已经断开
    */
    int CloseConnection();

    /*
        关闭套接字
    */
    void CloseSocket();

    /*
        创建一个新线程，并将当前连接转移给新线程执行，当前连接
        会失效，需要重新执行AcceptClient()函数接受新的连接
        输入
            func -- 新线程需要执行的函数
            as   -- 函数的参数列表
    */
    template <typename ... Args>
    void ThreadDetach(std::function<void(Args...)> const &func, Args &&... as);
};

template <typename ... Args>
void Socket::ThreadDetach(std::function<void(Args...)> const &func, Args &&... as)
{
    // 将连接保存下来，待会转移至新线程
    int conn = GetConnection(std::this_thread::get_id());
    SSL *ssl = GetSSLConnection(std::this_thread::get_id());
    // 将当前线程重置
    ResetConnection();
    // 在创建新线程前抢占锁，防止新线程访问threads容器
    thread_lock.lock();
    // 创建新线程
    std::thread new_thread(func, std::ref(as) ...);
    std::thread::id new_tid = new_thread.get_id();
    threads.push_back(Thread{new_tid, conn, false, ssl});
    // 解锁
    thread_lock.unlock();
    // 分离进程
    new_thread.detach();
}

#endif