#include "RomeSocketServer.h"
#if defined(__linux__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#elif defined(_WIN32)
#define _WIN32_WINNT 0x0601
#include <WinSock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

ERROR_CODE RomeSocketServer::CreateSocket(
    const IP_VERSION &ip_version,
    const SOCKET_TYPE &protocol,
    const unsigned &port,
    const std::string &ip = "")
{
    if (valid)
    {
        return ERROR_CODE::SOCKET_ALREADY_EXISTS;
    }
    int version = 0;
    if (ip_version == IP_VERSION::IPv4)
    {
        version = AF_INET;
    }
    else if (ip_version == IP_VERSION::IPv6)
    {
        version = AF_INET6;
    }
    else
    {
        return ERROR_CODE::UNKNOWN_IP_VERSION;
    }

    int pro = 0;
    if (protocol == SOCKET_TYPE::TCP)
    {
        pro = SOCK_STREAM;
    }
    else if (protocol == SOCKET_TYPE::UDP)
    {
        pro = SOCK_DGRAM;
    }
    else
    {
        return ERROR_CODE::UNKNOWN_SOCKET_TYPE;
    }

    #if defined(_WIN32)
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(1, 1), &wsa_data);
    #endif

    sock = socket(version, pro, 0);
    #if defined(__linux__)
    if (sock < 0)
    {
        return ERROR_CODE::FAILURE_CREATING_SOCKET;
    }
    #elif defined(_WIN32)
    if (sock == INVALID_SOCKET)
    {
        return ERROR_CODE::FAILURE_CREATING_SOCKET;
    }
    #endif

    sockaddr_in addr;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(sockaddr_in)) == -1)
    {
        return ERROR_CODE::FAILURE_BINDING_PORT;
    }

    if (listen(sock, max_conntion) == -1)
    {
        return ERROR_CODE::FAILURE_LISTENING_PORT;
    }
    valid = true;
    return ERROR_CODE::SUCCESS;
}

ERROR_CODE RomeSocketServer::Accept()
{
    if (!valid)
    {
        return ERROR_CODE::INVALID_SOCKET;
    }
    connection = accept(sock, nullptr, nullptr);
    if (connection == -1)
    {
        return ERROR_CODE::FAILURE_ACCEPT_CLIENT;
    }
    else
    {
        return ERROR_CODE::SUCCESS;
    }
}

ERROR_CODE RomeSocketServer::SendVector(BYTES &data) const
{
    return SendByIterator(connection, data.cbegin(), data.cend());
}

ERROR_CODE RomeSocketServer::ReceiveVector(BYTES &data) const
{
    return ReceiveByIterator(connection, data.begin(), data.end());
}

ERROR_CODE RomeSocketServer::SendString(const std::string &data) const
{
    return ReceiveByIterator(connection, data.begin(), data.end());
}

ERROR_CODE RomeSocketServer::ReceiveString(std::string &data) const
{
    return ReceiveByIterator(connection, data.begin(), data.end());
}

RomeSocket::~RomeSocketServer()
{
    if (connection != -1)
    {
        close(connection);
    }
}