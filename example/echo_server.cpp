#include "romesocket.hpp"
#include "exception.hpp"
#include <iostream>
#include <unistd.h>

class EchoServer : public Rocket
{
public:
    using Rocket::Rocket;

    void OnRead(char *buff, size_t size, int clinet_id) override {
        std::cout << "I receive a message whose length is: " << size << std::endl;
        for (size_t i = 0; i < size; ++i) {
            std::cout << buff[i];
        }
        std::cout << std::endl;
        if (Write(buff, size, clinet_id, true) < 0) {
            std::cout << "Error when writing." << std::endl;
        }
    }
};

int main() {
    EchoServer server(8000);
    while (1) {
        try {
            server.Start();
        } catch (SocketException &e) {
            std::cerr << e.what() << std::endl;
            std::cerr << "Retry in 3 seconds." << std::endl;
            sleep(3);
            continue;
        }
    }
}