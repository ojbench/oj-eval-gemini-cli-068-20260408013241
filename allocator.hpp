#ifndef TLSF_ALLOCATOR_HPP
#define TLSF_ALLOCATOR_HPP

#include <cstddef>
#include <memory>
#include <cstdint>
#include <array>

class TLSFAllocator {
public:
    // 构造与析构
    explicit TLSFAllocator(std::size_t memoryPoolSize);
    ~TLSFAllocator();
    
    // 内存分配和释放接口
    void* allocate(std::size_t size);
    void deallocate(void* ptr);
    
    // 获取TLSF内部管理的内存池起始地址
    void* getMemoryPoolStart() const;
    
    // 获取TLSF内部管理的内存池大小
    std::size_t getMemoryPoolSize() const;
    
    // 获取当前可用的最大连续块大小
    std::size_t getMaxAvailableBlockSize() const;
    
    // 禁用拷贝和移动操作
    TLSFAllocator(const TLSFAllocator&) = delete;
    TLSFAllocator& operator=(const TLSFAllocator&) = delete;
    TLSFAllocator(TLSFAllocator&&) = delete;
    TLSFAllocator& operator=(TLSFAllocator&&) = delete;
    
private:
    void* memoryPool; // 维护TLSF的内存池
    std::size_t poolSize; // 内存池的总大小
    
    // TLSF 数据结构定义
    static constexpr int FLI_BITS = 5; // First Level Index 级别位数
    static constexpr int SLI_BITS = 4; // Second Level Index 级别位数
    static constexpr int FLI_SIZE = (1 << FLI_BITS); // 32 个一级索引
    static constexpr int SLI_SIZE = (1 << SLI_BITS); // 16 个二级索引
    
    // memoryPool 被分割成多个块，每个块都有一个BlockHeader结构体储存相关信息
    struct BlockHeader {
        void* data; // 指向数据块头地址的指针
        std::size_t size; // 块大小（包含头部）
        bool isFree;      // 是否空闲
        BlockHeader* prevPhysBlock; // 指向物理上前一个块
        BlockHeader* nextPhysBlock; // 指向物理上后一个块
    };
    
    // 空闲块结构体继承自 BlockHeader
    struct FreeBlock : BlockHeader {
        FreeBlock* prevFree; // 指向空闲链表的前一个块
        FreeBlock* nextFree; // 指向空闲链表的下一个块
    };
    
    struct TLSFIndex {
        std::array<std::array<FreeBlock*, SLI_SIZE>, FLI_SIZE> freeLists; // 二级索引表
        std::uint32_t fliBitmap; // 记录哪些 First-Level 有空闲块
        std::array<std::uint16_t, FLI_SIZE> sliBitmaps; // 记录 Second-Level 空闲块
    };
    
    TLSFIndex index; // TLSF 索引结构
    
    // 初始化TLSF内存管理结构
    // 申请大小为 size 的内存池
    void initializeMemoryPool(std::size_t size);
    
    // 内部操作接口
    void splitBlock(FreeBlock* block, std::size_t size); // 切割块
    void mergeAdjacentFreeBlocks(FreeBlock* block); // 在deallocate时合并物理上的相邻块，减少碎片
    FreeBlock* findSuitableBlock(std::size_t size); // 找到合适的块
    void insertFreeBlock(FreeBlock* block); // 插入空闲块
    void removeFreeBlock(FreeBlock* block); // 移除空闲块
    
    // 计算 First-Level 和 Second-Level 索引
    void mappingFunction(std::size_t size, int& fli, int& sli);
};

#endif // TLSF_ALLOCATOR_HPP
