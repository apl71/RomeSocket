#include "romesocket.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "exception.hpp"
#include <iostream>

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
    _ring_mutex.lock();
    io_uring_submit(&_ring);
    _ring_mutex.unlock();
}

int Rocket::PrepareAccept(struct sockaddr *client_addr, socklen_t *addr_len)
{
    // 获取sqe
    _ring_mutex.lock();
    struct io_uring_sqe *sqe = io_uring_get_sqe(&_ring);
    if (sqe == nullptr)
    {
        return -1;
    }
    io_uring_prep_accept(sqe, _sock, client_addr, addr_len, 0);
    // 接受用户连接
    struct Request *request = new Request;
    request->buff = NULL;
    request->client_sock = 0;
    request->type = REQUEST_TYPE_ACCEPT;
    io_uring_sqe_set_data(sqe, request);
    // std::cout << "PrepareAccept" << std::endl;
    _ring_mutex.unlock();
    return 1;
}

int Rocket::PrepareRead(int client_sock)
{
    _ring_mutex.lock();
    struct io_uring_sqe *read_sqe = io_uring_get_sqe(&_ring);
    if (read_sqe == nullptr)
    {
        return -1;
    }
    char *read_buff = new char[_max_buffer_size];
    memset(read_buff, 0, _max_buffer_size);
    io_uring_prep_read(read_sqe, client_sock, read_buff, _max_buffer_size, 0);
    
    Request *req = new Request;
    req->buff = read_buff;
    req->client_sock = client_sock;
    req->type = REQUEST_TYPE_READ;
    io_uring_sqe_set_data(read_sqe, req);
    // std::cout << "PrepareRead" << std::endl;
    _ring_mutex.unlock();
    return 1;
}

