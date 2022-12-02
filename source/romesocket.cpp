#include "romesocket.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "exception.hpp"
#include <iostream>
#include "layer.h"
#include "client.h"

Rocket::Rocket(uint16_t port) : _port(port) {
    pool = new ThreadPool();
    if (sodium_init() < 0) {
        exit(0);
    }
    crypto_kx_keypair(server_pk, server_sk);
}

void Rocket::SetRingSize(size_t size) {
    _ring_size = size;
}

void Rocket::Initialize(int port) {
    // 初始化套接字
    _sock = socket(PF_INET, SOCK_STREAM, 0);
    if (_sock == -1) {
        std::cout << "Initialize: Fail to create socket." << std::endl;
        return;
    }
    sockaddr_in addr;
    memset((void *)&addr, 0, sizeof(struct sockaddr_in));
    ((sockaddr_in *)&addr)->sin_family = AF_INET;
    ((sockaddr_in *)&addr)->sin_port = htons(port);
    ((sockaddr_in *)&addr)->sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(_sock, (sockaddr *)&addr, sizeof(sockaddr_in)) == -1) {
        close(_sock);
        _sock = -1;
        std::cout << "Initialize: Fail to bind port." << std::endl;
        return;
    }

    if (listen(_sock, _max_connection) == -1) {
        close(_sock);
        _sock = -1;
        std::cout << "Initialize: Fail to listen to port." << std::endl;
        return;
    }

    // 初始化uring
    int result = io_uring_queue_init(_ring_size, &_ring, 0);
    if (result != 0) {
        close(_sock);
        _sock = -1;
        std::cout << "Initialize: Fail to initialize io_uring." << result << std::endl;
        return;
    }
}

void Rocket::Submit() {
    _ring_mutex.lock();
    io_uring_submit(&_ring);
    _ring_mutex.unlock();
}

int Rocket::PrepareAccept(struct sockaddr *client_addr, socklen_t *addr_len) {
    // 获取sqe
    _ring_mutex.lock();
    struct io_uring_sqe *sqe = io_uring_get_sqe(&_ring);
    if (sqe == nullptr) {
        _ring_mutex.unlock();
        return -1;
    }
    io_uring_prep_accept(sqe, _sock, client_addr, addr_len, 0);
    // 接受用户连接
    struct Request *request = new Request;
    request->buff = NULL;
    request->client_sock = 0;
    request->type = REQUEST_TYPE_ACCEPT;
    io_uring_sqe_set_data(sqe, request);
    _ring_mutex.unlock();
    return 1;
}

