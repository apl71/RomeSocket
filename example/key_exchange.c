#include <sodium.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

void PrintHex(unsigned char *data, unsigned length) {
    for (int i = 0; i < length; ++i) {
        printf("%.2x", data[i]);
    }
    printf("\n");
}

int main() {
    if (sodium_init() < 0) {
        printf("Fail to initialize libsodium\n");
        exit(0);
    }

    // client
    unsigned char client_pk[crypto_kx_PUBLICKEYBYTES], client_sk[crypto_kx_SECRETKEYBYTES];
    unsigned char client_rx[crypto_kx_SESSIONKEYBYTES], client_tx[crypto_kx_SESSIONKEYBYTES];
    crypto_kx_keypair(client_pk, client_sk);
    // client

    // server
    unsigned char server_pk[crypto_kx_PUBLICKEYBYTES], server_sk[crypto_kx_SECRETKEYBYTES];
    unsigned char server_rx[crypto_kx_SESSIONKEYBYTES], server_tx[crypto_kx_SESSIONKEYBYTES];
    crypto_kx_keypair(server_pk, server_sk);
    // server

    // client 用rx解密收到的消息，用tx加密发送消息
    if (crypto_kx_client_session_keys(client_rx, client_tx,
                                    client_pk, client_sk, server_pk) != 0) {
        printf("Client: Suspicious server key, abort.\n");
        exit(0);
    }
    // client

    // server 用rx解密收到的消息，用tx加密发送消息
    if (crypto_kx_server_session_keys(server_rx, server_tx,
                                    server_pk, server_sk, client_pk) != 0) {
        printf("Server: Suspicious server key, abort.\n");
        exit(0);
    }
    // server

    printf("Server: use the key for sending:\n");
    PrintHex(server_tx, crypto_kx_SESSIONKEYBYTES);
    printf("Server: use the key for receiving:\n");
    PrintHex(server_rx, crypto_kx_SESSIONKEYBYTES);

    printf("Client: use the key for sending:\n");
    PrintHex(client_tx, crypto_kx_SESSIONKEYBYTES);
    printf("Client: use the key for receiving:\n");
    PrintHex(client_rx, crypto_kx_SESSIONKEYBYTES);

    int RAW_LENGTH = 8192;
    int DATA_LENGTH = 8192 - crypto_secretbox_MACBYTES;
    printf("\nData length: %d, raw stream length: %d\n", DATA_LENGTH, RAW_LENGTH);
    char *data = malloc(DATA_LENGTH);
    strcpy(data, "Hello, I am your client.");
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    unsigned char ciphertext[RAW_LENGTH];
    // 客户端加密
    randombytes_buf(nonce, sizeof nonce);
    crypto_secretbox_easy(ciphertext, (unsigned char *)data, DATA_LENGTH, nonce, client_tx);

    // 服务端解密
    unsigned char *decrypted = malloc(DATA_LENGTH);
    if (crypto_secretbox_open_easy(decrypted, ciphertext, RAW_LENGTH, nonce, server_rx) != 0) {
        printf("Message forged.\n");
    } else {
        printf("Get message: %s\n", (char *)decrypted);
    }
    free(data);
    free(decrypted);
}


#ifdef __cplusplus
}
#endif