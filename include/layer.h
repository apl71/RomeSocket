#ifndef ROMESOCKET_LAYER_H_
#define ROMESOCKET_LAYER_H_

#include <stdlib.h>
#include <string.h>
#include <sodium.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ReadBuffer {
    char *buffer;
    unsigned length;
};

// 接收消息
struct ReadBuffer RomeSocketRead() {

}

// 定义头部长度
#define HEADER_LENGTH 3

// 拼接层工作
// 每块buffer是 Header(3B) + Data
// 返回完整的buffer指针，需要调用者清理
struct ReadBuffer RomeSocketConcatenate(struct ReadBuffer *buffers, unsigned count) {
    unsigned total_length = 0; // 有效数据的总长度
    unsigned *block_length = (unsigned *)malloc(sizeof(unsigned) * count); // 暂存块长度
    for (int i = 0; i < count; ++i) {
        if (buffers[i].length < 3) {
            ReadBuffer null{NULL, 0};
            return null;
        }
        block_length[i] = (unsigned)(buffers[i].buffer[2] << 8) |
                          (unsigned)(buffers[i].buffer[3] << 0);
        total_length += block_length[i];
    }
    // 申请内存
    char *complete = (char *)malloc(total_length);
    memset(complete, 0, total_length);
    // 复制并组装数据块
    for (int i = 0; i < count; ++i) {
        unsigned offset = 0;
        memcpy(complete + offset, buffers[i].buffer + HEADER_LENGTH, block_length[i]);
        offset += block_length[i];
    }
    // 返回结果
    struct ReadBuffer result{complete, total_length};
    return result;
}

// 解密层工作
// 接收一块完整的buffer，进行解密工作
// nonce + cipher + mac
// 返回明文数据，需要调用者清理
struct ReadBuffer RomeSocketDecrypt(struct ReadBuffer buffer, unsigned char *rx) {
    // 初始化sodium
    if (sodium_init() < 0) {
        ReadBuffer null{NULL, 0};
        return null;
    }
    // 检查长度
    if (buffer.length < crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES) {
        ReadBuffer null{NULL, 0};
        return null;
    }
    // 提取nonce
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    memcpy(nonce, buffer.buffer, crypto_secretbox_NONCEBYTES);
    // 解密进入明文
    char *plaintext = (char *)malloc(buffer.length - crypto_secretbox_NONCEBYTES);
    if (crypto_secretbox_open_easy(
            (unsigned char *)plaintext,
            (unsigned char *)buffer.buffer + crypto_secretbox_NONCEBYTES,
            buffer.length - crypto_secretbox_NONCEBYTES,
            nonce,
            rx) == 0) {
        ReadBuffer result{plaintext, buffer.length - crypto_secretbox_NONCEBYTES};
        return result;
    } else {
        // MAC认证失败
        ReadBuffer null{NULL, 0};
        return null;
    }
}

#ifdef __cplusplus
}
#endif

#endif