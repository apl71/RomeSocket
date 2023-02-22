#include <iostream>
#include <unistd.h>
#include "romesocket.hpp"
#include "exception.hpp"

// 随机返回的服务器
// 如果收到的消息串首字节为0x00-0x0c，响应随机长度串
// 如果首字节为0x0d-0xf7，则Pass
// 如果首字节为0xf8-0xff，则返回相同消息

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

class RandomServer : public Rocket {
private:
    std::mutex io_mutex;

public:
    using Rocket::Rocket;

    void OnStart() override {
        std::cout << "Server start." << std::endl;
    }

    void OnRead(char *buff, size_t size, int client_id) override {
        // 将开始时间记为首次读取用户输入的时间
        if ((unsigned char)buff[0] <= 0x0c) {
            // 返回随机长度
            Buffer buffer = RandomBytes();
            Write(buffer.buffer, buffer.length, client_id, true);
            delete[]buffer.buffer;
        } else if ((unsigned char)buff[0] <= 0xf7) {
            // Pass
            Pass(client_id);
        } else {
            // 返回同样消息
            Write(buff, size, client_id, true);
        }
    }
};

int main(int argc, char **argv) {
    RandomServer server(8000);
    int retry = 1;
    int max_retry = 30;
    while (1) {
        try {
            server.Start();
        } catch (SocketException &e) {
            std::cerr << e.what() << std::endl;
            std::cerr << "Retry in 3 seconds. Retry: " << retry++ << std::endl;
            if (retry >= max_retry) {
                exit(0);
            }
            sleep(3);
            continue;
        }
    }
}