int Rocket::PrepareWrite(int client_sock, char *to_write, size_t size, bool link)
{
    char *write_buff = new char[_max_buffer_size];
    memcpy(write_buff, to_write, size);
    Request *req = new Request;
    req->buff = write_buff;
    req->client_sock = client_sock;
    req->type = REQUEST_TYPE_WRITE;
    _ring_mutex.lock();
    io_uring_sqe *write_sqe = io_uring_get_sqe(&_ring);
    if (write_sqe == nullptr)
    {
        return -1;
    }
    io_uring_prep_write(write_sqe, client_sock, write_buff, _max_buffer_size, 0);
    
    //strcpy(write_buff, to_write);
    io_uring_sqe_set_data(write_sqe, req);
    //std::cout << "PrepareWrite task: " << size << " bytes." << std::endl;
    //std::cout << "Ready to write: " << write_buff << std::endl;
    if (link) {
        write_sqe->flags |= IOSQE_IO_LINK;
    }
    _ring_mutex.unlock();
    return 1;
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
        throw SocketException();
    }

    sockaddr client_addr;
    socklen_t addr_len = sizeof(client_addr);
    if (PrepareAccept(&client_addr, &addr_len) <= 0)
    {
        // std::cout << "[Error] Fatal error: fail to prepare accept." << std::endl;
        exit(0);
    }
    Submit();

    __kernel_timespec ts = {
        .tv_sec = 0L,
        .tv_nsec = 1L
    };

    while (1)
    {
        // 等待一个io事件完成，可能是接收到了用户连接，也有可能是用户发来数据
        // 用user_data字段区分它们
        struct io_uring_cqe *cqe;
        if (std::unique_lock<std::mutex> lock(_ring_mutex);
            io_uring_wait_cqe_timeout(&_ring, &cqe, &ts) != 0)
        {
            // std::cout << "Timeout, continue." << std::endl;
            continue;
        }
        if (cqe->res <= 0)
        {
            std::unique_lock<std::mutex> lock(_ring_mutex);
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
            if (PrepareAccept(&client_addr, &addr_len) <= 0)
            {
                // std::cout << "[Error] Fatal error: fail to prepare accept." << std::endl;
                exit(0);
            }
            // 还要增加一个读任务，用来处理刚刚连过来的用户的请求
            if (PrepareRead(cqe->res) <= 0)
            {
                // std::cout << "[Error] Error: fail to prepare read." << std::endl;
                continue;
            }
            break;
        case REQUEST_TYPE_READ:
        {
            // 读取到了输入
            char *buffer = new char[_max_buffer_size];
            memcpy(buffer, cqe_request->buff, _max_buffer_size);
            // 支持任意长度传输，每个“包”的首两字节为标志字节，前两位为非00表示后续有块
            // 11表示最后一个报文 后14位表示该缓冲区中的有效数据长度
            char flag = buffer[0];
            uint16_t length = uint16_t(buffer[1]) << 8 | (uint16_t(buffer[2]) & 0x00FF);
            std::cout << "received packet " << (unsigned)flag << ": length = " << length << std::endl;
            if (length > _max_buffer_size - SERVER_HEADER_LENGTH) {
                length = _max_buffer_size - SERVER_HEADER_LENGTH;
            }
            int client_sock = cqe_request->client_sock;
            
            // 找到暂存的缓冲区
            auto iter = wait_queue.find(client_sock);
            if (iter == wait_queue.end()) {
                // 新增一个表项
                wait_queue[client_sock] = std::vector<Buffer>{{buffer, flag, length}};
            } else {
                iter->second.push_back({buffer, flag, length});
                std::sort(iter->second.begin(), iter->second.end(), [=](Buffer a, Buffer b){
                    return (unsigned char)a.flag < (unsigned char)b.flag;
                });
            }

            if ((unsigned char)flag != 0xFF) {
                // 提交一个新的读任务
                PrepareRead(client_sock);
            } else {
                unsigned blocks = 0;
                auto iter = wait_queue.find(client_sock);
                // 如果前面还有项，一起拿出来
                if (iter != wait_queue.end()) {
                    blocks += iter->second.size();
                }
                // 计算总长度
                unsigned total_length = 0;
                for (auto i : iter->second) {
                    total_length += i.length;
                }
                char *complete_buffer = new char[total_length];
                memset(complete_buffer, 0, total_length);
                // 复制内存，并清理结构体中的动态内存
                // TODO:如果能提前知道长度，就可以一次性申请大内存，就不必复制一遍了
                unsigned pointer = 0;
                for (auto i : iter->second) {
                    if (i.buffer) {
                        memcpy(complete_buffer + pointer, i.buffer + SERVER_HEADER_LENGTH, i.length);
                        delete[]i.buffer;
                        pointer += (_max_buffer_size - SERVER_HEADER_LENGTH);
                    }
                }
                // 执行读取事件
                pool->AddTask([=, this](){
                    OnRead(complete_buffer, total_length, client_sock);
                    delete[]complete_buffer;
                });
            }
            break;
        }
        case REQUEST_TYPE_WRITE:
            // std::cout << "Write: " << cqe_request->buff << " done." << std::endl;
            break;
        }
        Submit();
        _ring_mutex.lock();
        io_uring_cqe_seen(&_ring, cqe);
        FreeRequest(&cqe_request);
        _ring_mutex.unlock();
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

int Rocket::Write(char *buff, size_t size, int client_id, bool more) {
    // 拆分为大小为8191（_max_buffer_size - 1）的块
    unsigned blocks = size / (_max_buffer_size - SERVER_HEADER_LENGTH) + 1;
    size_t offset = 0;
    for (unsigned i = 0; i < blocks; ++i) {
        char buffer[_max_buffer_size];
        memset(buffer, 0, _max_buffer_size);
        buffer[0] = (i == blocks - 1) ? 0xFF : (char)i;
        unsigned copy_size = (size - offset > _max_buffer_size - SERVER_HEADER_LENGTH) ?
            (_max_buffer_size - SERVER_HEADER_LENGTH) : (size - offset);
        buffer[1] = copy_size >> 8;
        buffer[2] = copy_size;
        memcpy(buffer + SERVER_HEADER_LENGTH, buff + offset, copy_size);
        offset += (_max_buffer_size - SERVER_HEADER_LENGTH);
        if (PrepareWrite(client_id, buffer, _max_buffer_size, i < blocks - 1) <= 0) {
            return -2;
        }
    }
    
    if (more) {
        if (PrepareRead(client_id) <= 0) {
            return -2;
        }
    }
    Submit();
    return 1;
}