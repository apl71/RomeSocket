#include "client.h"
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>

std::mutex io_mutex;

std::string UnitConversion(double bps) {
    std::vector<std::string> units{"Bps", "KBps", "MBps", "GBps", "TBps"};
    size_t unit = 0;
    while (bps >= 1024.0 && unit < units.size()) {
        bps /= 1024.0;
        ++unit;
    }
    return std::to_string(bps) + units[unit];
}

std::chrono::_V2::system_clock::time_point GetTime() {
    return std::chrono::system_clock::now();
}

double GetDuration(std::chrono::_V2::system_clock::time_point start, std::chrono::_V2::system_clock::time_point end) {
    return double(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) * std::chrono::microseconds::period::num / std::chrono::microseconds::period::den;
}

void DownloadTest() {
    auto connection = RomeSocketConnect("localhost", 8000, 30);
    io_mutex.lock();
    std::cout << "Start downloading test." << std::endl;
    io_mutex.unlock();
    long long total_bytes = 0;
    auto start = GetTime();
    char d[] = "d";
    RomeSocketSend(connection, {d, 2});
    int count = 0;
    while (true) {
        auto response = RomeSocketReceive(connection, 16);
        if (!response.buffer) {
            io_mutex.lock();
            std::cout << "Test no." << count << " fail." << std::endl;
            std::cout << "Fail to receive message." << std::endl;
            io_mutex.unlock();
        }
        total_bytes += response.length;
        if (response.buffer[0] == 'e') {
            break;
        } else if (response.buffer[0] == 'c') {
            // do nothing
        } else {
            io_mutex.lock();
            std::cout << "strange." << std::endl;
            io_mutex.unlock();
        }
        RomeSocketClearBuffer(response);
        ++count;
    }
    auto end = GetTime();
    auto duration = GetDuration(start, end);
    double speed = (double)total_bytes / (double)duration;
    io_mutex.lock();
    std::cout << "Time: " << duration << " seconds." << std::endl;
    std::cout << "Average speed: " << UnitConversion(speed) << std::endl;
    std::cout << "Downloading test ends.\n" << std::endl;
    io_mutex.unlock();
    RomeSocketClose(&connection);
}

void UploadTest(int packs_num) {
    auto connection = RomeSocketConnect("localhost", 8000, 30);
    io_mutex.lock();
    std::cout << "Start uploading test." << std::endl;
    io_mutex.unlock();
    auto start = GetTime();
    long long total_bytes = 0;
    char buff[8000] = {'u'};
    for (int i = 0; i < packs_num; ++i) {
        RomeSocketSend(connection, {buff, 8000});
        total_bytes += 8000;
    }
    auto end = GetTime();
    double duration = GetDuration(start, end);
    double speed = (double)total_bytes / (double)duration;
    io_mutex.lock();
    std::cout << "Time: " << duration << " seconds." << std::endl;
    std::cout << "Average speed: " << UnitConversion(speed) << std::endl;
    std::cout << "Uploading test ends.\n" << std::endl;
    io_mutex.unlock();
    RomeSocketClose(&connection);
}

int main() {
    constexpr int threads_count = 8;
    std::thread threads[threads_count];

    for (int i = 0; i < threads_count; ++i) {
        if (i % 2 == 0) {
            threads[i] = std::thread(UploadTest, 100000);
        } else {
            threads[i] = std::thread(DownloadTest);
        }
    }

    for (int i = 0; i < threads_count; ++i) {
        threads[i].join();
    }
    
}