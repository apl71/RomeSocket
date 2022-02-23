#include "RomeSocket.h"
#include <iostream>
#include <mutex>
#include <thread>

std::mutex io_mutex;

void Connect(int num);

int main()
{
    int connection = 1;
    for (int i = 0; i < connection; ++i)
    {
        io_mutex.lock();
        std::cout << "Creating new thread " << i << "." << std::endl;
        io_mutex.unlock();
        std::thread t(Connect, i);
        t.detach();
    }
}

void Connect(int num)
{
    int result = 0;
    Socket sock;
    io_mutex.lock();
    if ((result = sock.CreateSocket(IPv4, TCP, CLIENT, 2222, "127.0.0.1", false)) <= 0)
    {
        std::cout << "Error on CreateSocket(): return " << result << std::endl;
        return;
    }
    io_mutex.unlock();
    if ((result = sock.ConnectServer()) <= 0)
    {
        io_mutex.lock();
        std::cout << "Error on ConnectServer(): return " << result << std::endl;
        io_mutex.unlock();
    }
    std::string data;
    if ((result = sock.ReceiveString(data)) <= 0)
    {
        io_mutex.lock();
        std::cout << "Error on receiving: " << result << std::endl;
        io_mutex.unlock();
    }
    io_mutex.lock();
    std::cout << "client no. " << num << " get data: " << data << std::endl;
    io_mutex.unlock();
    data = "Got it!";
    if ((result = sock.SendString(data)) <= 0)
    {
        io_mutex.lock();
        std::cout << "Error on sending: " << result << std::endl;
        io_mutex.unlock();
    }
    for (int i = 0; i < 2; ++i)
    {
        if ((result = sock.ReceiveString(data)) <= 0)
        {
            io_mutex.lock();
            std::cout << "Error on receiving: " << result << std::endl;
            io_mutex.unlock();
        }
        io_mutex.lock();
        std::cout << "client no. " << num << " get data: " << data << std::endl;
        io_mutex.unlock();

        if ((result = sock.SendString("client no. " + std::to_string(num))) <= 0)
        {
            io_mutex.lock();
            std::cout << "Error on sending: " << result << std::endl;
            io_mutex.unlock();
        }
        if ((result = sock.ReceiveString(data)) <= 0)
        {
            io_mutex.lock();
            std::cout << "Error on receiving: " << result << std::endl;
            io_mutex.unlock();
        }
        io_mutex.lock();
        std::cout << "client no. " << num << " get data: " << data << std::endl;
        io_mutex.unlock();
    }
    if ((result = sock.SendString("bye")) <= 0)
    {
        io_mutex.lock();
        std::cout << "Error on sending: " << result << std::endl;
        io_mutex.unlock();
    }
    sock.CloseSocket();
}