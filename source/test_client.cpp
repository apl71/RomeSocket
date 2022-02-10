#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>

void StartClient(int port);

SSL_CTX *CreateContext()
{
    const SSL_METHOD *method = TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx)
    {
        // error
        exit(EXIT_FAILURE);
    }
    return ctx;
}

int main()
{
    StartClient(2222);
}

void StartClient(int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    // struct sockaddr_in *addr;
    // addr = new struct sockaddr_in;
    // int addr_size = sizeof(sockaddr_in);
    // memset(addr, 0, addr_size);
    // ((sockaddr_in *)addr)->sin_family = AF_INET;
    // ((sockaddr_in *)addr)->sin_port = htons(port);

    // BIO *bio = BIO_new_connect("127.0.0.1:2222");
    // if (!bio)
    // {
    //     exit(0);
    // }
    // if (BIO_do_connect(bio) <= 0)
    // {
    //     exit(0);
    // }
    // 创建SSL环境变量
    SSL_CTX *ctx = CreateContext();
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);

    BIO *bio = BIO_new_ssl_connect(ctx);
    BIO_get_ssl(bio, ssl);
    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
    BIO_set_conn_hostname(bio, "127.0.0.1:2222");
    if (BIO_do_connect(bio) <= 0)
    {
        exit(0);
    }
    int rst = 0;
    // if ((rst = SSL_connect(ssl)) <= 0)
    // {
    //     std::cout << rst << std::endl;
    //     exit(0);
    // }
    char buff[6];
    BIO_read(bio, buff, 6);
    std::cout << buff << std::endl;
    char send[] = "got!\n";
    BIO_write(bio, send, 6);

BIO_free_all(bio);
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sock);
    SSL_CTX_free(ctx);
}