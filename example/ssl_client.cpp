#include "RomeSocket.h"
#include <iostream>

int main()
{
    int result = 0;
    Socket sock;
    if ((result = sock.CreateSocket(IPv4, TCP, CLIENT, 2222, "127.0.0.1")) <= 0)
    {
        std::cout << "Error on CreateSocket(): return " << result << std::endl;
    }
    if ((result = sock.ConnectServer()) <= 0)
    {
        std::cout << "Error on ConnectServer(): return " << result << std::endl;
    }
    char recv_buff[25] = { 0x00 };
    sock.ReceiveDataFix(recv_buff, 25);
    std::cout << recv_buff << std::endl;
    char send_buff[25] = "Got it!";
    sock.SendDataFix(send_buff, 25);
    sock.CloseSocket();
}