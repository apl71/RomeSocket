#include <gtest/gtest.h>
#include "layer.h"
#include "sodium.h"

class LayerTest : public testing::Test {
protected:
    unsigned char key[crypto_kx_SESSIONKEYBYTES];

    void SetUp() override {
        // 生成密钥
        if (sodium_init() < 0) {
            exit(-1);
        }
        unsigned char key[crypto_kx_SESSIONKEYBYTES];
        randombytes_buf(key, crypto_kx_SESSIONKEYBYTES);
        // 生成随机数据
        randombytes_buf(data1, 8192);
        randombytes_buf(data2, 8189);
        randombytes_buf(data3, 1);
        randombytes_buf(data4, 81920);
        randombytes_buf(data5, 100000);
        // 生成公钥
        randombytes_buf(pk, crypto_kx_PUBLICKEYBYTES);
    }

    void BufferTest(Buffer a, Buffer b) {
        ASSERT_EQ(a.length, b.length);
        for (unsigned i = 0; i < a.length; ++i) {
            EXPECT_EQ(a.buffer[i], b.buffer[i]) << "differ at index " << i;
        }
    }

    void HelloTester() {
        // 生成Hello包
        Buffer hello = {new char[BLOCK_LENGTH], BLOCK_LENGTH};
        int result = RomeSocketGetHello(&hello, (unsigned char *)pk);
        ASSERT_EQ(result, 1);
        // 测试hello包
        unsigned char pk_test[crypto_kx_PUBLICKEYBYTES];
        result = RomeSocketCheckHello(hello, pk_test);
        ASSERT_EQ(result, 1);
        // 一致性测试
        BufferTest({pk, crypto_kx_PUBLICKEYBYTES}, {(char *)pk_test, crypto_kx_PUBLICKEYBYTES});
        delete[]hello.buffer;
    }

    void BadHelloTester() {;
        Buffer bad_hellos[7];
        bad_hellos[0] = {nullptr, 0};
        bad_hellos[1] = {nullptr, 20};
        bad_hellos[2] = {new char[100], 100}; // random bad data
        bad_hellos[3] = {new char[20], 20};
        bad_hellos[4] = {new char[50], 50};
        memset(bad_hellos[4].buffer, 0, 50);
        char hello[8] = "RSHELLO";
        bad_hellos[5] = {hello, 8};
        bad_hellos[6] = {new char[30], 30};
        memcpy(bad_hellos[6].buffer, hello, 7);

        for (int i = 0; i < 8; ++i) {
            int result = RomeSocketCheckHello(bad_hellos[i], (unsigned char *)pk);
            EXPECT_EQ(result, 0);
        }

        delete[]bad_hellos[2].buffer;
        delete[]bad_hellos[3].buffer;
        delete[]bad_hellos[4].buffer;
        delete[]bad_hellos[6].buffer;
    }

    void ThoroughTester(char *data, unsigned length) {
        // 测试
        Buffer origin = {data, length};
        Buffer encrypted = RomeSocketEncrypt(origin, key);
        unsigned count = 0;
        Buffer *buffers = RomeSocketSplit(encrypted, &count);
        Buffer concatenated = RomeSocketConcatenate(buffers, count);
        Buffer result = RomeSocketDecrypt(concatenated, key);
        // 检查
        ASSERT_EQ(origin.length, result.length);
        BufferTest(origin, result);
        // 清理资源
        RomeSocketClearBuffer(encrypted);
        RomeSocketClearBuffer(concatenated);
        for (unsigned i = 0; i < count; ++i) {
            delete[]buffers[i].buffer;
        }
        delete[]buffers;
    }

    char data1[8192];
    char data2[8189];
    char data3[1];
    char data4[81920];
    char data5[100000];

    char pk[crypto_kx_PUBLICKEYBYTES];
};

TEST_F(LayerTest, ThoroughTest) {
    ThoroughTester(data1, 8192);
    ThoroughTester(data2, 8189);
    ThoroughTester(data3, 1);
    ThoroughTester(data4, 81920);
    ThoroughTester(data5, 100000);
}

TEST_F(LayerTest, HelloTest) {
    HelloTester();
    BadHelloTester();
}