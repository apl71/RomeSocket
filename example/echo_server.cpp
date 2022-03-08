#include "romesocket.hpp"
#include <iostream>

class EchoServer : public Rocket
{
public:
    EchoServer(uint16_t port) : Rocket(port) {}

    void OnRead(char *buff, size_t size, int clinet_id) override
    {
        char to_write[8192] = "Hello, you say \"";
        strncat(to_write, buff, size);
        strcat(to_write, "\"");
        if (Write(to_write, 8192, clinet_id, true) < 0)
        {
            std::cout << "Message too long." << std::endl;
        }
    }
};

int main()
{
    EchoServer server(8000);
    server.Start();
}