#include "romesocket.hpp"
#include "exception.hpp"
#include <iostream>
#include <unistd.h>
#include <fstream>
#include <chrono>

class BenchmarkServer : public Rocket {
private:
    static int64_t GetTime() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
public:
    using Rocket::Rocket;

    // 截至上一次统计时，总共传输的字节数
    int64_t last_bytes = 0;
    // 当前总共传输的字节数
    int64_t current_bytes = 0;
    // 上一次统计的时间
    int64_t last_time;

    // 每秒执行一次统计
    void ChronicTask(time_t now) override {
        // 获取当前时间
        int64_t current_time = GetTime();
        // 计算时间差
        int64_t delta_time = current_time - last_time;
        // 计算传输字节数的差
        int64_t delta_bytes = current_bytes - last_bytes;
        // 计算速度
        double speed = (double)delta_bytes / (double)delta_time * 1000.0 / 1024.0 / 1024.0;
        //std::cout << "[" << current_time << "]" << "Speed: " << speed << " MegaBytes per second." << std::endl;
        std::cout << current_time << " " << speed << std::endl;
        // 更新统计时间和数据
        last_time = GetTime();
        last_bytes = current_bytes;
    }

    void OnStart() override {
        last_time = GetTime();
    }

    void OnRead(char *buff, size_t size, int client_id) override {
        if (buff[0] == 'u') {
            // 上传数据测试，不需要做任何事
            current_bytes += size;
            Pass(client_id);
        } else if (buff[0] == 'd') {
            int data_length = 8000;
            // 下载测试，随机发送100000个数据包，起始字节为c表示后续还有报文，e表示结束发送
            char data[data_length];
            data[0] = 'c';
            int result;
            for (int i = 0; i < 100000 - 1; ++i) {
                //std::cout << i << " ";
                result = Write(data, data_length, client_id, false);
                if (result <= 0) {
                    std::cout << "Fail to write." << std::endl;
                }
                current_bytes += data_length;
            }
            data[0] = 'e';
            result = Write(data, data_length, client_id, true);
            current_bytes += data_length;
            if (result <= 0) {
                std::cout << "Fail to write." << std::endl;
            }
        } else if (buff[0] == 's') {
            std::cout << "shuting down." << std::endl;
            Shutdown();
        } else {
            std::cout << "strange." << std::endl;
            Pass(client_id);
        }
    }
};

int main(int argc, char **argv) {
    int ring_size = 4096;
    if (argc > 1) {
        ring_size = std::atoi(argv[1]);
    }
    BenchmarkServer server(8000);
    server.SetRingSize(ring_size);
    server.SetRegisterBuffer(false);
    int retry = 1;
    int max_retry = 30;
    while (!server.IsShutdown()) {
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