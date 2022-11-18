#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sodium.h>
#include "client.h"

#define HEADER_LENGTH 3
#define MAX_CONNECTION 2000

const unsigned BLOCK_SIZE = 8192;

struct Connection RomeSocketConnect(const char *server, const unsigned port) {
    if (sodium_init() < 0) {
        printf("Fail to initialize libsodium\n");
        exit(0);
    }
    
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    size_t addr_size = sizeof(struct sockaddr_in);
    memset(&addr, 0, addr_size);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, server, &addr.sin_addr);
    connect(sock, (struct sockaddr *)&addr, addr_size);
    // 准备交换密钥
    unsigned char client_pk[crypto_kx_PUBLICKEYBYTES], client_sk[crypto_kx_SECRETKEYBYTES];
    unsigned char *client_rx = malloc(crypto_kx_SESSIONKEYBYTES);
    unsigned char *client_tx = malloc(crypto_kx_SESSIONKEYBYTES);
    crypto_kx_keypair(client_pk, client_sk);

    // 接收来自服务器的公钥
    unsigned char server_pk[crypto_kx_PUBLICKEYBYTES];
    // 发送明文hello消息，并附上自己的公钥
    char hello[BLOCK_SIZE];
    memset(hello, 0, BLOCK_SIZE);
    strcpy(hello, "RSHELLO");
    memcpy(hello + 8, client_pk, crypto_kx_PUBLICKEYBYTES);
    RomeSocketSplitAndSend(sock, hello, BLOCK_SIZE - HEADER_LENGTH - 1);
    // 接收握手消息，收到公钥
    char *shake = NULL;
    RomeSocketReceive(sock, &shake);
    memcpy(server_pk, shake + 8, crypto_kx_PUBLICKEYBYTES);
    printf("Client public key: ");
    PrintHex(client_pk, crypto_kx_PUBLICKEYBYTES);
    printf("Get public key from server: ");
    PrintHex(server_pk, crypto_kx_PUBLICKEYBYTES);
    // 清理接收缓冲区
    RomeSocketClearBuffer(&shake);
    // 计算密钥
    if (crypto_kx_client_session_keys(client_rx, client_tx,
                                    client_pk, client_sk, server_pk) != 0) {
        printf("Client: Suspicious server key, abort.\n");
        struct Connection conn = {0, NULL, NULL};
        return conn;
    }
    printf("Finish key exchange.\n");
    printf("client tx: ");
    PrintHex(client_tx, crypto_kx_SESSIONKEYBYTES);
    struct Connection conn = {sock, client_rx, client_tx};
    return conn;
}

void RomeSocketSend(struct Connection sock, const char *buffer, const unsigned size) {
    // 加密buffer
    unsigned ciphertext_length = size + crypto_secretbox_MACBYTES + crypto_secretbox_NONCEBYTES;
    unsigned char *ciphertext = malloc(ciphertext_length);
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    randombytes_buf(nonce, sizeof nonce);
    crypto_secretbox_easy(ciphertext + crypto_secretbox_NONCEBYTES, (unsigned char *)buffer, size, nonce, sock.tx);
    // 将nonce附加在消息开头
    memcpy(ciphertext, nonce, crypto_secretbox_NONCEBYTES);
    printf("Send total encrypted message, nonce: ");
    PrintHex(nonce, crypto_secretbox_NONCEBYTES);
    // 分块并发送
    printf("ciphertext length: %u\n", ciphertext_length);
    PrintHex(ciphertext, ciphertext_length);
    RomeSocketSplitAndSend(sock.sock, (char *)ciphertext, ciphertext_length);
    free(ciphertext);
}

void RomeSocketSendAll(int sock, char *send_buff, size_t length) {
    size_t sent = 0, remain = length;
    while (remain > 0) {
        size_t size = send(sock, send_buff + sent, remain, 0);
        sent += size;
        remain -= size;
    }
}

void RomeSocketSplitAndSend(int sock, const char *buffer, const unsigned size) {
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
        RomeSocketSendAll(sock, send_buff, BLOCK_SIZE);
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

void RomeSocketClose(struct Connection *conn) {
    close(conn->sock);
    free(conn->rx);
    free(conn->tx);
}