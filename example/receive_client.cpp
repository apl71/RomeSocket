#include <iostream>
#include <string>
#include "client.h"

int main() {
    auto connection = RomeSocketConnect("localhost", 8000, 15);
    char hello[] = "hello";
    RomeSocketSend(connection, {hello, 6});
    int count = 0;
    while (true) {
        auto recv_buf = RomeSocketReceive(connection, 16);
        if (!recv_buf.buffer) {
            std::cout << "Connection lost." << std::endl;
            break;
        }
        if (std::string(recv_buf.buffer) == "done") {
            std::cout << "done" << std::endl;
            break;
        }
        std::cout << "Get " << count++ << std::endl;
        RomeSocketClearBuffer(recv_buf);
    }
    RomeSocketClose(&connection);
}