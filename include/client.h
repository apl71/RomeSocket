#ifndef ROMESOCKET_CLIENT_H_
#define ROMESOCKET_CLIENT_H_

#ifdef __cplusplus
extern "C" {
#endif

int RomeSocketConnect(const char *server, const unsigned port);

void RomeSocketSend(int sock, const char *buffer, const unsigned size);

unsigned RomeSocketReceive(int sock, char **buffer);

#ifdef __cplusplus
}
#endif

#endif