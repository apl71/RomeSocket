#include <sodium.h>
#include <iostream>
#include "client.h"

// 长度为0表示随机长度，范围是1-1024*1024字节
// 需要手动清理返回的数组
Buffer RandomBytes(unsigned length = 0) {
    if (sodium_init() < 0) {
        return Buffer{nullptr, 0};
    }
    // 随机数
    while (length == 0) {
        length = randombytes_uniform(1024 * 16);
    }
    char *data = new char[length];
    randombytes_buf(data, length);
    return {data, length};
}

bool CheckBuffer(Buffer a, Buffer b) {
    if (a.length != b.length) {
        return false;
    }
    for (unsigned i = 0; i < a.length; ++i) {
        if (a.buffer[i] != b.buffer[i]) {
            return false;
        }
    }
    return true;
}

int main() {
    // 连接服务端
    Connection connection = RomeSocketConnect("localhost", 8000, 5);
    for (int i = 0; i < 1000; ++i) {
        // 随机生成串
        Buffer data = RandomBytes();
        if ((unsigned char)data.buffer[0] <= 0x0c) {
            // 希望得到随机长度串
            std::cout << "Send data length = " << data.length << ". Random response expected." << std::endl;
            RomeSocketSend(connection, data);
            Buffer response = RomeSocketReceive(connection, 16);
            std::cout << "Get response length = " << response.length << std::endl;
            RomeSocketClearBuffer(response);
        } else if ((unsigned char)data.buffer[0] <= 0xf7) {
            // Pass
            std::cout << "Send data length = " << data.length << ". No response expected." << std::endl;
            RomeSocketSend(connection, data);
        } else {
            // 希望得到同样串
            std::cout << "Send data length = " << data.length << ". Identical response expected." << std::endl;
            RomeSocketSend(connection, data);
            Buffer response = RomeSocketReceive(connection, 16);
            std::cout << "Get response length = " << response.length << std::endl;
            if (CheckBuffer(data, response)) {
                std::cout << "OK." << std::endl;
            } else {
                std::cout << "Error." << std::endl;
            }
            RomeSocketClearBuffer(response);
        }
        RomeSocketClearBuffer(data);
    }
}