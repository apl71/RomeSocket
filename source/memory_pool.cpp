#include "memory_pool.hpp"

MemoryPool::MemoryPool(unsigned blocks, unsigned _block_size) {
    block_size = _block_size, block_count = blocks;
    // 初始化内存池
    pool.resize(blocks);
    for (int i = 0; i < blocks; ++i) {
        pool[i].s = new char[block_size];
        pool[i].segment_table.push_back({pool[i].s, block_size});
    }
}

MemoryPool::~MemoryPool() {
    for (auto &i : pool) {
        delete[]i.s;
        i.s = nullptr;
    }
}

char *MemoryPool::Allocate(unsigned length) {
    // 申请的内存分为三种：超大块（1）、大块（2）、小块（3）
    // 超过一块内存的大小称为超大块，不接受该类分配
    // 不到一块内存1/2的称为小块，从内存池尾部的块开始分配
    // 其他称为大块，从内存池头部开始分配
    // 块内均采用首次适应算法分配
    if (length > block_size) {
        return nullptr;
    } else if (length < block_size / 2) {
        
    }
}