#include "romesocket.hpp"
#include "exception.hpp"
#include <iostream>
#include <unistd.h>
#include <fstream>

class EchoServer : public Rocket
{
public:
    using Rocket::Rocket;

    void OnRead(char *buff, size_t size, int clinet_id) override {
        std::cout << "I receive a message whose length is: " << size << std::endl;
        if (Write(buff, size, clinet_id, true) < 0) {
            std::cout << "Error when writing." << std::endl;
        }
    }
};

int main() {
    EchoServer server(8000);
    int retry = 1;
    int max_retry = 30;
    while (1) {
        try {
            server.Start();
        } catch (SocketException &e) {
            std::cerr << e.what() << std::endl;
            std::cerr << "Retry in 3 seconds. Retry: " << retry++ << std::endl;
            if (retry >= max_retry) {
                exit(0);
            }
            sleep(3);
            continue;
        }
    }
}