#include "RomeSocket.h"
#include <openssl/ssl.h>
#include <fstream>
#if defined(__linux__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#elif defined(_WIN32)
#define _WIN32_WINNT 0x0601
#include <WinSock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

Socket::Socket()
{
    valid = false;
}

int Socket::GetConnection(std::thread::id tid)
{
    int conn = -1;
    thread_lock.lock_shared();
    for (auto iter = threads.begin(); iter != threads.end(); ++iter)
    {
        if (tid == iter->tid)
        {
            conn = iter->conn;
            break;
        }
    }
    thread_lock.unlock_shared();
    return conn;
}

SSL *Socket::GetSSLConnection(std::thread::id tid)
{
    SSL *ssl = nullptr;
    thread_lock.lock_shared();
    for (auto iter = threads.begin(); iter != threads.end(); ++iter)
    {
        if (tid == iter->tid)
        {
            ssl = iter->ssl;
            break;
        }
    }
    thread_lock.unlock_shared();
    return ssl;
}

void Socket::ClearClosedConnection()
{
    // 将已经断开的连接删除
    thread_lock.lock();
    auto iter = threads.begin();
    while (iter != threads.end())
    {
        if (iter->conn == -1 && !iter->is_main_thread)
        {
            iter = threads.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
    thread_lock.unlock();
}

int Socket::CreateSocket(IP_VERSION ip_version, SOCKET_TYPE protocol, ROLE r, unsigned port, std::string ip, bool ssl)
{
    // 已经创建了可用套接字，不能重复创建
    if (valid)
    {
        return 0;
    }

    // 选择IPv4或IPv6
    int version = 0;
    if (ip_version == IPv4)
    {
        version = AF_INET;
    }
    else if (ip_version == IPv6)
    {
        version = AF_INET6;
    }
    else
    {
        return -1;
    }

    role = r;

    // 选择TCP或UDP协议
    int pro = 0;
    if (protocol == TCP)
    {
        pro = SOCK_STREAM;
    }
    else if (protocol == UDP)
    {
        pro = SOCK_DGRAM;
    }
    else
    {
        return -2;
    }

    #if defined(_WIN32)
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(1, 1), &wsa_data);
    #endif

    sock = socket(version, pro, 0);
    #if defined(__linux__)
    if (sock < 0)
    {
        return -7;
    }
    #elif defined(_WIN32)
    if (sock == INVALID_SOCKET)
    {
        return -7;
    }
    #endif
    addr = new struct sockaddr_in;
    addr_size = sizeof(sockaddr_in);
    memset(addr, 0, addr_size);
    ((sockaddr_in *)addr)->sin_family = version;
    ((sockaddr_in *)addr)->sin_port = htons(port);

    host = ip;
    target_port = port;

    if (r == CLIENT)
    {
        if (ssl)
        {
            const SSL_METHOD *method = TLS_client_method();
            ctx = SSL_CTX_new(method);
            if (!ctx)
            {
                return -8;
            }
            client_ssl = SSL_new(ctx);
            SSL_set_fd(client_ssl, sock);
            use_ssl = true;
            valid = true;
            return 1;
        }
        else
        {
            if (inet_pton(version, ip.c_str(), &((sockaddr_in *)addr)->sin_addr) <= 0)
            {
                return -3;
            }
            else
            {
                valid = true;
                return 1;
            }
        }
    }
    else if (r == SERVER)
    {
        ((sockaddr_in *)addr)->sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(sock, (sockaddr *)addr, addr_size) == -1)
        {
            return -4;
        }

        if (listen(sock, max_conn) == -1)
        {
            return -5;
        }
        valid = true;
        // here
        if (ssl)
        {
            const SSL_METHOD *method = TLS_server_method();
            ctx = SSL_CTX_new(method); // free when socket closed
            if (!ctx)
            {
                return -8;
            }
            // 配置证书和私钥，每次自动生成
            EVP_PKEY *pem_key = EVP_RSA_gen(4096);

            if (SSL_CTX_use_PrivateKey(ctx, pem_key) <= 0 ) {
                return -9;
            }

            X509 *x509 = X509_new();
            ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);
            X509_gmtime_adj(X509_getm_notBefore(x509), 0);
            X509_gmtime_adj(X509_getm_notAfter(x509), 3153600000L);
            X509_set_pubkey(x509, pem_key);
            X509_sign(x509, pem_key, EVP_sha256());

            if (SSL_CTX_use_certificate(ctx, x509) <= 0)
            {
                return -10;
            }

            use_ssl = true;
        }
        return 1;
    }
    else
    {
        return -6;
    }
    return 0;
}

