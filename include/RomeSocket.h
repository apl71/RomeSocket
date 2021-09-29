/*
    跨平台Socket接口
    by apl74
    2021.09.20
*/

#ifndef ROMESOCKET_H_
#define ROMESOCKET_H_

#include <string>

enum SOCKET_TYPE { TCP, UDP };
enum IP_VERSION { IPv4, IPv6 };
enum ROLE { SERVER, CLIENT, UNKNOWN };

#if defined(__linux__)
typedef int SocketHandle;
#elif defined(_WIN32)
typedef unsigned SocketHandle;
#endif

class Socket
{
private:
    // 保存socket套接字句柄
    SocketHandle sock;
    // 由于各个平台的不可用套接字的值不同，需要该变量来表示套接字的可用性
    bool valid = false;
    // 服务器端使用的连接
    int conn = -1;
    // 指定客户端或服务端
    ROLE role = UNKNOWN;
    // 保存结构体
    void *addr = nullptr;
    // 结构体大小
    unsigned addr_size = 0;
    // 最大连接数量，不设置默认为200
    unsigned max_conn = 200;

public:
    Socket();

    /*
        创建套接字
        输入
            ip_version 使用的IP协议族，可选的值有IPv4和IPv6
            protocol 传输层协议，可选的值有TCP和UDP
        输出
            整数表示执行结果
            1  = 成功
            0  = 失败，该对象已经创建了一个可用的套接字
            -1 = 失败，IP类型不正确
            -2 = 失败，传输层协议不正确
            -3 = 失败，inet_pton错误
            -4 = 失败，未能绑定套接字
            -5 = 失败，未能监听端口
            -6 = 失败，指定CS类型错误
            -7 = 失败，未能创建套接字
    */
    int CreateSocket(IP_VERSION ip_version, SOCKET_TYPE protocol, ROLE r, unsigned port, std::string ip = "");

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
        开始接收客户端消息
        输出
            整数表示执行结果
            1  = 成功
            0  = 失败，非服务器端不能使用此函数
            -1 = 失败，已经开始接收客户端消息
            -2 = 失败，未能成功接收客户端请求
            -3 = 失败，套接字不可用
    */
    int AcceptClient();

    /*
        连接到服务端
        输出
            整数表示执行结果
            1  = 成功
            0  = 失败，未能连接到服务器
            -1 = 失败，套接字不可用
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
    int SendData(unsigned char *send_buff, unsigned length) const;

    /*
        接收消息，此函数是阻塞的
        输入
            recv_buff 接收数据的缓冲区
            length 可用的缓冲区大小
        输出
            实际接收到的数据大小，若为负数，参阅以下错误代码
            -1 = 失败，套接字不可用
            -2 = 接收数据失败
            -3 = 未知错误
    */
    int ReceiveData(unsigned char *recv_buff, unsigned length) const;

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
    int ReceiveDataFix(unsigned char *recv_buff, unsigned length) const;

    /*
        重置连接
    */
    void ResetConnection();

    /*
        关闭连接
        输出
            整数表示执行结果
            1 = 成功
            0 = 失败，仅有服务器可以断开连接
    */
    int CloseConnection();

    /*
        关闭套接字
    */
    void CloseSocket();
};

#endif