#include "RomeSocket.h"
#include <tuple>
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

static ERROR_CODE SendFix(
    const int &connection_id,
    const void *data,
    const size_t &length)
{
    size_t sent = 0, remains = length;
    while (remains != 0 && sent != 0)
    {
        sent = send(connection_id, data, remains, 0);
        remains -= sent;
    }
    if (remains == 0)
    {
        return ERROR_CODE::SUCCESS;
    }
    else if (sent == 0)
    {
        return ERROR_CODE::CONNECTION_CLOSED_BY_PEER;
    }
    else
    {
        return ERROR_CODE::UNKNOWN_ERROR;
    }
}

static ERROR_CODE ReceiveFix(
    const int &connection_id,
    void *data,
    const size_t &length)
{
    size_t received = 0, remains = length;
    while (remains != 0 && received != 0)
    {
        received = recv(connection_id, data, remains, 0);
        remains -= received;
    }
    if (remains == 0)
    {
        return ERROR_CODE::SUCCESS;
    }
    else if (received == 0)
    {
        return ERROR_CODE::CONNECTION_CLOSED_BY_PEER;
    }
    else
    {
        return ERROR_CODE::UNKNOWN_ERROR;
    }
}