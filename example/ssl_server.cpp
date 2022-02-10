#include "RomeSocket.h"
#include <iostream>

int main()
{
    int result = 0;
    Socket sock;
    if ((result = sock.CreateSocket(IPv4, TCP, SERVER, 2222)) <= 0)
    {
        std::cout << "Error on CreateSocket(): return " << result << std::endl;
    }
    
    while (1)
    {
        if ((result = sock.AcceptClient()) <= 0)
        {
            std::cout << "Error on AcceptClient(): return " << result << std::endl;
        }
        char send_buff[25] = "Hello!";
        sock.SendDataFix(send_buff, 25);
        char recv_buff[25] = { 0x00 };
        sock.ReceiveDataFix(recv_buff, 25);
        std::cout << recv_buff << std::endl;
        sock.CloseConnection();
    }
    sock.CloseSocket();
}