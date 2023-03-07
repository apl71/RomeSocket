#ifndef ROMESOCKET_CLIENT_H_
#define ROMESOCKET_CLIENT_H_

#ifdef __WIN32__
#include <winsock2.h>
#elif __linux__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include "layer.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __linux__
#define SOCKET int
#define SOCKET_ERROR -1
#endif

struct Connection {
    SOCKET sock;
    unsigned char rx[crypto_kx_SESSIONKEYBYTES];
    unsigned char tx[crypto_kx_SESSIONKEYBYTES];
};

struct Connection RomeSocketConnect(const char *server, const unsigned port, time_t timeout);

void RomeSocketSendAll(int sock, char *send_buff, size_t length);

void RomeSocketSend(struct Connection sock, struct Buffer send_buffer);

int RomeSocketReceiveAll(int sock, char *buffer);

struct Buffer RomeSocketReceive(struct Connection conn, unsigned max_block);

void RomeSocketClose(struct Connection *conn);

#ifdef __cplusplus
}
#endif

#endif