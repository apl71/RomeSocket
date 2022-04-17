#include "romesocket.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

Rocket::Rocket(uint16_t port) : _port(port)
{
    pool = new ThreadPool();
}

void Rocket::Initialize(int port)
{
    // 初始化套接字
    _sock = socket(PF_INET, SOCK_STREAM, 0);
    if (_sock == -1)
    {
        return;
    }
    sockaddr_in addr;
    memset((void *)&addr, 0, sizeof(struct sockaddr_in));
    ((sockaddr_in *)&addr)->sin_family = AF_INET;
    ((sockaddr_in *)&addr)->sin_port = htons(port);
    ((sockaddr_in *)&addr)->sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(_sock, (sockaddr *)&addr, sizeof(sockaddr_in)) == -1)
    {
        close(_sock);
        _sock = -1;
        return;
    }

    if (listen(_sock, _max_connection) == -1)
    {
        close(_sock);
        _sock = -1;
        return;
    }

    // 初始化uring
    int result = io_uring_queue_init(_ring_size, &_ring, 0);
    if (result != 0)
    {
        close(_sock);
        _sock = -1;
        return;
    }
}

void Rocket::Submit()
{
    io_uring_submit(&_ring);
}

void Rocket::PrepareAccept(struct sockaddr *client_addr, socklen_t *addr_len)
{
    // 获取sqe
    struct io_uring_sqe *sqe = io_uring_get_sqe(&_ring);
    io_uring_prep_accept(sqe, _sock, client_addr, addr_len, 0);
    // 接受用户连接
    struct Request *request = new Request;
    request->buff = NULL;
    request->client_sock = 0;
    request->type = REQUEST_TYPE_ACCEPT;
    io_uring_sqe_set_data(sqe, request);
}

void Rocket::PrepareRead(int client_sock)
{
    struct io_uring_sqe *read_sqe = io_uring_get_sqe(&_ring);
    char *read_buff = new char[_max_buffer_size];
    io_uring_prep_read(read_sqe, client_sock, read_buff, _max_buffer_size, 0);
    struct Request *req = new Request;
    req->buff = read_buff;
    req->client_sock = client_sock;
    req->type = REQUEST_TYPE_READ;
    io_uring_sqe_set_data(read_sqe, req);
}

void Rocket::PrepareWrite(int client_sock, char *to_write, size_t size)
{
    char *write_buff = new char[_max_buffer_size];
    strncpy(write_buff, to_write, size);
    Request *req = new Request;
    req->buff = write_buff;
    req->client_sock = client_sock;
    req->type = REQUEST_TYPE_WRITE;
    struct io_uring_sqe *write_sqe = io_uring_get_sqe(&_ring);
    io_uring_prep_write(write_sqe, client_sock, write_buff, _max_buffer_size, 0);
    strcpy(write_buff, to_write);
    io_uring_sqe_set_data(write_sqe, req);
}

void Rocket::FreeRequest(Request **request)
{
    if (*request)
    {
        if ((*request)->buff)
        {
            delete (*request)->buff;
            (*request)->buff = NULL;
        }
        delete *request;
        *request = NULL;
    }
}

void Rocket::Start()
{
    Initialize(_port);
    if (_sock == -1)
    {
        throw;
    }

    struct sockaddr client_addr;
    socklen_t addr_len = sizeof(client_addr);
    PrepareAccept(&client_addr, &addr_len);
    Submit();

    while (1)
    {
        // 等待一个io事件完成，可能是接收到了用户连接，也有可能是用户发来数据
        // 用user_data字段区分它们
        struct io_uring_cqe *cqe;
        io_uring_wait_cqe(&_ring, &cqe);
        if (cqe->res < 0)
        {
            io_uring_cqe_seen(&_ring, cqe);
            continue;
        }
        else if (cqe->res == 0)
        {
            io_uring_cqe_seen(&_ring, cqe);
            continue;
        }
        Request *cqe_request = reinterpret_cast<Request *>(cqe->user_data);
        switch (cqe_request->type)
        {
        case REQUEST_TYPE_ACCEPT:
            // 接受用户的事件，由于请求队列中的“接受用户”的任务被消耗
            // 这里需要创建一个新的，来保证其他用户能够连接服务器
            // 总之，sq中应该总有一个任务，来接受用户连接
            PrepareAccept(&client_addr, &addr_len);
            // 还要增加一个读任务，用来处理刚刚连过来的用户的请求
            PrepareRead(cqe->res);
            Submit();
            break;
        case REQUEST_TYPE_READ:
        {
            // 读取到了用户的输入
            // 提交一个写任务，把用户的输入echo出去(或者加个私货
            char *buffer = new char[_max_buffer_size];
            strncpy(buffer, cqe_request->buff, _max_buffer_size);
            int *client_sock = new int;
            *client_sock = cqe_request->client_sock;
            pool->AddTask([&](){
                OnRead(buffer, _max_buffer_size, *client_sock);
                delete[]buffer;
                delete client_sock;
            });
            // OnRead(cqe_request->buff, _max_buffer_size, cqe_request->client_sock);
            break;
        }
        case REQUEST_TYPE_WRITE:
            break;
        }
        io_uring_cqe_seen(&_ring, cqe);
        FreeRequest(&cqe_request);
    }
}

Rocket::~Rocket()
{
    io_uring_queue_exit(&_ring);
    if (_sock != -1)
    {
        close(_sock);
    }
    delete pool;
}

int Rocket::Write(char *buff, size_t size, int client_id, bool more)
{
    if (size > _max_buffer_size)
    {
        return -1;
    }
    PrepareWrite(client_id, buff, size);
    if (more)
    {
        PrepareRead(client_id);
    }
    Submit();
    return 1;
}