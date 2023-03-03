// 测试：服务端大量发送消息，Write()的more参数采用true或false，收到done为止
#include "romesocket.hpp"
#include "exception.hpp"
#include <iostream>
#include <unistd.h>
#include <fstream>

class ReceiveServer : public Rocket {
public:
    time_t last_beat = 0;
    time_t interval = 3;

    using Rocket::Rocket;

    void ChronicTask(time_t now) override {
        // 每三秒发送一次心跳包
        if (last_beat + interval <= now) {
            std::cout << "Chronic task" << std::endl;
            last_beat = now;
        }
    }

    void OnStart() {
        // 设定心跳包起始时间
        last_beat = time(nullptr);
    }

    void OnRead(char *buff, size_t size, int client_id) override {
        // 发送k次数据，最后发送done
        int k = 1000, result, count = 0;
        char send_buf[8000];
        for (int i = 0; i < k; ++i) {
            if (count++ % 8 == 0) {
                sleep(2);
            }
            send_buf[0] = i;
            std::cout << "Sending " << i << std::endl;
            result = Write(send_buf, 8000, client_id, false);
            if (result < 0) {
                std::cout << "Error on writing, code = " << result << ", i = " << i << std::endl;
                return;
            }
        }
        strcpy(send_buf, "done");
        result = Write(send_buf, 8000, client_id, true);
        if (result < 0) {
            std::cout << "Error when writing: " << result << std::endl;
        }
    }
};

int main(int argc, char **argv) {
    int ring_size = 16;
    if (argc > 1) {
        ring_size = std::atoi(argv[1]);
    }
    ReceiveServer server(8000);
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