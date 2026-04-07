#include "allocator.hpp"
#include <cstdlib>
#include <algorithm>
#include <iostream>

#ifdef _MSC_VER
#include <intrin.h>
#endif

static inline int msb(std::size_t size) {
    if (size == 0) return -1;
#if defined(__GNUC__) || defined(__clang__)
    return 63 - __builtin_clzll(size);
#elif defined(_MSC_VER)
    unsigned long index;
    _BitScanReverse64(&index, size);
    return index;
#else
    int r = 0;
    while (size >>= 1) r++;
    return r;
#endif
}

static inline int lsb(std::uint32_t v) {
    if (v == 0) return -1;
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_ctz(v);
#elif defined(_MSC_VER)
    unsigned long index;
    _BitScanForward(&index, v);
    return index;
#else
    int r = 0;
    while ((v & 1) == 0) { v >>= 1; r++; }
    return r;
#endif
}

TLSFAllocator::TLSFAllocator(std::size_t memoryPoolSize) {
    initializeMemoryPool(memoryPoolSize);
}

TLSFAllocator::~TLSFAllocator() {
    std::free(memoryPool);
}

void TLSFAllocator::initializeMemoryPool(std::size_t size) {
    poolSize = size;
    memoryPool = std::malloc(size);
    
    index.fliBitmap = 0;
    for (int i = 0; i < FLI_SIZE; ++i) {
        index.sliBitmaps[i] = 0;
        for (int j = 0; j < SLI_SIZE; ++j) {
            index.freeLists[i][j] = nullptr;
        }
    }
    
    if (size > sizeof(FreeBlock)) {
        FreeBlock* initialBlock = static_cast<FreeBlock*>(memoryPool);
        initialBlock->data = reinterpret_cast<char*>(initialBlock) + sizeof(BlockHeader);
        initialBlock->size = size;
        initialBlock->isFree = true;
        initialBlock->prevPhysBlock = nullptr;
        initialBlock->nextPhysBlock = nullptr;
        initialBlock->prevFree = nullptr;
        initialBlock->nextFree = nullptr;
        
        insertFreeBlock(initialBlock);
    }
}

void TLSFAllocator::mappingFunction(std::size_t size, int& fli, int& sli) {
    fli = msb(size);
    if (fli < 0) {
        fli = 0;
        sli = 0;
        return;
    }
    if (fli >= FLI_SIZE) {
        fli = FLI_SIZE - 1;
    }
    
    std::size_t divisions = std::min<std::size_t>(1ULL << fli, SLI_SIZE);
    std::size_t step = (1ULL << fli) / divisions;
    if (step == 0) {
        sli = 0;
    } else {
        sli = (size - (1ULL << fli)) / step;
    }
    if (sli >= SLI_SIZE) sli = SLI_SIZE - 1;
}

void TLSFAllocator::insertFreeBlock(FreeBlock* block) {
    int fli, sli;
    mappingFunction(block->size, fli, sli);
    
    block->isFree = true;
    block->prevFree = nullptr;
    block->nextFree = index.freeLists[fli][sli];
    
    if (index.freeLists[fli][sli]) {
        index.freeLists[fli][sli]->prevFree = block;
    }
    index.freeLists[fli][sli] = block;
    
    index.fliBitmap |= (1U << fli);
    index.sliBitmaps[fli] |= (1U << sli);
}

void TLSFAllocator::removeFreeBlock(FreeBlock* block) {
    int fli, sli;
    mappingFunction(block->size, fli, sli);
    
    if (block->prevFree) {
        block->prevFree->nextFree = block->nextFree;
    } else {
        index.freeLists[fli][sli] = block->nextFree;
    }
    
    if (block->nextFree) {
        block->nextFree->prevFree = block->prevFree;
    }
    
    if (!index.freeLists[fli][sli]) {
        index.sliBitmaps[fli] &= ~(1U << sli);
        if (!index.sliBitmaps[fli]) {
            index.fliBitmap &= ~(1U << fli);
        }
    }
}

