#ifndef MEMORY_POOL_H_
#define MEMORY_POOL_H_

#include <vector>

// huge block:  BLOCK_SIZE * 1000
// large block: BLOCK_SIZE * 10
// small block: BLOCK_SIZE

struct Segment {
    void *start = nullptr;
    unsigned length = 0;
};

struct MemoryBlock {
    void *s = nullptr;
    std::vector<Segment> segment_table;
};

class MemoryPool {
private:
    std::vector<MemoryBlock> pool;
    unsigned block_count, block_size;

public:
    // 构造一个内存池，包含的huge block, large block和small block分别为hb, lb和sb
    // 默认参数会占用约117MB内存
    MemoryPool(size_t hb = 10, size_t lb = 1000, size_t sb = 10000);
    char *Allocate(unsigned length);
    void Free(char *s);
    ~MemoryPool();
};

#endif