#include "RomeSocket.h"
#include <iostream>
#include <mutex>
#include <signal.h>

std::mutex io_mutex;

void Serve(Socket &sock);

int main()
{
    signal(SIGPIPE, SIG_IGN);
    int result = 0;
    Socket sock;
    if ((result = sock.CreateSocket(IPv4, TCP, SERVER, 2222, "", false)) <= 0)
    {
        std::cout << "Error on CreateSocket(): return " << result << std::endl;
    }

    io_mutex.lock();
    std::cout << "================================== " << std::endl;
    sock.PrintThreadTable();
    std::cout << "================================== " << std::endl;
    io_mutex.unlock();
    
    while (1)
    {
        if ((result = sock.AcceptClient()) <= 0)
        {
            std::cout << "Error on AcceptClient(): return " << result << std::endl;
        }
        std::string data = "Hello!";
        if ((result = sock.SendString(data)) <= 0)
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
        std::cout << data << std::endl;
        io_mutex.unlock();
        std::function<void(Socket &)> func(Serve);
        sock.ThreadDetach(func, sock);
        io_mutex.lock();
        std::cout << "================================== " << std::endl;
        sock.PrintThreadTable();
        std::cout << "================================== " << std::endl;
        io_mutex.unlock();
    }
    sock.CloseSocket();
}

void Serve(Socket &sock)
{
    int result = 0;
    while (1)
    {
        if ((result = sock.SendString("I'm Ready")) <= 0)
        {
            io_mutex.lock();
            std::cout << "Error on sending: " << result << std::endl;
            io_mutex.unlock();
        }
        std::string name;
        if ((result = sock.ReceiveString(name)) <= 0)
        {
            io_mutex.lock();
            std::cout << "Error on receiving: " << result << std::endl;
            io_mutex.unlock();
        }

        if (name == "bye")
        {
            io_mutex.lock();
            std::cout << "End serving." << std::endl;
            io_mutex.unlock();
            break;
        }
        else
        {
            std::string hello = "Welcome! " + name;
            io_mutex.lock();
            std::cout << hello << std::endl;
            io_mutex.unlock();
            if ((result = sock.SendString(hello)) <= 0)
            {
                io_mutex.lock();
                std::cout << "Error on sending: " << result << std::endl;
                io_mutex.unlock();
            }
        }
    }
    sock.CloseConnection();
}