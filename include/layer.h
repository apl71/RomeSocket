#ifndef ROMESOCKET_LAYER_H_
#define ROMESOCKET_LAYER_H_

#include <stdlib.h>
#include <string.h>
#include <sodium.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Buffer {
    char *buffer;
    unsigned length;
};

void PrintHex(unsigned char *data, size_t length);

void Endian();

// 定义头部长度
#define HEADER_LENGTH 3
#define BLOCK_LENGTH 8192

// 拼接层工作
// 每块buffer是 Header(3B) + Data
// 返回完整的buffer指针，需要调用者清理
struct Buffer RomeSocketConcatenate(struct Buffer *buffers, unsigned count);

// 解密层工作
// 接收一块完整的buffer，进行解密工作
// nonce + cipher + mac
// 返回明文数据，需要调用者清理
struct Buffer RomeSocketDecrypt(struct Buffer buffer, unsigned char *rx);

// 加密层工作
// 接收一块明文buffer，进行加密
// 返回密文数据，需要调用者清理
struct Buffer RomeSocketEncrypt(struct Buffer buffer, unsigned char *tx);

// 分割层工作
// 将完整的full_block按照块大小分割为多个块
// 返回一系列buffer，需要调用者清理
struct Buffer *RomeSocketSplit(struct Buffer full_block, unsigned *length);

void RomeSocketClearBuffer(struct Buffer buffer);

#ifdef __cplusplus
}
#endif

#endif