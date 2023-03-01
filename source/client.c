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

struct Connection RomeSocketConnect(const char *server, const unsigned port, time_t timeout) {
    if (sodium_init() < 0) {
        printf("Fail to initialize libsodium.\n");
        exit(0);
    }
    
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    // 设置超时时间
    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) == -1) {
        printf("Fail to set SO_RCVTIMEO.\n");
    }
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv) == -1) {
        printf("Fail to set SO_SNDTIMEO.\n");
    }
    // 绑定地址并连接
    struct sockaddr_in addr;
    size_t addr_size = sizeof(struct sockaddr_in);
    memset(&addr, 0, addr_size);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, server, &addr.sin_addr);
    if (connect(sock, (struct sockaddr *)&addr, addr_size) == -1) {
        printf("Fail to connect to server.\n");
        return (struct Connection){-1, NULL, NULL};
    }
    // 准备交换密钥
    unsigned char client_pk[crypto_kx_PUBLICKEYBYTES], client_sk[crypto_kx_SECRETKEYBYTES];
    // unsigned char *client_rx = malloc(crypto_kx_SESSIONKEYBYTES);
    // unsigned char *client_tx = malloc(crypto_kx_SESSIONKEYBYTES);
    crypto_kx_keypair(client_pk, client_sk);

    // 接收来自服务器的公钥
    unsigned char server_pk[crypto_kx_PUBLICKEYBYTES];
    // 发送明文hello消息，并附上自己的公钥
    char temp[BLOCK_LENGTH];
    struct Buffer hello = {temp, BLOCK_LENGTH};
    if (RomeSocketGetHello(&hello, client_pk) != 1) {
        printf("Fail to generate hello package.\n");
        exit(1);
    }
    RomeSocketSendAll(sock, hello.buffer, hello.length);
    // 接收握手消息，收到公钥
    char shake[BLOCK_LENGTH];
    int got = RomeSocketReceiveAll(sock, shake);
    if (got != BLOCK_LENGTH) {
        return (struct Connection){-1, NULL, NULL};
    }
    if (RomeSocketCheckHello((struct Buffer){shake, BLOCK_LENGTH}, server_pk) != 1) {
        printf("Bad shake package.\n");
        return (struct Connection){-1, NULL, NULL};
    }
    // 计算密钥
    struct Connection conn;
    conn.sock = sock;
    if (crypto_kx_client_session_keys(conn.rx, conn.tx,
                                    client_pk, client_sk, server_pk) != 0) {
        printf("Client: Suspicious server key, abort.\n");
        return (struct Connection){-1, NULL, NULL};
    }
    return conn;
}

void RomeSocketSend(struct Connection sock, struct Buffer send_buffer) {
    struct Buffer ciphertext = RomeSocketEncrypt(send_buffer, sock.tx);
    unsigned length = 0;
    struct Buffer *buffers   = RomeSocketSplit(ciphertext, &length);
    // 逐块发送
    for (unsigned i = 0; i < length; ++i) {
        RomeSocketSendAll(sock.sock, buffers[i].buffer, buffers[i].length);
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
        long long size = send(sock, send_buff + sent, remain, 0);
        sent += size;
        remain -= size;
    }
}

int RomeSocketReceiveAll(int sock, char *buffer) {
    size_t got = 0, remain = BLOCK_LENGTH;
    while (remain > 0) {
        long long size = recv(sock, buffer + got, remain, 0);
        if (size <= 0) {
            break;
        }
        got += size;
        remain -= size;
    }
    return got;
}

// 返回的Buffer结构需要手动清理
struct Buffer RomeSocketReceive(struct Connection conn, unsigned max_block) {
    // 接收全部报文
    struct Buffer *temp_list = malloc(sizeof (struct Buffer) * max_block);
    unsigned count = 0;
    int timeout = 0;
    while (count < max_block - 1) {
        temp_list[count].buffer = malloc(sizeof(char) * BLOCK_LENGTH);
        int got = RomeSocketReceiveAll(conn.sock, temp_list[count].buffer);
        if (got != BLOCK_LENGTH) {
            timeout = 1;
            break;
        }
        temp_list[count].length = BLOCK_LENGTH;
        if ((unsigned char)temp_list[count++].buffer[0] == 0xFF) {
            break;
        }
    }
    struct Buffer ciphertext = {NULL, 0}, plaintext = {NULL, 0};
    if (!timeout) {
        // 拼接
        ciphertext = RomeSocketConcatenate(temp_list, count);
        // 解密
        plaintext = RomeSocketDecrypt(ciphertext, conn.rx);
    }
    // 清理缓存
    if (ciphertext.buffer) {
        free(ciphertext.buffer);
    }
    for (unsigned i = 0; i < count; ++i) {
        if (temp_list[i].buffer) {
            free(temp_list[i].buffer);
        }
    }
    if (temp_list) {
        free(temp_list);
    }
    return plaintext;
}

void RomeSocketClose(struct Connection *conn) {
    close(conn->sock);
}