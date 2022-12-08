/*
    Usage: echo_client [threads] [hostname/IP] [port]
*/

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <mutex>
#include <fstream>
#include <iostream>
#include "thread_pool.hpp"
#include "client.h"

const std::string red   = "\033[31m";
const std::string green = "\033[32m";
const std::string reset = "\033[0m";

std::mutex io_mutex;

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

// 比较buffer内容是否相同，相同返回-1，不同则返回第一个不同的位置
int Compare(Buffer a, Buffer b) {
    unsigned min_len = a.length < b.length ? a.length : b.length;
    for (unsigned i = 0; i < a.length; ++i) {
        if (a.buffer[i] != b.buffer[i]) {
            return (int)i;
        }
    }
    if (a.length != b.length) {
        return (int)min_len + 1;
    }
    return -1;
}

void go(int id, std::string server, int port) {
    Connection connection = RomeSocketConnect(server.c_str(), port, 15);

    // 测试数量
    unsigned test_cases = 1000;
    for (unsigned i = 0; i < test_cases; ++i) {
        // io_mutex.lock();
        // std::cout << "Thread " << id << ":\t";
        // io_mutex.unlock();
        // 生成随机数据
        Buffer data = RandomBytes();
        if (!data.buffer || data.length == 0) {
            std::lock_guard<std::mutex> lock(io_mutex);
            std::cout << red << "[FAIL] " << reset
                      << std::dec << "Test case " << i << " fail: "
                      << "fail to generate data." << std::endl;
            continue;
        }
        RomeSocketSend(connection, data);
        Buffer buffer = RomeSocketReceive(connection, 256);
        if (!buffer.buffer) {
            std::lock_guard<std::mutex> lock(io_mutex);
            std::cout << red << "[FAIL] " << reset
                      << std::dec << "Test case " << i << " fail: "
                      << "buffer is null pointer." << std::endl;
            continue;
        }
        int result = Compare(data, buffer);
        if (result != -1) {
            io_mutex.lock();
            std::cout << red << "[FAIL] " << reset
                      << std::dec << "Test case " << i << " fail: "
                      << "Inconstant first occur at " << result << std::endl;
            std::cout << "length: " << data.length << " and " << buffer.length << std::endl;
            // PrintHex((unsigned char *)data.buffer, data.length);
            // PrintHex((unsigned char *)buffer.buffer, buffer.length);
            io_mutex.unlock();
        } else {
            // std::cout << green << "[PASS] " << reset << std::dec << "Test case " << i << " pass" << std::endl;
        }
        delete[]buffer.buffer;
        delete[]data.buffer;
    }

    RomeSocketClose(&connection);
}

int main(int argc, char **argv) {
    // 默认配置
    int threads = 1;
    std::string server = "localhost";
    int port = 8000;
    // 解析参数
    if (argc >= 2) {
        threads = std::atoi(argv[1]);
    }
    if (argc >= 4) {
        server = std::string(argv[2]);
        port = std::atoi(argv[3]);
    }
    ThreadPool pool(threads, threads);
    for (int i = 0; i < threads; ++i)
        pool.AddTask(go, i, server, port);
}