int Rocket::PrepareRead(int client_sock) {
    _ring_mutex.lock();
    struct io_uring_sqe *read_sqe = io_uring_get_sqe(&_ring);
    if (read_sqe == nullptr) {
        _ring_mutex.unlock();
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
    _ring_mutex.unlock();
    return 1;
}

int Rocket::PrepareWrite(int client_sock, char *to_write, size_t size, bool link) {
    char *write_buff = new char[_max_buffer_size];
    memcpy(write_buff, to_write, size);
    Request *req = new Request;
    req->buff = write_buff;
    req->client_sock = client_sock;
    req->type = REQUEST_TYPE_WRITE;
    _ring_mutex.lock();
    io_uring_sqe *write_sqe = io_uring_get_sqe(&_ring);
    if (write_sqe == nullptr) {
        _ring_mutex.unlock();
        return -1;
    }
    io_uring_prep_write(write_sqe, client_sock, write_buff, _max_buffer_size, 0);
    io_uring_sqe_set_data(write_sqe, req);
    if (link) {
        write_sqe->flags |= IOSQE_IO_LINK;
    }
    _ring_mutex.unlock();
    return 1;
}

void Rocket::FreeRequest(Request **request) {
    if (*request) {
        if ((*request)->buff) {
            delete (*request)->buff;
            (*request)->buff = NULL;
        }
        delete *request;
        *request = NULL;
    }
}

void Rocket::Start() {
    // 初始化套接字、IO Uring等资源
    Initialize(_port);
    if (_sock == -1) {
        throw SocketException();
    }

    // 提交一个初始的sqe，用于接收首个用户的连接
    sockaddr client_addr;
    socklen_t addr_len = sizeof(client_addr);
    if (PrepareAccept(&client_addr, &addr_len) <= 0) {
        std::cout << "[Error] Fatal error: fail to prepare accept." << std::endl;
        exit(0);
    }
    Submit();

    // 开始服务
    OnStart();

    __kernel_timespec kt = {
        0,
        1000 * 1000 * 1000 / 2
    };

    while (1) {
        // 等待一个io事件完成，可能是接收到了用户连接，也有可能是用户发来数据
        // 用user_data字段区分它们
        struct io_uring_cqe *cqe;
        if (io_uring_wait_cqe_timeout(&_ring, &cqe, &kt) != 0) {
            ChronicTask(time(nullptr));
            continue;
        }

        // 删除过期的客户
        auto it = clients.begin();
        while (it != clients.end()) {
            if (it->second.last_time + timeout < time(nullptr)) {
                close(it->second.connection);
                it = clients.erase(it);
            } else {
                ++it;
            }
        }

        // 任务出现问题
        if (cqe->res <= 0) {
            std::unique_lock<std::mutex> lock(_ring_mutex);
            io_uring_cqe_seen(&_ring, cqe);
            continue;
        }
        Request *cqe_request = reinterpret_cast<Request *>(cqe->user_data);
        switch (cqe_request->type) {
        case REQUEST_TYPE_ACCEPT:
            // 接受用户的事件，由于请求队列中的“接受用户”的任务被消耗
            // 这里需要创建一个新的，来保证其他用户能够连接服务器
            // 总之，sq中应该总有一个任务，来接受用户连接
            if (PrepareAccept(&client_addr, &addr_len) <= 0) {
                std::cout << "[Error] Fatal error: fail to prepare accept." << std::endl;
                exit(0);
            }
            // 还要增加一个读任务，用来处理刚刚连过来的用户的请求
            if (PrepareRead(cqe->res) <= 0) {
                std::cout << "[Error] Error: fail to prepare read." << std::endl;
                break;
            }
            break;
        case REQUEST_TYPE_READ: {
            // 读取到了输入
            char *buffer = new char[_max_buffer_size];
            int client_sock = cqe_request->client_sock;
            memcpy(buffer, cqe_request->buff, _max_buffer_size);
            Buffer hello = {buffer, (unsigned)_max_buffer_size};
            unsigned char client_pk[crypto_kx_PUBLICKEYBYTES];
            // 是握手包
            if (RomeSocketCheckHello(hello, client_pk) == 1) {
                Client client;
                client.connection = client_sock;
                client.last_time = time(nullptr);
                // 生成密钥对
                if (crypto_kx_server_session_keys(client.rx, client.tx,
                                        server_pk, server_sk, client_pk) != 0) {
                    break;
                } else {
                    // 发回握手包
                    Buffer shake = {new char[_max_buffer_size], (unsigned)_max_buffer_size};
                    if (RomeSocketGetHello(&shake, server_pk) != 1) {
                        std::cerr << "Fail to generate shake package." << std::endl;
                    }
                    if (PrepareWrite(client_sock, shake.buffer, shake.length) <= 0) {
                        std::cout << "Fail to prepare write task." << std::endl;
                    }
                    clients[client_sock] = client;
                    // 提交一个新的读任务
                    if (PrepareRead(client_sock) <= 0) {
                        std::cout << "Fail to prepare read task." << std::endl;
                    }
                    delete[]shake.buffer;
                    break;
                }
            }
            // 支持任意长度传输，每个“包”的首三字节为标志字节，前第一位表示块顺序
            // 0xFF表示最后一个报文 两字节表示有效数据长度
            char flag = buffer[0];
            
            // 找到暂存的缓冲区
            auto iter = wait_queue.find(client_sock);
            if (iter == wait_queue.end()) {
                // 新增一个表项
                wait_queue[client_sock] = std::vector<Buffer>{{buffer, (unsigned)_max_buffer_size}};
            } else {
                iter->second.push_back({buffer, (unsigned)_max_buffer_size});
                std::sort(iter->second.begin(), iter->second.end(), [=](Buffer a, Buffer b){
                    return (unsigned)a.buffer[0] < (unsigned)b.buffer[0];
                });
            }

            // 检查该分组是否为一个报文的最后一组，如果是，则组装并解密报文，交给用户处理
            if ((unsigned char)flag != 0xFF) {
                // 不是最后一组，因此提交一个新的读任务
                if (PrepareRead(client_sock) <= 0) {
                    std::cout << "Fail to prepare read task." << std::endl;
                }
            } else {
                auto iter = wait_queue.find(client_sock);
                // 消息总块数
                auto buffer_count = iter->second.size();
                // 利用RomeSocketConcatenate拼接层接口拼接缓冲区
                Buffer complete_cipher_buffer = RomeSocketConcatenate(iter->second.data(), buffer_count);
                // 查询私钥
                auto client_iter = clients.find(client_sock);
                if (client_iter == clients.end()) {
                    std::cerr << "fail to get client info." << std::endl;
                    delete[]complete_cipher_buffer.buffer;
                    break;
                }
                unsigned char *server_rx = client_iter->second.rx;
                // 利用RomeSocketDecrypt解密层接口解密
                Buffer complete_plain_buffer = RomeSocketDecrypt(complete_cipher_buffer, server_rx);
                // printf("message: ");
                // PrintHex((unsigned char *)complete_plain_buffer.buffer, complete_plain_buffer.length);
                // printf("\n");
                // 执行读取事件
                pool->AddTask([=, this](){
                    OnRead(complete_plain_buffer.buffer, complete_plain_buffer.length, client_sock);
                    delete[]complete_plain_buffer.buffer;
                    delete[]complete_cipher_buffer.buffer;
                });
                // 清除缓存的消息块
                for (auto &i : iter->second) {
                    delete[]i.buffer;
                }
                iter->second.clear();
            }
            break;
        }
        case REQUEST_TYPE_WRITE:
            break;
        }
        auto iter = clients.find(cqe_request->client_sock);
        if (iter != clients.end()) {
            iter->second.last_time = time(nullptr);
        }
        Submit();
        _ring_mutex.lock();
        io_uring_cqe_seen(&_ring, cqe);
        FreeRequest(&cqe_request);
        _ring_mutex.unlock();
    }
}

Rocket::~Rocket() {
    io_uring_queue_exit(&_ring);
    if (_sock != -1) {
        close(_sock);
    }
    delete pool;
}

int Rocket::Write(char *buff, size_t size, int client_id, bool more) {
    // 查找发送密钥
    auto client_iter = clients.find(client_id);
    if (client_iter == clients.end()) {
        std::cout << "Fail to get user info" << std::endl;
        return -3;
    }
    struct Buffer plaintext = {buff, (unsigned)size};
    // printf("ready to send plaintext length %u:\n", plaintext.length);
    // PrintHex((unsigned char *)plaintext.buffer, plaintext.length);
    // printf("\n");
    struct Buffer ciphertext = RomeSocketEncrypt(plaintext, client_iter->second.tx);
    unsigned length = 0;
    struct Buffer *buffers   = RomeSocketSplit(ciphertext, &length);
    // 逐块发送
    for (unsigned i = 0; i < length; ++i) {
        if (PrepareWrite(client_id, buffers[i].buffer, buffers[i].length, i < length - 1) <= 0) {
            std::cout << "Fail to prepare write" << std::endl;
            return -2;
        }
    }
    // 清理资源
    free(ciphertext.buffer);
    for (unsigned i = 0; i < length; ++i) {
        free(buffers[i].buffer);
    }
    free(buffers);
    
    if (more) {
        if (PrepareRead(client_id) <= 0) {
            std::cout << "Fail to prepare read more" << std::endl;
            return -2;
        }
    }
    Submit();
    return 1;
}