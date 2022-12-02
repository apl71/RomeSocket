#include "layer.h"

// debug
void PrintHex(unsigned char *data, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        printf("%x", data[i]);
    }
    printf("\n");
}

int RomeSocketCheckHello(struct Buffer buffer, unsigned char *pk) {
    if (buffer.length < 11 + crypto_kx_PUBLICKEYBYTES ||
        (unsigned char)buffer.buffer[7]  != 0x00 ||
        (unsigned char)buffer.buffer[8]  != 0xFF ||
        (unsigned char)buffer.buffer[9]  != 0x00 ||
        (unsigned char)buffer.buffer[10] != 0xFF ||
        memcmp("RSHELLO", buffer.buffer, 7) != 0) {
        return 0;
    }
    memcpy(pk, buffer.buffer + 11, crypto_kx_PUBLICKEYBYTES);
    return 1;
}

int RomeSocketGetHello(struct Buffer *buffer, unsigned char *pk) {
    if (buffer->length < 11 + crypto_kx_PUBLICKEYBYTES) {
        return 0;
    }
    memcpy(buffer->buffer, "RSHELLO", 7);
    buffer->buffer[7] = buffer->buffer[9]  = 0x00;
    buffer->buffer[8] = buffer->buffer[10] = 0xFF;
    memcpy(buffer->buffer + 11, pk, crypto_kx_PUBLICKEYBYTES);
    return 1;
}

struct Buffer RomeSocketConcatenate(struct Buffer *buffers, unsigned count) {
    unsigned total_length = 0; // 有效数据的总长度
    unsigned *block_length = (unsigned *)malloc(sizeof(unsigned) * count); // 暂存块长度
    for (unsigned  i = 0; i < count; ++i) {
        if (buffers[i].length < 3) {
            printf("RomeSocketConcatenate: Too short buffer: %u\n", buffers[i].length);
            struct Buffer null = {NULL, 0};
            return null;
        }
        block_length[i] = ((unsigned)(buffers[i].buffer[1] << 8)) |
                          ((unsigned)(buffers[i].buffer[2]) & 0x00FF);
        if (block_length[i] >= 0xFFFF) {
            printf("RomeSocketConcatenate: Bad buffer length header: %x and %x = %u\n", buffers[i].buffer[1], buffers[i].buffer[2], block_length[i]);
            struct Buffer null = {NULL, 0};
            return null;
        }
        total_length += block_length[i];
    }
    // 申请内存
    char *complete = (char *)malloc(total_length);
    if (!complete) {
        printf("RomeSocketConcatenate: Fail to allocate memory\n");
        struct Buffer null = {NULL, 0};
        return null;
    }
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
        printf("RomeSocketDecrypt: Fail to initialize libsodium.\n");
        struct Buffer null = {NULL, 0};
        return null;
    }
    // 检查长度
    if (buffer.length < crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES) {
        printf("RomeSocketDecrypt: Too short buffer: %u\n", buffer.length);
        struct Buffer null = {NULL, 0};
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
        struct Buffer result = {plaintext, buffer.length - crypto_secretbox_NONCEBYTES - crypto_secretbox_MACBYTES};
        return result;
    } else {
        // MAC认证失败
        printf("RomeSocketDecrypt: MAC authentication fail");
        struct Buffer null = {NULL, 0};
        return null;
    }
}

struct Buffer RomeSocketEncrypt(struct Buffer buffer, unsigned char *tx) {
    struct Buffer ciphertext;
    // 开辟密文和nonce所需的内存空间
    ciphertext.length = crypto_secretbox_NONCEBYTES + buffer.length + crypto_secretbox_MACBYTES;
    ciphertext.buffer = malloc(ciphertext.length);
    if (!ciphertext.buffer) {
        printf("RomeSocketEncrypt: Fail to allocate memory.\n");
        struct Buffer null = {NULL, 0};
        return null;
    }
    // 生成nonce
    randombytes_buf(ciphertext.buffer, crypto_secretbox_NONCEBYTES);
    // 加密
    crypto_secretbox_easy(
        (unsigned char *)ciphertext.buffer + crypto_secretbox_NONCEBYTES,
        (unsigned char *)buffer.buffer,
        buffer.length,
        (unsigned char *)ciphertext.buffer,
        tx);
    return ciphertext;
}

struct Buffer *RomeSocketSplit(struct Buffer full_block, unsigned *length) {
    // 计算需要的总块数
    unsigned block_num = full_block.length / (BLOCK_LENGTH - HEADER_LENGTH);
    if (full_block.length % (BLOCK_LENGTH - HEADER_LENGTH) != 0) {
        ++block_num;
    }
    // 生成分块
    struct Buffer *buffers = malloc(sizeof(struct Buffer) * block_num);
    unsigned current = 0, remain = full_block.length;
    for (unsigned i = 0; i < block_num; ++i) {
        // 计算块大小
        unsigned current_length = BLOCK_LENGTH - HEADER_LENGTH;
        if (remain < BLOCK_LENGTH - HEADER_LENGTH) {
            current_length = remain;
        }
        buffers[i].buffer = malloc(BLOCK_LENGTH);
        memcpy(buffers[i].buffer + HEADER_LENGTH, full_block.buffer + current, current_length);
        current += current_length, remain -= current_length;
        // 写入文件头
        if (i == block_num - 1) {
            buffers[i].buffer[0] = 0xFF;
        } else {
            buffers[i].buffer[0] = i;
        }
        buffers[i].buffer[1] = current_length >> 8;
        buffers[i].buffer[2] = current_length;
        buffers[i].length = BLOCK_LENGTH;
    }
    // 返回数据
    *length = block_num;
    return buffers;
}

void RomeSocketClearBuffer(struct Buffer buffer) {
    if (buffer.buffer) {
        free(buffer.buffer);
    }
}