#include "RomeSocket.h"
#if defined(__linux__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#elif defined (_WIN32)
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

Socket::Socket()
{
    valid = false;
}

int Socket::CreateSocket(IP_VERSION ip_version, SOCKET_TYPE protocol, ROLE r, unsigned port, std::string ip)
{
    // 已经创建了可用套接字，不能重复创建
    if (valid)
    {
        return 0;
    }

    // 选择IPv4或IPv6
    int version = 0;
    if (ip_version == IPv4)
    {
        version = AF_INET;
    }
    else if (ip_version == IPv6)
    {
        version = AF_INET6;
    }
    else
    {
        return -1;
    }

    role = r;

    // 选择TCP或UDP协议
    int pro = 0;
    if (protocol == TCP)
    {
        pro = SOCK_STREAM;
    }
    else if (protocol == UDP)
    {
        pro = SOCK_DGRAM;
    }
    else
    {
        return -2;
    }

    #if defined(_WIN32)
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(1, 1), &wsa_data);
    #endif

    sock = socket(version, pro, 0);
    #if defined(__linux__)
    if (sock < 0)
    {
        return -7;
    }
    #elif defined(_WIN32)
    if (sock == INVALID_SOCKET)
    {
        return -7;
    }
    #endif
    addr = new struct sockaddr_in;
    addr_size = sizeof(sockaddr_in);
    memset(addr, 0, addr_size);
    ((sockaddr_in *)addr)->sin_family = version;
    ((sockaddr_in *)addr)->sin_port = htons(port);

    if (r == CLIENT)
    {
        if (inet_pton(version, ip.c_str(), &((sockaddr_in *)addr)->sin_addr) <= 0)
        {
            return -3;
        }
        else
        {
            valid = true;
            return 1;
        }
    }
    else if (r == SERVER)
    {
        ((sockaddr_in *)addr)->sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(sock, (sockaddr *)addr, addr_size) == -1)
        {
            return -4;
        }

        if (listen(sock, max_conn) == -1)
        {
            return -5;
        }
        valid = true;
        return 1;
    }
    else
    {
        return -6;
    }
}

bool Socket::IsValid()
{
    return valid;
}

int Socket::AcceptClient()
{
    if (!valid)
    {
        return -3;
    }

    if (role != SERVER)
    {
        return 0;
    }
    if (conn != -1)
    {
        return -1;
    }

    conn = accept(sock, nullptr, nullptr);
    if (conn == -1)
    {
        return -2;
    }
    else
    {
        return 1;
    }
}

int Socket::ConnectServer()
{
    if (!valid)
    {
        return -1;
    }

    if (connect(sock, (sockaddr *)addr, addr_size) < 0)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

int Socket::SendData(unsigned char *send_buff, unsigned length)
{
    if (!valid)
    {
        return -1;
    }
    int len = 0;
    if (role == CLIENT)
    {
        if ((len = send(sock, (char *)send_buff, length, 0)) < 0)
        {
            return -2;
        }
        else
        {
            return len;
        }
    }
    else if (role == SERVER)
    {
        if ((len = send(conn, (char *)send_buff, length, 0)) < 0)
        {
            return -2;
        }
        else
        {
            return len;
        }
    }
    else
    {
        return -3;
    }
}

int Socket::RecieveData(unsigned char *recv_buff, unsigned length)
{
    if (!valid)
    {
        return -1;
    }
    int len = 0;
    if (role == CLIENT)
    {
        if ((len = recv(sock, (char *)recv_buff, length, 0)) < 0)
        {
            return -2;
        }
        else
        {
            return len;
        }
    }
    else if (role == SERVER)
    {
        if ((len = recv(conn, (char *)recv_buff, length, 0)) < 0)
        {
            return -2;
        }
        else
        {
            return len;
        }
    }
    else
    {
        return -3;
    }
}

void Socket::CloseSocket()
{
    if (!valid)
    {
        return;
    }

    valid = false;
    conn = -1;
    role = UNKNOWN;
    delete[](sockaddr *)addr;
    addr = nullptr;
    addr_size = 0;

    #if defined(__linux__)
    close(sock);
    #elif defined(_WIN32)
    closesocket(sock);
    #endif
}

int Socket::CloseConnection()
{
    if (role != SERVER)
    {
        return 0;
    }
    #if defined(__linux__)
    close(conn);
    #elif defined(_WIN32)
    closesocket(conn);
    #endif
    return 1;
}