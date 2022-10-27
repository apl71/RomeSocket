#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define HEADER_LENGTH 3

const unsigned BLOCK_SIZE = 8192;

int RomeSocketConnect(const char *server, const unsigned port) {
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    size_t addr_size = sizeof(struct sockaddr_in);
    memset(&addr, 0, addr_size);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, server, &addr.sin_addr);
    connect(sock, (struct sockaddr *)&addr, addr_size);
    return sock;
}

void RomeSocketSend(int sock, const char *buffer, const unsigned size) {
    unsigned blocks = size / (BLOCK_SIZE - HEADER_LENGTH) + 1;
    char *send_buff = malloc(sizeof(char) * BLOCK_SIZE);
    memset(send_buff, 0, BLOCK_SIZE);
    for (int i = 0; i < blocks; ++i) {
        memset(send_buff, 0, BLOCK_SIZE);
        // 设置消息头
        unsigned start = i * (BLOCK_SIZE - HEADER_LENGTH), length = BLOCK_SIZE - HEADER_LENGTH;
        send_buff[0] = i;
        if (i == blocks - 1) {
            length = size - start;
            send_buff[0] = 0xFF;
        }
        send_buff[1] = length >> 8;
        send_buff[2] = length;
        memcpy(send_buff + HEADER_LENGTH, buffer + start, length);
        send(sock, send_buff, BLOCK_SIZE, 0);
        printf("send packet %d: length = %u\n", i, length);
    }
    free(send_buff);
}

void ReceiveFullBlock(int sock, char *buffer) {
    size_t got = 0, remain = BLOCK_SIZE;
    while (remain > 0) {
        size_t size = recv(sock, buffer + got, remain, 0);
        got += size;
        remain -= size;
    }
}

// 输入一个值为空的指针作为缓冲区
// 需要调用RomeSocketClearBuffer回收
unsigned RomeSocketReceive(int sock, char **buffer) {
    char *temp_list[256] = {NULL};
    int   length[256] = {0};
    unsigned count = 0;
    unsigned total_length = 0;
    while (1) {
        char *temp = malloc(sizeof(char) * BLOCK_SIZE);
        memset(temp, 0, BLOCK_SIZE);
        printf("waiting for block %d\n", count);
        ReceiveFullBlock(sock, temp);
        printf("receive block %d\n", count);
        temp_list[count] = temp;
        length[count] = (unsigned)(temp[1] << 8) | ((unsigned)temp[2] & 0x00FF);
        total_length += length[count++];
        if ((unsigned char)temp[0] == 0xFF) {
            break;
        }
    }
    // 组装
    *buffer = malloc(sizeof(char) * total_length);
    memset(*buffer, 0, total_length);
    size_t offset = 0;
    for (int i = 0; i < count; ++i) {
        memcpy(*buffer + offset, temp_list[i] + HEADER_LENGTH, length[i]);
        offset += (BLOCK_SIZE - HEADER_LENGTH);
        // 清除缓存
        free(temp_list[i]);
    }
    return total_length;
}

void RomeSocketClearBuffer(char **buffer) {
    free(*buffer);
    *buffer = NULL;
}