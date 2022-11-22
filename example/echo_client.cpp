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

std::mutex io_mutex;

void go(int id)
{
    Connection connection = RomeSocketConnect(SERVER, PORT);

    // 读取一个长度大于8192的文件
    std::ifstream ifs("../../LICENSE", std::ios::in | std::ios::ate | std::ios::binary);
    if (!ifs.is_open()) {
        printf("cannot read file\n");
        return;
    }
    auto size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    char *send_buff = new char[(unsigned)size];
    ifs.read(send_buff, (unsigned)size);
    send_buff[size] = 0x00;
    for (int i = 0; i < 1; ++i)
    {
        // sprintf(send_buff, "Hello, I'm thread %d, request %d", id, i);
        // send(sock, send_buff, BUFFER_SIZE, 0);
        RomeSocketSend(connection, send_buff, (unsigned)size);
        struct Buffer buffer = RomeSocketReceive(connection, 256);
        io_mutex.lock();
        // io
        std::cout << "get message length: " << buffer.length << std::endl;
        io_mutex.unlock();
        free(buffer.buffer);
    }

    ifs.close();
    RomeSocketClose(&connection);
}

int main()
{
    go(1);
    // ThreadPool pool;
    // for (int i = 0; i < 4; ++i)
    //     pool.AddTask(go, i);
}