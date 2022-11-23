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

#define SERVER "localhost"
#define PORT 8000

const std::string red   = "\033[31m";
const std::string green = "\033[32m";
const std::string reset = "\033[0m";

std::mutex io_mutex;

// 长度为0表示随机长度，范围是1-1024*1024字节
// 需要手动清理返回的数组
Buffer RandomBytes(unsigned length = 0) {
    sodium_init();
    // 随机数
    while (length == 0) {
        length = randombytes_uniform(1024 * 10);
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

void go(int id)
{
    Connection connection = RomeSocketConnect(SERVER, PORT);
    // 测试数量
    unsigned test_cases = 2;
    for (unsigned i = 0; i < test_cases; ++i)
    {
        // 生成随机数据
        Buffer data = RandomBytes();
        RomeSocketSend(connection, data);
        Buffer buffer = RomeSocketReceive(connection, 256);
        int result = Compare(data, buffer);
        io_mutex.lock();
        // io
        if (result != -1) {
            std::cout << red << "[FAIL]" << reset
                      << std::dec << "Test case " << i << " fail: "
                      << "Inconstant first occur at " << result << std::endl;
            std::cout << "length: " << data.length << " and " << buffer.length << std::endl;
            // PrintHex((unsigned char *)data.buffer, data.length);
            // PrintHex((unsigned char *)buffer.buffer, buffer.length);
        } else {
            std::cout << green << "[PASS]" << reset << std::dec << "Test case " << i << " pass" << std::endl;
        }
        io_mutex.unlock();
        delete[]buffer.buffer;
        delete[]data.buffer;
    }

    RomeSocketClose(&connection);
}

int main()
{
    go(1);
    // ThreadPool pool;
    // for (int i = 0; i < 4; ++i)
    //     pool.AddTask(go, i);
}