TLSFAllocator::FreeBlock* TLSFAllocator::findSuitableBlock(std::size_t size) {
    int fli, sli;
    mappingFunction(size, fli, sli);
    
    std::uint32_t slMap = index.sliBitmaps[fli] & (~0U << sli);
    if (slMap) {
        int sl = lsb(slMap);
        return index.freeLists[fli][sl];
    }
    
    std::uint32_t flMap = index.fliBitmap & (~0U << (fli + 1));
    if (flMap) {
        int fl = lsb(flMap);
        slMap = index.sliBitmaps[fl];
        int sl = lsb(slMap);
        return index.freeLists[fl][sl];
    }
    
    return nullptr;
}

void TLSFAllocator::splitBlock(FreeBlock* block, std::size_t size) {
    if (block->size >= size + sizeof(FreeBlock)) {
        std::size_t remainingSize = block->size - size;
        
        FreeBlock* newBlock = reinterpret_cast<FreeBlock*>(reinterpret_cast<char*>(block) + size);
        newBlock->data = reinterpret_cast<char*>(newBlock) + sizeof(BlockHeader);
        newBlock->size = remainingSize;
        newBlock->isFree = true;
        
        newBlock->prevPhysBlock = block;
        newBlock->nextPhysBlock = block->nextPhysBlock;
        if (newBlock->nextPhysBlock) {
            newBlock->nextPhysBlock->prevPhysBlock = newBlock;
        }
        
        block->size = size;
        block->nextPhysBlock = newBlock;
        
        insertFreeBlock(newBlock);
    }
}

void* TLSFAllocator::allocate(std::size_t size) {
    std::size_t totalSize = size + sizeof(BlockHeader);
    if (totalSize < sizeof(FreeBlock)) {
        totalSize = sizeof(FreeBlock);
    }
    
    FreeBlock* block = findSuitableBlock(totalSize);
    if (!block) {
        return nullptr;
    }
    
    removeFreeBlock(block);
    splitBlock(block, totalSize);
    block->isFree = false;
    
    return block->data;
}

void TLSFAllocator::mergeAdjacentFreeBlocks(FreeBlock* block) {
    if (block->nextPhysBlock && block->nextPhysBlock->isFree) {
        FreeBlock* nextBlock = static_cast<FreeBlock*>(block->nextPhysBlock);
        removeFreeBlock(nextBlock);
        
        block->size += nextBlock->size;
        block->nextPhysBlock = nextBlock->nextPhysBlock;
        if (block->nextPhysBlock) {
            block->nextPhysBlock->prevPhysBlock = block;
        }
    }
    
    if (block->prevPhysBlock && block->prevPhysBlock->isFree) {
        FreeBlock* prevBlock = static_cast<FreeBlock*>(block->prevPhysBlock);
        removeFreeBlock(prevBlock);
        
        prevBlock->size += block->size;
        prevBlock->nextPhysBlock = block->nextPhysBlock;
        if (prevBlock->nextPhysBlock) {
            prevBlock->nextPhysBlock->prevPhysBlock = prevBlock;
        }
        block = prevBlock;
    }
    
    insertFreeBlock(block);
}

void TLSFAllocator::deallocate(void* ptr) {
    if (!ptr) return;
    
    BlockHeader* header = reinterpret_cast<BlockHeader*>(reinterpret_cast<char*>(ptr) - sizeof(BlockHeader));
    FreeBlock* block = static_cast<FreeBlock*>(header);
    
    block->isFree = true;
    mergeAdjacentFreeBlocks(block);
}

void* TLSFAllocator::getMemoryPoolStart() const {
    return memoryPool;
}

std::size_t TLSFAllocator::getMemoryPoolSize() const {
    return poolSize;
}

std::size_t TLSFAllocator::getMaxAvailableBlockSize() const {
    std::size_t maxSize = 0;
    for (int i = FLI_SIZE - 1; i >= 0; --i) {
        if (index.fliBitmap & (1U << i)) {
            for (int j = SLI_SIZE - 1; j >= 0; --j) {
                if (index.sliBitmaps[i] & (1U << j)) {
                    FreeBlock* block = index.freeLists[i][j];
                    while (block) {
                        if (block->size > maxSize) {
                            maxSize = block->size;
                        }
                        block = block->nextFree;
                    }
                }
            }
            if (maxSize > 0) return maxSize;
        }
    }
    return maxSize;
}