bool Socket::IsValid() const
{
    return valid;
}

int Socket::SetSocketSendBuff(int bytes)
{
    return setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char *)&bytes, sizeof(int));
}

int Socket::SetSocketReceiveBuff(int bytes)
{
    return setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char *)&bytes, sizeof(int));
}

void Socket::SetReceiveTimeout(int seconds)
{
    timeval timeout = {seconds};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeval));
}

int Socket::AcceptClient()
{
    ClearClosedConnection();
    if (!valid)
    {
        return -3;
    }

    if (role != SERVER)
    {
        return 0;
    }
    // 检查当前线程是否有对应的连接
    std::thread::id current_tid = std::this_thread::get_id();
    if (GetConnection(current_tid) != -1)
    {
        return -1;
    }

    int new_conn = accept(sock, nullptr, nullptr);

    if (new_conn == -1)
    {
        return -2;
    }
    else
    {
        // 初始化SSL
        SSL *ssl = nullptr;
        if (use_ssl)
        {
            ssl = SSL_new(ctx);
            SSL_set_fd(ssl, new_conn);
            if (SSL_accept(ssl) <= 0)
            {
                return -4;
            }
        }
        // 当主线程创建一个新线程后，主线程对应的连接会被重置为-1
        // 在这种情况下，不能直接将新的连接信息插入线程表中
        // 否则导致线程表冲突（包含两个主线程对应的连接号）
        // 造成下一次创建新线程时无法正常转移连接
        thread_lock.lock();
        bool is_exists = false;
        for (auto iter = threads.begin(); iter != threads.end(); ++iter)
        {
            if (iter->tid == current_tid)
            {
                iter->conn = new_conn;
                iter->ssl = ssl;
                is_exists = true;
            }
        }
        if (!is_exists)
        {
            threads.push_back(Thread{current_tid, new_conn, true, ssl});
        }
        thread_lock.unlock();
        return 1;
    }
}

int Socket::ConnectServer()
{
    if (!valid)
    {
        return -1;
    }

    if (use_ssl)
    {
        bio = BIO_new_ssl_connect(ctx);
        BIO_get_ssl(bio, client_ssl);
        SSL_set_mode(client_ssl, SSL_MODE_AUTO_RETRY);
        BIO_set_conn_hostname(bio, (host + ":" + std::to_string(target_port)).c_str());
        return 1;
    }
    else
    {
        if (connect(sock, (sockaddr *)addr, addr_size) < 0)
        {
            return 0;
        }
        else
        {
            return 1;
        }
    }
}

int Socket::SendData(unsigned char *send_buff, unsigned length)
{
    if (!valid)
    {
        return -1;
    }
    int len = 0;
    if (role == CLIENT)
    {
        if (use_ssl)
        {
            if ((len = BIO_write(bio, (char *)send_buff, length)) < 0)
            {
                return -2;
            }
            else
            {
                return len;
            }
        }
        else
        {
            if ((len = send(sock, (char *)send_buff, length, 0)) < 0)
            {
                return -2;
            }
            else
            {
                return len;
            }
        }
    }
    else if (role == SERVER)
    {
        // 获取当前线程对应的连接
        int conn = GetConnection(std::this_thread::get_id());
        if (conn == -1)
        {
            return -3;
        }
        if (use_ssl)
        {
            SSL *ssl = GetSSLConnection(std::this_thread::get_id());
            if ((len = SSL_write(ssl, send_buff, length)) < 0)
            {
                return -2;
            }
            else
            {
                return len;
            }
        }
        else
        {
            if ((len = send(conn, (char *)send_buff, length, 0)) < 0)
            {
                return -2;
            }
            else
            {
                return len;
            }
        }
    }
    else
    {
        return -4;
    }
}

int Socket::SendDataFix(unsigned char *send_buff, unsigned length)
{
    unsigned sent = 0;
    unsigned remain = length;
    while (sent < length)
    {
        int count = SendData(send_buff + sent, remain);
        if (count < 0)
        {
            return count;
        }
        remain -= count;
        sent += count;
    }
    return sent;
}

