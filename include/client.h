#ifndef ROMESOCKET_CLIENT_H_
#define ROMESOCKET_CLIENT_H_

#include "layer.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Connection {
    int sock;
    unsigned char *rx;
    unsigned char *tx;
};

struct Connection RomeSocketConnect(const char *server, const unsigned port);

void RomeSocketSendAll(int sock, char *send_buff, size_t length);

void RomeSocketSend(struct Connection sock, const char *buffer, const unsigned size);

void RomeSocketSplitAndSend(int sock, const char *buffer, const unsigned size);

void RomeSocketReceiveAll(int sock, char *buffer);

struct Buffer RomeSocketReceive(struct Connection conn, unsigned max_block);

void RomeSocketClose(struct Connection *conn);

#ifdef __cplusplus
}
#endif

#endif