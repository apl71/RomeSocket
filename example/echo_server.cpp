#include "romesocket.hpp"
#include <iostream>

class EchoServer : public Rocket
{
public:
    using Rocket::Rocket;

    void OnRead(char *buff, size_t size, int clinet_id) override
    {
        std::cout << "I receive a message whose length is: " << size << std::endl;
        char to_write[8192] = "Hello, you say \"";
        strncat(to_write, buff, 8000);
        strcat(to_write, "\"");
        if (Write(to_write, 8192, clinet_id, true) < 0)
        {
            std::cout << "Error when writing." << std::endl;
        }
    }
};

int main()
{
    EchoServer server(8000);
    server.Start();
}