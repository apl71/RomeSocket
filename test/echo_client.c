#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

#define SERVER "localhost"
#define PORT 8000
#define BUFFER_SIZE 4096

int main()
{
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    size_t addr_size = sizeof(struct sockaddr_in);
    memset(&addr, 0, addr_size);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER, &addr.sin_addr);
    connect(sock, (struct sockaddr *)&addr, addr_size);

    char send_buff[BUFFER_SIZE];
    char recv_buff[BUFFER_SIZE];
    while (scanf("%s", send_buff))
    {
        send(sock, send_buff, BUFFER_SIZE, 0);
        recv(sock, recv_buff, BUFFER_SIZE, 0);
        printf("%s\n", recv_buff);
    }

    close(sock);
}