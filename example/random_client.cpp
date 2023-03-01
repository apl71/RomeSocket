#include <sodium.h>
#include <iostream>
#include <cctype>
#include <unistd.h>
#include "client.h"

std::string CryptoRandomString(unsigned length, const std::string &list) {
    if (sodium_init() < 0) {
        throw;
    }
    std::string result;
    for (unsigned i = 0; i < length; ++i) {
        uint32_t index = randombytes_uniform(list.length());
        char random_char = list[index];
        result.push_back(random_char);
    }
    return result;
}

Buffer StringToBuffer(const std::string &str) {
    return Buffer{(char *)str.c_str(), (unsigned)str.size() + 1};
}

std::string GetStringFromRemote(Connection conn, const std::string &data) {
    RomeSocketSend(conn, StringToBuffer(data));
    auto buff = RomeSocketReceive(conn, 16);
    std::string response;
    if (buff.buffer) {
        response = std::string(buff.buffer);
    }
    RomeSocketClearBuffer(buff);
    return response;
}

int main() {
    // 连接服务端
    Connection connection = RomeSocketConnect("localhost", 8000, 5);
    for (int i = 0; i < 1000; ++i) {
        // 随机生成串
        std::string data = CryptoRandomString(10000, "1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM");
        if (isdigit(data[0])) {
            // 希望得到随机长度串
            std::cout << "Send data length = " << data.length() << ". Random response expected." << std::endl;
            std::string response = GetStringFromRemote(connection, data);
            std::cout << "Get response length = " << response.length() << std::endl;
        } else if (islower(data[0])) {
            // Pass
            std::cout << "Send data length = " << data.length() << ". No response expected." << std::endl;
            RomeSocketSend(connection, StringToBuffer(data));
        } else {
            // 希望得到同样串
            std::cout << "Send data length = " << data.length() << ". Identical response expected." << std::endl;
            std::string response = GetStringFromRemote(connection, data);
            if (response == data) {
                std::cout << "OK." << std::endl;
            } else {
                std::cout << "Error." << std::endl;
            }
        }
        std::cout << "Sleep for " << i << " seconds." << std::endl;
        sleep(i);
    }
}