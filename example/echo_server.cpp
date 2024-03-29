#include "romesocket.hpp"
#include "exception.hpp"
#include <iostream>
#include <unistd.h>
#include <fstream>

std::string UnitConversion(double bps) {
    std::vector<std::string> units{"bps", "Kbps", "Mbps", "Gbps", "Tbps"};
    size_t unit = 0;
    while (bps >= 1024.0 && unit < units.size()) {
        bps /= 1024.0;
        ++unit;
    }
    return std::to_string(bps) + units[unit];
}

class EchoServer : public Rocket {
private:
    time_t start_time = 0;
    time_t end_time = 0;
    uint64_t total_bytes = 0;
    time_t last_cout = 0;

    time_t t = 10; // 每10秒执行一次周期任务
    time_t last_time = 0;

    std::mutex io_mutex;

public:
    using Rocket::Rocket;

    void OnStart() override {
        std::cout << "Server start." << std::endl;
        last_time = time(nullptr);
    }

    void ChronicTask(time_t now) override {
        if (last_time + t <= now) {
            std::cout << "Chronic task executing." << std::endl;
            last_time = now;
        }
    }

    void OnRead(char *buff, size_t size, int clinet_id) override {
        // 将开始时间记为首次读取用户输入的时间
        if (start_time == 0) {
            last_cout = start_time = time(nullptr);
        }
        total_bytes += size * 2;
        end_time = time(nullptr);
        // 每三秒计算一次平均吞吐量
        if (end_time - last_cout > 2) {
            std::lock_guard<std::mutex> lock(io_mutex);
            std::cout << "Average throughoutput = " << UnitConversion(total_bytes * 8 / (double)(end_time - start_time)) << std::endl;
            last_cout = time(nullptr);
        }
        int result = Write(buff, size, clinet_id, true);
        if (result < 0) {
            std::cout << "Error when writing: " << result << std::endl;
        }
    }
};

int main(int argc, char **argv) {
    int ring_size = 128;
    if (argc > 1) {
        ring_size = std::atoi(argv[1]);
    }
    EchoServer server(8000);
    server.SetRingSize(ring_size);
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