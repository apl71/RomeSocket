/*
    跨平台Socket通信库
    by apl71
    2021.09.20
*/

#ifndef ROMESOCKET_H_
#define ROMESOCKET_H_

#include <string>
#include <vector>
#include <cstddef>
#include <iterator>
#include "Error.h"

using BYTES = std::vector<std::byte>;

enum class SOCKET_TYPE : int { TCP, UDP };
enum class IP_VERSION : int { IPv4, IPv6 };

constexpr uint64_t HEAD_LENGTH = 4;
constexpr uint64_t MAX_LENGTH = (1 << 8 * HEAD_LENGTH) - 1;

static_assert(HEAD_LENGTH < 8);

#if defined(__linux__)
typedef int SocketHandle;
#elif defined(_WIN32)
typedef unsigned SocketHandle;
#endif

class RomeSocket
{
protected:
    // 保存socket套接字句柄
    SocketHandle sock;
    // 由于各个平台的不可用套接字的值不同，需要该变量来表示套接字的可用性
    bool valid = false;

// =========================================================================================================================
//  SendArray()/ReceiveArray() 发送/接收array<byte, LEN>，表示字节流 |
//  SendVector()/ReceiveVector() 发送/接收vector<byte>，表示字节流   |
//  SendString()/ReceiveString() 发送/接收string，表示字符串         | 
// =========================================================================================================================
//  SendByIterator()/ReceiveByIterator() 发送/接收迭代器指定的区间    | SendCString()/ReceiveCString() 接收/发送C风格字符串或字节流
// =========================================================================================================================
//                                         SendFix()/ReceiveFix() 发送/接收固定长度的字符串
// =========================================================================================================================

    /*
        第一层封装的基础函数，用户不应调用此函数
        利用循环发送固定长度的数据
        使用前需要检查connection_id的可用性
        该函数不对连接可用性做检测
    */
    static ERROR_CODE SendFix(
        const int &connection_id,
        const char *data,
        const size_t &length);

    static ERROR_CODE ReceiveFix(
        const int &connection_id,
        char *data,
        const size_t &length);

    /*
        第二层封装的基础函数，用户不应调用此函数
        使用两个迭代器表示字节流，前闭后开
        该函数不对连接的可用性以及迭代器的合法性作检查
    */
    template <std::random_access_iterator RANDOM_ITER>
    static ERROR_CODE SendByIterator(
        const int &connection_id,
        const RANDOM_ITER &cbegin,
        const RANDOM_ITER &cend);
    
    template <std::random_access_iterator RANDOM_ITER>
    static ERROR_CODE ReceiveByIterator(
        const int &connection_id,
        RANDOM_ITER begin,
        RANDOM_ITER end);

public:
    RomeSocket();
    RomeSocket(const RomeSocket &sock) = delete;
    RomeSocket &operator=(const RomeSocket &sock) = delete;

    virtual ERROR_CODE CreateSocket(const IP_VERSION &ip_version,
        const SOCKET_TYPE &protocol,
        const unsigned &port,
        const std::string &ip = "") = 0;

    bool IsValid() const;

    virtual ERROR_CODE SendVector(BYTES &data) const = 0;
    virtual ERROR_CODE ReceiveVector(BYTES &data) const = 0;
    virtual ERROR_CODE SendString(const std::string &data) const = 0;
    virtual ERROR_CODE ReceiveString(std::string &data) const = 0;

    virtual ~RomeSocket() = 0;
};

template <std::random_access_iterator RANDOM_ITER>
static ERROR_CODE SendByIterator(
        const int &connection_id,
        const RANDOM_ITER &cbegin,
        const RANDOM_ITER &cend)
{
    BYTES bytes(cbegin, cend);
    auto body_length = cend - cbegin;
    if (body_length > MAX_LENGTH)
    {
        return ERROR_CODE::OVERFLOW;
    }
    std::array<std::byte, HEAD_LENGTH> head;
    for (uint64_t i = 0; i < HEAD_LENGTH; ++i)
    {
        head[i] = std::byte{body_length >> (i * 8)};
    }
    ERROR_CODE error = ERROR_CODE::SUCCESS;
    if (error = SendFix(connection_id, head.data(), head.size());
        error != ERROR_CODE::SUCCESS)
    {
        return error;
    }
    if (error = SendFix(connection_id, bytes.data(), bytes.size());
        error != ERROR_CODE::SUCCESS)
    {
        return error;
    }
    return error;
}

template <std::random_access_iterator RANDOM_ITER>
static ERROR_CODE ReceiveByIterator(
    const int &connection_id,
    RANDOM_ITER begin,
    RANDOM_ITER end)
{
    char head_buff[HEAD_LENGTH];
    ERROR_CODE error = ERROR_CODE::SUCCESS;
    if (error = ReceiveFix(connection_id, head_buff, HEAD_LENGTH);
        error != ERROR_CODE::SUCCESS)
    {
        return error;
    }
    size_t body_length = 0;
    for (size_t i = 0; i < HEAD_LENGTH; ++i)
    {
        size_t offset = HEAD_LENGTH - i;
        body_length += (static_cast<size_t>(head_buff[i]) << (8 * offset));
    }
    char *buff = new char[body_length];
    if (error = ReceiveFix(connection_id, buff, body_length);
        error != ERROR_CODE::SUCCESS)
    {
        return error;
    }
    for (auto iter = begin, i = 0; iter != end; ++iter, ++i)
    {
        *iter = buff[i];
    }
    return error;
}

#endif