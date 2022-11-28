#include "romesocket.hpp"
#include "exception.hpp"
#include <iostream>
#include <unistd.h>
#include <fstream>

class EchoServer : public Rocket {
private:
    time_t start_time = 0;
    time_t end_time = 0;
    uint64_t total_bytes = 0;
    time_t last_cout = 0;

public:
    using Rocket::Rocket;

    void OnStart() {
        std::cout << "Server start." << std::endl;
    }

    // average throughoutput = 731352 bytes/second
    void OnRead(char *buff, size_t size, int clinet_id) override {
        // 将开始时间记为首次读取用户输入的时间
        if (start_time == 0) {
            last_cout = start_time = time(nullptr);
        }
        total_bytes += size;
        end_time = time(nullptr);
        // 每三秒计算一次平均吞吐量
        if (end_time - last_cout > 3) {
            std::cout << "Average throughoutput = " << total_bytes / (double)(end_time - start_time) << " bytes/second" << std::endl;
            last_cout = time(nullptr);
        }
        if (Write(buff, size, clinet_id, true) < 0) {
            std::cout << "Error when writing." << std::endl;
        }
    }
};

int main() {
    EchoServer server(8000);
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