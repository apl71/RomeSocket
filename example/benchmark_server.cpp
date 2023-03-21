#include "romesocket.hpp"
#include "exception.hpp"
#include <iostream>
#include <unistd.h>
#include <fstream>

class BenchmarkServer : public Rocket {
public:
    using Rocket::Rocket;

    void OnRead(char *buff, size_t size, int client_id) override {
        if (buff[0] == 'u') {
            // 上传数据测试，不需要做任何事
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
            }
            data[0] = 'e';
            result = Write(data, data_length, client_id, true);
            if (result <= 0) {
                std::cout << "Fail to write." << std::endl;
            }
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
    server.SetLogFile("./benchmark_server.log", true);
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