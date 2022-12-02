#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sodium.h>
#include "client.h"

#define MAX_CONNECTION 2000

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
    char temp[BLOCK_LENGTH];
    struct Buffer hello = {temp, BLOCK_LENGTH};
    if (RomeSocketGetHello(&hello, client_pk) != 1) {
        printf("Fail to generate hello package.");
        exit(1);
    }
    RomeSocketSendAll(sock, hello.buffer, hello.length);
    // 接收握手消息，收到公钥
    char shake[BLOCK_LENGTH];
    RomeSocketReceiveAll(sock, shake);
    if (RomeSocketCheckHello((struct Buffer){shake, BLOCK_LENGTH}, server_pk) != 1) {
        printf("Bad shake package.");
        exit(1);
    }
    // 计算密钥
    if (crypto_kx_client_session_keys(client_rx, client_tx,
                                    client_pk, client_sk, server_pk) != 0) {
        printf("Client: Suspicious server key, abort.\n");
        struct Connection conn = {0, NULL, NULL};
        return conn;
    }
    struct Connection conn = {sock, client_rx, client_tx};
    return conn;
}

void RomeSocketSend(struct Connection sock, struct Buffer send_buffer) {
    struct Buffer ciphertext = RomeSocketEncrypt(send_buffer, sock.tx);
    unsigned length = 0;
    struct Buffer *buffers   = RomeSocketSplit(ciphertext, &length);
    // 逐块发送
    for (unsigned i = 0; i < length; ++i) {
        RomeSocketSendAll(sock.sock, buffers[i].buffer, buffers[i].length);
        // printf("send: \n");
        // unsigned length__ = ((unsigned)(buffers[i].buffer[1] << 8)) |
        //                   ((unsigned)(buffers[i].buffer[2]) & 0x00FF);
        // PrintHex(buffers[i].buffer, length__);
        PrintHex(buffers[i].buffer, buffers[i].length);
    }
    // 清理资源
    free(ciphertext.buffer);
    for (unsigned i = 0; i < length; ++i) {
        free(buffers[i].buffer);
    }
    free(buffers);
}

void RomeSocketSendAll(int sock, char *send_buff, size_t length) {
    size_t sent = 0, remain = length;
    while (remain > 0) {
        size_t size = send(sock, send_buff + sent, remain, 0);
        sent += size;
        remain -= size;
    }
}

void RomeSocketReceiveAll(int sock, char *buffer) {
    size_t got = 0, remain = BLOCK_LENGTH;
    while (remain > 0) {
        size_t size = recv(sock, buffer + got, remain, 0);
        got += size;
        remain -= size;
    }
}

// 返回的Buffer结构需要手动清理
struct Buffer RomeSocketReceive(struct Connection conn, unsigned max_block) {
    // 接收全部报文
    struct Buffer *temp_list = malloc(sizeof (struct Buffer) * max_block);
    unsigned count = 0;
    while (count < max_block - 1) {
        temp_list[count].buffer = malloc(sizeof(char) * BLOCK_LENGTH);
        RomeSocketReceiveAll(conn.sock, temp_list[count].buffer);
        temp_list[count].length = BLOCK_LENGTH;
        if ((unsigned char)temp_list[count++].buffer[0] == 0xFF) {
            break;
        }
    }
    // 拼接
    struct Buffer ciphertext = RomeSocketConcatenate(temp_list, count);
    // 解密
    struct Buffer plaintext = RomeSocketDecrypt(ciphertext, conn.rx);
    // 清理缓存
    free(ciphertext.buffer);
    for (unsigned i = 0; i < count; ++i) {
        free(temp_list[i].buffer);
    }
    free(temp_list);
    return plaintext;
}

void RomeSocketClose(struct Connection *conn) {
    close(conn->sock);
    free(conn->rx);
    free(conn->tx);
}