#ifndef MEMORY_POOL_H_
#define MEMORY_POOL_H_

#include <vector>

struct SegmentTable {
    char *start = nullptr;
    unsigned length = 0;
};

struct MemoryBlock {
    char *s = nullptr;
    std::vector<SegmentTable> segment_table;
};

class MemoryPool {
private:
    std::vector<MemoryBlock> pool;
    unsigned block_count, block_size;

public:
    // 构造一个块大小为block_size、共blocks块的内存池
    MemoryPool(unsigned blocks, unsigned _block_size);
    char *Allocate(unsigned length);
    void Free(char *s);
    ~MemoryPool();
};

#endif