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
    // int sock = socket(PF_INET, SOCK_STREAM, 0);
    // struct sockaddr_in addr;
    // size_t addr_size = sizeof(struct sockaddr_in);
    // memset(&addr, 0, addr_size);
    // addr.sin_family = AF_INET;
    // addr.sin_port = htons(PORT);
    // inet_pton(AF_INET, SERVER, &addr.sin_addr);
    // connect(sock, (struct sockaddr *)&addr, addr_size);
    int sock = RomeSocketConnect(SERVER, PORT);

    // 读取一个长度大于8192的文件
    std::ifstream ifs("../../LICENSE", std::ios::in | std::ios::ate | std::ios::binary);
    if (!ifs.is_open()) {
        printf("cannot read file\n");
        return;
    }
    auto size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    char *send_buff = new char[(unsigned)size + 1];
    ifs.read(send_buff, (unsigned)size);
    send_buff[size] = 0x00;
    for (int i = 0; i < 1; ++i)
    {
        // sprintf(send_buff, "Hello, I'm thread %d, request %d", id, i);
        // send(sock, send_buff, BUFFER_SIZE, 0);
        RomeSocketSend(sock, send_buff, (unsigned)size + 1);
        char *buffer = NULL;
        unsigned length = RomeSocketReceive(sock, &buffer);
        io_mutex.lock();
        for (unsigned i = 0; i < length; ++i) {
            std::cout << buffer[i];
        }
        std::cout << std::endl;
        io_mutex.unlock();
        RomeSocketClearBuffer(&buffer);
    }

    ifs.close();
    close(sock);
}

int main()
{
    go(1);
    // ThreadPool pool;
    // for (int i = 0; i < 4; ++i)
    //     pool.AddTask(go, i);
}