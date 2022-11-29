#ifndef MEMORY_POOL_H_
#define MEMORY_POOL_H_

#include <vector>
#include <cstdint>
#include <cstring>
#include <iostream>

constexpr uint64_t HUGE_BLOCK_SIZE  = 8192 * 1000;
constexpr uint64_t LARGE_BLOCK_SIZE = 8192 * 10;
constexpr uint64_t SMALL_BLOCK_SIZE = 8192 * 1;
// large block: BLOCK_SIZE * 10
// small block: BLOCK_SIZE

struct Segment {
    void *start = nullptr;
    unsigned length = 0;
};

enum class BLOCK_TYPE {
    HUGE,
    LARGE,
    SMALL
};

struct MemoryBlock {
    BLOCK_TYPE type;
    void *s = nullptr;
    std::vector<Segment> segment_table;
};

enum class POLICY {
    STRICT_PREALLOCATE_MODE,   // 严格的预分配模式，当预分配的内存不足时，将会报错，而非向系统申请新内存
    PREALLOCATE_MODE,          // 预分配模式，当预分配的内存不足时，将会尝试向OS申请内存
    DIRECT_MODE                // 不使用预分配模式，直接向操作系统申请内存
};

template<POLICY policy>
class MemoryPool {
private:
    std::vector<MemoryBlock> pool;
    unsigned block_count, block_size;
    uint64_t used_space;

public:
    // 构造一个内存池，包含的huge block, large block和small block分别为hb, lb和sb
    // 默认参数会占用约117MB内存
    MemoryPool(size_t hb = 10, size_t lb = 1000, size_t sb = 10000);
    
    void *Allocate(unsigned length);
    void Free(char *s);

    uint64_t AvailableSpace();
    uint64_t UsedSpace();

    void Dump(const std::string &dump_file);
    
    ~MemoryPool();
};

// -------------------------- 实现 --------------------------

template<POLICY policy>
MemoryPool<policy>::MemoryPool(size_t hb, size_t lb, size_t sb) {
    for (size_t i = 0; i < hb + lb + sb; ++i) {
        MemoryBlock block;
        uint64_t size = 0;
        if (i < hb) {
            block.type = BLOCK_TYPE::HUGE;
            size = HUGE_BLOCK_SIZE;
        } else if (i < hb + lb) {
            block.type = BLOCK_TYPE::LARGE;
            size = LARGE_BLOCK_SIZE;
        } else {
            block.type = BLOCK_TYPE::SMALL;
            size = SMALL_BLOCK_SIZE;
        }
        try {
            block.s = new char[size];
        } catch (std::exception &e) {
            std::cerr << e.what() << std::endl;
            exit(-1);
        }
        memset(block.s, 0, size);
        pool.push_back(block);
    }
}

template<POLICY policy>
MemoryPool<policy>::~MemoryPool() {
    for (auto block : pool) {
        delete[]block.s;
    }
}

#endif