/*
    Usage: echo_toy [hostname/IP] [port]
*/
#include <iostream>
#include "client.h"

const std::string red   = "\033[31m";
const std::string green = "\033[32m";
const std::string reset = "\033[0m";

int main(int argc, char **argv) {
    // 默认配置
    std::string server = "localhost";
    int port = 8000;
    // 解析参数
    if (argc >= 3) {
        server = std::string(argv[1]);
        port = std::atoi(argv[2]);
    }
    Connection connection = RomeSocketConnect(server.c_str(), port, 15);

    std::cout << "[---------------- Echo Playground ----------------]" << std::endl;
    std::cout << "Enter string to play(127 charactors max), enter exit if bored." << std::endl;
    std::cout << ">> ";

    // 测试数量
    while (true) {
        char str[128];
        std::cin.getline(str, 128);
        if (std::string(str) == "exit") {
            std::cout << "bye." << std::endl;
            break;
        }
        Buffer message = {str, 128};
        RomeSocketSend(connection, message);
        Buffer echo = RomeSocketReceive(connection, 2);
        std::cout << "<< " << std::string(echo.buffer) << std::endl;
        std::cout << ">> ";
    }

    RomeSocketClose(&connection);
}