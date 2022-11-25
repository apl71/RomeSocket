#include <gtest/gtest.h>
#include "layer.h"
#include "sodium.h"

class LayerTest : public testing::Test {
protected:
    unsigned char key[crypto_kx_SESSIONKEYBYTES];

    void SetUp() override {
        // 生成密钥
        sodium_init();
        unsigned char key[crypto_kx_SESSIONKEYBYTES];
        randombytes_buf(key, crypto_kx_SESSIONKEYBYTES);
        // 生成随机数据
        randombytes_buf(data1, 8192);
        randombytes_buf(data2, 8189);
        randombytes_buf(data3, 1);
        randombytes_buf(data4, 81920);
        randombytes_buf(data5, 100000);
    }

    void BufferTest(Buffer a, Buffer b) {
        ASSERT_EQ(a.length, b.length);
        for (unsigned i = 0; i < a.length; ++i) {
            EXPECT_EQ(a.buffer[i], b.buffer[i]) << "differ at index " << i;
        }
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
        for (unsigned i = 0; i < origin.length; ++i) {
            EXPECT_EQ(origin.buffer[i], result.buffer[i]) << "differ at index " << i;
        }
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
};

TEST_F(LayerTest, ThoroughTest) {
    ThoroughTester(data1, 8192);
    ThoroughTester(data2, 8189);
    ThoroughTester(data3, 1);
    ThoroughTester(data4, 81920);
    ThoroughTester(data5, 100000);
}