int Socket::ReceiveData(unsigned char *recv_buff, unsigned length)
{
    if (!valid)
    {
        return -1;
    }
    int len = 0;
    if (role == CLIENT)
    {
        if (use_ssl)
        {
            if ((len = BIO_read(bio, (char *)recv_buff, length)) < 0)
            {
                return -2;
            }
            else
            {
                return len;
            }
        }
        else
        {
            if ((len = recv(sock, (char *)recv_buff, length, 0)) < 0)
            {
                return -2;
            }
            else
            {
                return len;
            }
        }
    }
    else if (role == SERVER)
    {
        // 获取当前线程对应的连接
        int conn = GetConnection(std::this_thread::get_id());
        if (conn == -1)
        {
            return -3;
        }
        if (use_ssl)
        {
            SSL *ssl = GetSSLConnection(std::this_thread::get_id());
            if ((len = SSL_read(ssl, (char *)recv_buff, length)) < 0)
            {
                return -2;
            }
            else
            {
                return len;
            }
        }
        else
        {
            if ((len = recv(conn, (char *)recv_buff, length, 0)) < 0)
            {
                return -2;
            }
            else
            {
                return len;
            }
        }
    }
    else
    {
        return -4;
    }
}

int Socket::ReceiveDataFix(unsigned char *recv_buff, unsigned length)
{
    unsigned received = 0;
    unsigned remain = length;
    while (received < length)
    {
        int count = ReceiveData(recv_buff + received, remain);
        if (count <= 0)
        {
            return count;
        }
        remain -= count;
        received += count;
    }
    return received;
}

int Socket::SendString(const std::string &str)
{
    size_t length = str.length();
    // 检查字符串长度，无法传输长度超过四字节无符号整数能表示的范围的字符串
    if ((unsigned long long)length > ((1ULL << 32) - 1))
    {
        return -1;
    }
    size_t buff_length = length + 5;
    char *buff = new char[buff_length];
    buff[0] = length >> 24;
    buff[1] = length >> 16;
    buff[2] = length >> 8;
    buff[3] = length;
    strcpy(buff + 4, str.c_str());
    int send_count = SendDataFix(buff, buff_length);
    delete[]buff;
    return send_count;
}

int Socket::ReceiveString(std::string &str)
{
    // 接收首部四字节数据，表示数据长度
    char head_buff[4] = { 0x00 };
    ReceiveDataFix(head_buff, 4);
    size_t length = ((size_t)head_buff[0] << 24) |
                    ((size_t)head_buff[1] << 16) |
                    ((size_t)head_buff[2] <<  8) |
                    ((size_t)head_buff[3]);
    char *buff = new char[length + 1];
    int recv_count = ReceiveDataFix(buff, length + 1);
    str = std::string(buff);
    delete[]buff;
    return recv_count > 0 ? recv_count + 4 : recv_count;
}

void Socket::CloseSocket()
{
    if (!valid)
    {
        return;
    }

    if (use_ssl)
    {
        if (role == CLIENT)
        {
            BIO_free_all(bio);
            SSL_shutdown(client_ssl);
            SSL_free(client_ssl);
        }
        SSL_CTX_free(ctx);
    }

    valid = false;
    threads.clear();
    role = UNKNOWN;
    delete (sockaddr *)addr;
    addr = nullptr;
    addr_size = 0;
    
    #if defined(__linux__)
    close(sock);
    #elif defined(_WIN32)
    closesocket(sock);
    #endif
}

int Socket::CloseConnection()
{
    if (role != SERVER)
    {
        return 0;
    }

    // 获取当前线程对应的连接
    int conn = GetConnection(std::this_thread::get_id());
    if (conn == -1)
    {
        return -1;
    }

    if (use_ssl)
    {
        SSL *ssl = GetSSLConnection(std::this_thread::get_id());
        SSL_shutdown(ssl);
        SSL_free(ssl);
    }

    #if defined(__linux__)
    close(conn);
    #elif defined(_WIN32)
    closesocket(conn);
    #endif
    ResetConnection();
    return 1;
}

void Socket::ResetConnection()
{
    // 获取当前线程对应的连接
    thread_lock.lock();

    std::thread::id current_tid = std::this_thread::get_id();
    for (auto iter = threads.begin(); iter != threads.end(); ++iter)
    {
        if (iter->tid == current_tid)
        {
            iter->conn = -1;
            iter->ssl = nullptr;
            break;
        }
    }
    thread_lock.unlock();
    ClearClosedConnection();
}

