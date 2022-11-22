#include "layer.h"

// debug
void PrintHex(unsigned char *data, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        printf("%x", data[i]);
    }
    printf("\n");
}

struct Buffer RomeSocketConcatenate(struct Buffer *buffers, unsigned count) {
    unsigned total_length = 0; // 有效数据的总长度
    unsigned *block_length = (unsigned *)malloc(sizeof(unsigned) * count); // 暂存块长度
    for (int i = 0; i < count; ++i) {
        if (buffers[i].length < 3) {
            struct Buffer null = {NULL, 0};
            return null;
        }
        block_length[i] = ((unsigned)(buffers[i].buffer[1] << 8)) |
                          ((unsigned)(buffers[i].buffer[2]) & 0x00FF);
        total_length += block_length[i];
    }
    // 申请内存
    char *complete = (char *)malloc(total_length);
    memset(complete, 0, total_length);
    // 复制并组装数据块
    unsigned offset = 0;
    for (int i = 0; i < count; ++i) {
        memcpy(complete + offset, buffers[i].buffer + HEADER_LENGTH, block_length[i]);
        offset += block_length[i];
    }
    // 返回结果
    free(block_length);
    struct Buffer result = {complete, total_length};
    return result;
}

struct Buffer RomeSocketDecrypt(struct Buffer buffer, unsigned char *rx) {
    // 初始化sodium
    if (sodium_init() < 0) {
        struct Buffer null = {NULL, 0};
        return null;
    }
    // 检查长度
    if (buffer.length < crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES) {
        struct Buffer null = {NULL, 0};
        return null;
    }
    // 提取nonce
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    memcpy(nonce, buffer.buffer, crypto_secretbox_NONCEBYTES);
    printf("nonce: ");
    PrintHex(nonce, crypto_secretbox_NONCEBYTES);
    // 解密进入明文
    char *plaintext = (char *)malloc(buffer.length - crypto_secretbox_NONCEBYTES);
    if (crypto_secretbox_open_easy(
            (unsigned char *)plaintext,
            (unsigned char *)buffer.buffer + crypto_secretbox_NONCEBYTES,
            buffer.length - crypto_secretbox_NONCEBYTES,
            nonce,
            rx) == 0) {
        struct Buffer result = {plaintext, buffer.length - crypto_secretbox_NONCEBYTES};
        return result;
    } else {
        // MAC认证失败
        struct Buffer null = {NULL, 0};
        return null;
    }
}