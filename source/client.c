#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

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
    unsigned blocks = size / (BLOCK_SIZE - 1) + 1;
    char *send_buff = malloc(sizeof(char) * BLOCK_SIZE);
    memset(send_buff, 0, BLOCK_SIZE);
    FILE *file = fopen("../../LICENSE_3", "w");
    for (int i = 0; i < blocks; ++i) {
        memset(send_buff, 0, BLOCK_SIZE);
        send_buff[0] = (i == blocks - 1) ? 0xFF : (char)i;
        unsigned start = i * (BLOCK_SIZE - 1), length = BLOCK_SIZE - 1;
        if (i == blocks - 1) {
            length = size - start;
        }
        memcpy(send_buff + 1, buffer + start, length);
        fwrite(send_buff + 1, sizeof(char), length, file);
        for (int j = 0; j < length; ++j) {
            printf("%c", *(send_buff + 1 + j));
        }
        send(sock, send_buff, BLOCK_SIZE, 0);
    }
    free(send_buff);
    fclose(file);
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
// 需要手动回收
unsigned RomeSocketReceive(int sock, char **buffer) {
    char *temp_list[256] = {NULL};
    int count = 0;
    while (1) {
        char *temp = malloc(sizeof(char) * BLOCK_SIZE);
        memset(temp, 0, BLOCK_SIZE);
        printf("waiting for block %d\n", count);
        ReceiveFullBlock(sock, temp);
        printf("receive block %d\n", count);
        temp_list[count++] = temp;
        if ((unsigned char)temp[0] == 0xFF) {
            break;
        }
    }
    // 组装
    size_t total_length = (BLOCK_SIZE - 1) * count;
    *buffer = malloc(sizeof(char) * total_length);
    memset(*buffer, 0, total_length);
    size_t offset = 0;
    for (int i = 0; i < count; ++i) {
        memcpy(*buffer + offset, temp_list[i] + 1, BLOCK_SIZE - 1);
        offset += (BLOCK_SIZE - 1);
        // 清除缓存
        free(temp_list[i]);
    }
    return offset;
}

void RomeSocketClearBuffer(char **buffer) {
    free(*buffer);
    *buffer = NULL;
}