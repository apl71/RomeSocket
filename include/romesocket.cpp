#include <liburing.h>
#include <fcntl.h>
#include <cstring>
#include <cstdio>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

constexpr int MAX_CONNECTION = 10;

int InitializeSocket(int port)
{
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        return -1;
    }
    sockaddr_in addr;
    memset((void *)&addr, 0, sizeof(sockaddr_in));
    ((sockaddr_in *)&addr)->sin_family = AF_INET;
    ((sockaddr_in *)&addr)->sin_port = htons(port);
    ((sockaddr_in *)&addr)->sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (sockaddr *)&addr, sizeof(sockaddr_in)) == -1)
    {
        return -1;
    }

    if (listen(sock, MAX_CONNECTION) == -1)
    {
        return -1;
    }
    return sock;
}

int main()
{
    // 初始化io_uring结构体
    io_uring ring;
    io_uring_queue_init(32, &ring, 0);
    // 提交读写请求
    io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    int fd = open("test.txt", O_WRONLY | O_CREAT);
    const char *data = "Hello world";
    iovec iov =
        {
            .iov_base = (void *)data,
            .iov_len = strlen(data),
        };
    io_uring_prep_writev(sqe, fd, &iov, 1, 0);
    io_uring_submit(&ring);
    // 获取IO结果
    io_uring_cqe *cqe;

    while (1)
    {
        io_uring_peek_cqe(&ring, &cqe);
        if (!cqe)
        {
            printf("Waiting...");
        }
        else
        {
            printf("Finished.");
            break;
        }
    }
    io_uring_cqe_seen(&ring, cqe);
    io_uring_queue_exit(&ring);
}