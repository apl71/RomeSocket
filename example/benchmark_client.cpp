#include "client.h"
#include <iostream>
#include <vector>
#include <string>

std::string UnitConversion(double bps) {
    std::vector<std::string> units{"Bps", "KBps", "MBps", "GBps", "TBps"};
    size_t unit = 0;
    while (bps >= 1024.0 && unit < units.size()) {
        bps /= 1024.0;
        ++unit;
    }
    return std::to_string(bps) + units[unit];
}

int main() {
    auto connection = RomeSocketConnect("localhost", 8000, 30);
    // 先进行下载测试
    std::cout << "Start uploading test." << std::endl;
    time_t start = time(nullptr);
    long long total_bytes = 0;
    char buff[8000] = {'u'};
    int count = 0;
    for (int i = 0; i < 100000; ++i) {
        RomeSocketSend(connection, {buff, 8000});
        total_bytes += 8000;
        if (++count % 10000 == 0) {
            std::cout << count / 10000 << std::endl;
        }
    }
    time_t end = time(nullptr);
    double speed = (double)total_bytes / (double)(end - start);
    std::cout << "Average speed: " << UnitConversion(speed) << std::endl;
    std::cout << "Uploading test ends." << std::endl;
    // 上传测试
    std::cout << "Start download test." << std::endl;
    total_bytes = 0;
    start = time(nullptr);
    RomeSocketSend(connection, {"d", 2});
    count = 0;
    while (true) {
        //std::cout << count << " ";
        auto response = RomeSocketReceive(connection, 16);
        if (!response.buffer) {
            std::cout << "Test no." << count << " fail." << std::endl;
            std::cout << "Fail to receive message." << std::endl;
        }
        total_bytes += response.length;
        if (response.buffer[0] == 'e') {
            break;
        } else if (response.buffer[0] == 'c') {
            // do nothing
        } else {
            std::cout << "strange." << std::endl;
        }
        RomeSocketClearBuffer(response);
        ++count;
    }
    end = time(nullptr);
    speed = (double)total_bytes / (double)(end - start);
    std::cout << "Average speed: " << UnitConversion(speed) << std::endl;
    std::cout << "Downloading test ends." << std::endl;
    RomeSocketClose(&connection);
}