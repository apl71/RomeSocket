#include <liburing.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

#define RING_SIZE 4
#define MAX_CONNECTION 500
#define PORT 8000
#define WRITE_BUFFER_SIZE READ_BUFFER_SIZE
#define READ_BUFFER_SIZE 4096
#define REQUEST_TYPE_ACCEPT 1
#define REQUEST_TYPE_READ 2
#define REQUEST_TYPE_WRITE 3

// 用来标记sqe，是监听用户用的、还是接受用户输入用的？
struct Request
{
    int type;
    int client_sock;
    char *buff;
};

void FatalError(const char *msg)
{
    printf("%s\n", msg);
    exit(1);
}

int InitializeSocket(int port)
{
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        return -1;
    }
    struct sockaddr_in addr;
    memset((void *)&addr, 0, sizeof(struct sockaddr_in));
    ((struct sockaddr_in *)&addr)->sin_family = AF_INET;
    ((struct sockaddr_in *)&addr)->sin_port = htons(port);
    ((struct sockaddr_in *)&addr)->sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1)
    {
        return -1;
    }

    if (listen(sock, MAX_CONNECTION) == -1)
    {
        return -1;
    }
    return sock;
}

void Submit(struct io_uring *ring)
{
    int submitted = io_uring_submit(ring);
    printf("SubmitAccept(): %d sqes submitted.\n", submitted);
}

void PrepareAccept(int sock, struct io_uring *ring, struct sockaddr *client_addr, socklen_t *addr_len)
{
    // 获取sqe
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_prep_accept(sqe, sock, client_addr, addr_len, 0);
    // 接受用户连接
    struct Request *request = malloc(sizeof(struct Request));
    request->buff = NULL;
    request->client_sock = 0;
    request->type = REQUEST_TYPE_ACCEPT;
    io_uring_sqe_set_data(sqe, request);
}

void PrepareRead(int client_sock, struct io_uring *ring)
{
    struct io_uring_sqe *read_sqe = io_uring_get_sqe(ring);
    char *read_buff = malloc(READ_BUFFER_SIZE);
    io_uring_prep_read(read_sqe, client_sock, read_buff, READ_BUFFER_SIZE, 0);
    struct Request *req = malloc(sizeof(struct Request));
    req->buff = read_buff;
    req->client_sock = client_sock;
    req->type = REQUEST_TYPE_READ;
    io_uring_sqe_set_data(read_sqe, req);
}

void PrepareWrite(int client_sock, struct io_uring *ring, char *to_write)
{
    char *write_buff = malloc(WRITE_BUFFER_SIZE);
    struct Request *req = malloc(sizeof(struct Request));
    req->buff = write_buff;
    req->client_sock = client_sock;
    req->type = REQUEST_TYPE_WRITE;
    struct io_uring_sqe *write_sqe = io_uring_get_sqe(ring);
    io_uring_prep_write(write_sqe, client_sock, write_buff, WRITE_BUFFER_SIZE, 0);
    strcpy(write_buff, to_write);
    io_uring_sqe_set_data(write_sqe, req);
}

void FreeRequest(struct Request **request)
{
    if (*request)
    {
        if ((*request)->buff)
        {
            free((*request)->buff);
            (*request)->buff = NULL;
        }
        free(*request);
        *request = NULL;
    }
}

int main()
{
    int result = 0;
    // 初始化uring
    struct io_uring uring, *ring = &uring;
    result = io_uring_queue_init(RING_SIZE, ring, 0);
    if (result != 0)
    {
        FatalError("Fail to initialize io_uring.");
    }

    // 初始化socket
    int sock = InitializeSocket(PORT);
    if (sock == -1)
    {
        FatalError("Fail to create socket.");
    }

    struct sockaddr client_addr;
    socklen_t addr_len = sizeof(client_addr);
    PrepareAccept(sock, ring, &client_addr, &addr_len);
    Submit(ring);

    while (1)
    {
        // 等待一个io事件完成，可能是接收到了用户连接，也有可能是用户发来数据
        // 用user_data字段区分它们
        struct io_uring_cqe *cqe;
        io_uring_wait_cqe(ring, &cqe);
        printf("cqe->res = %d\n", cqe->res);
        if (cqe->res < 0)
        {
            printf("error cqe. skip it.\n");
            io_uring_cqe_seen(ring, cqe);
            continue;
        }
        else if (cqe->res == 0)
        {
            printf("a client quit.\n");
            io_uring_cqe_seen(ring, cqe);
            continue;
        }
        struct Request *cqe_request = cqe->user_data;
        switch (cqe_request->type)
        {
        case REQUEST_TYPE_ACCEPT:
            // 接受用户的事件，由于请求队列中的“接受用户”的任务被消耗
            // 这里需要创建一个新的，来保证其他用户能够连接服务器
            // 总之，sq中应该总有一个任务，来接受用户连接
            PrepareAccept(sock, ring, &client_addr, &addr_len);
            // 还要增加一个读任务，用来处理刚刚连过来的用户的请求
            PrepareRead(cqe->res, ring);
            printf("client sock: %d\n", cqe->res);
            Submit(ring);
            break;
        case REQUEST_TYPE_READ:
            // 读取到了用户的输入
            // 提交一个写任务，把用户的输入echo出去(或者加个私货
            {
                char *data = cqe_request->buff;
                char to_write[WRITE_BUFFER_SIZE] = "Hello, you say \"";
                strcat(to_write, data);
                strcat(to_write, "\"");
                PrepareWrite(cqe_request->client_sock, ring, to_write);
                PrepareRead(cqe_request->client_sock, ring);
                Submit(ring);
                break;
            }
        case REQUEST_TYPE_WRITE:
            printf("write done.\n");
            break;
        default:
            printf("something goes wrong.\n");
        }
        io_uring_cqe_seen(ring, cqe);
        printf("1 cqe seen.\n");
        FreeRequest(&cqe_request);
    }

    // 销毁uring
    io_uring_queue_exit(ring);
    close(sock);
    return 0;
}