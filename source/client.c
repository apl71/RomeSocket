#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

const char *SERVER = "localhost";
const unsigned BLOCK_SIZE = 8192;

int RomeSocketConnect(const char *server, const unsigned port) {
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    size_t addr_size = sizeof(struct sockaddr_in);
    memset(&addr, 0, addr_size);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, SERVER, &addr.sin_addr);
    connect(sock, (struct sockaddr *)&addr, addr_size);
    return sock;
}

void RomeSocketSend(int sock, const char *buffer, const unsigned size) {
    unsigned blocks = size / (BLOCK_SIZE - 1) + 1;
    char *send_buff = malloc(sizeof(char) * BLOCK_SIZE);
    memset(send_buff, 0, BLOCK_SIZE);
    for (int i = 0; i < blocks; ++i) {
        send_buff[0] = (i == blocks - 1) ? 0xFF : (char)i;
        unsigned start = i * BLOCK_SIZE, length = BLOCK_SIZE - 1;
        if (i == blocks - 1) {
            length = size - start;
        }
        memcpy(send_buff + 1, buffer + start, length);
        send(sock, send_buff, length, 0);
    }
}