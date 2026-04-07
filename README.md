# Problem 068 - 重建Allocator

**ACMOJ Problem ID**: 2677

## Table of Contents

- [Problem 068 - 重建Allocator](#problem-068-重建allocator)
  - [Table of Contents](#table-of-contents)
  - [Introduction](#introduction)
    - [Background](#background)
  - [Assignment Description](#assignment-description)
    - [Grade Composition](#grade-composition)
  - [Assignment Requirements](#assignment-requirements)
    - [Input Format](#input-format)
    - [Output Format](#output-format)
    - [Samples](#samples)
    - [Data Constraints](#data-constraints)
  - [Per-Testcase Resource Limits](#per-testcase-resource-limits)
  - [Test Data](#test-data)
  - [Submission Requirements](#submission-requirements)
    - [OJ Git Repository Compilation Process](#oj-git-repository-compilation-process)
    - [Git Configuration Requirements](#git-configuration-requirements)
    - [Submission Guidelines](#submission-guidelines)
    - [Evaluation Notes](#evaluation-notes)
    - [Academic Integrity](#academic-integrity)

## Introduction

### Background

Implement TLSF (Two-Level Segregated Fit) memory allocator with O(1) alloc/free. Bitmap indexing.

## Assignment Description

### Grade Composition

| Grading Component | Percentage |
| :--: | :--: |
| Pass **2677. 重建Allocator** | 80% |
| Code Review | 20% |

Here are several points that need clarification:

- In the Code Review, we will **strictly examine your code style and repository organization structure, etc.**

- This assignment provides some sample data for testing, stored in the `/workspace/data/068/data_test/` directory. Note that these are not the test cases on the Online Judge. Passing all local test cases does not guarantee that you will pass the OJ tests.

- Besides the provided sample data, we also encourage you to design your own test data based on your program logic to assist debugging.

## Assignment Requirements

### Problem Description

## 题意描述

一觉醒来，你被机惨了。具体而言，你的allocator库被删的一干二净，而现在你正面对一道考察内存分配的题目。于是你决定自己实现一个allocator库。

你发现自己还没有学过系统，于是你参考自己电脑上linux内核的allocator库，发现其中有一个基于Segregated Free Lists实现的内存池。于是你决定通过发明TLSF来改进这个算法，并实现一个Allocator类来模拟这个过程。


## 核心思想

**Two-Level Segregated Fit（TLSF）** 是一种高效的动态内存分配算法，旨在提供 **O(1)** 复杂度的分配和释放，同时最大程度地减少内存碎片。这使得 TLSF 特别适用于实时系统，因为其内存管理操作具有可预测的执行时间。

TLSF 结合了 **分级分离存储（Segregated Free Lists）** 和 **位图索引（Bitmap Indexing）** 的思想。其核心原理包括：

1. **第一层索引（First Level）**  
  采用 **指数分割（Exponential Segregation）**，将不同数量级的内存块分组。

2. **第二层索引（Second Level）**  
  采用 **线性分割（Linear Segregation）**，进一步细分同一数量级内的内存块。

3. **位图索引（Bitmaps）**  
  记录每个层级的空闲块，使得分配操作能够在 **O(1)** 时间内找到合适的块。

## 数据结构

1. **First-Level Index（FLI，第一层索引）**

- 用于将不同数量级的内存块分组。
- **计算方式**：`FLI = ⌊log2(size)⌋`  
  例如：大小为 1000B 的块，其 `FLI = ⌊log2(1000)⌋ = 9`，意味着它落在 `2^9 = 512B ~ 1024B` 这一组。

2. **Second-Level Index（SLI，第二层索引）**

- 在 First-Level 确定的范围内，进一步对块大小进行细分。
- **计算方式**：  
  `SLI = ((size - (1 << FLI)) / ((1 << FLI) / divisions))`，其中 `divisions` 由 `SLI_BITS` 确定。  
  例如：如果 `SLI_BITS = 4`，则 `divisions = 1 << 4 = 16` 个槽，每个槽管理的大小更精细。
  **hint**： divisions = min((1 << FLI), 16)

3. **位图（Bitmaps）**

- 维护 First-Level 和 Second-Level 的空闲块信息，使得分配和释放操作可以快速定位合适的块。
- 例如：  
  - **FL Bitmap**：记录 First-Level 中哪些索引包含空闲块。  
  - **SL Bitmap**：记录 Second-Level 细分中的具体可用块。

4. **自由块列表（FreeBlock List）**

- 每个 Second-Level 索引存储一个 **双向链表**，链接所有可用的空闲块。
- 释放时会合并相邻的空闲块，以减少碎片。

---

## TLSF 主要操作

1. **内存分配 (`allocate(size)`)**

- **步骤**：
  1. 计算**满足所在分区最小块**的 FLI 和 SLI，并找到空闲块，不要求best match，任意一个即可。
  2. 如果对应分区没有FreeBlock（通过查询位图），检查更大的组，找到最近的非空分区，任意取一个块。
  3. 若找到合适块：
    - 分割大块（若所需块较小）。
    - 更新 **位图** 和 **链表** 结构。
    - 返回分配的内存块指针。

2. **内存释放 (`deallocate(ptr)`)**

- **步骤**：
  1. 将块插回相应的 Second-Level 链表。
  2. 尝试合并相邻空闲块，减少碎片。
  3. 更新位图，如果某个 Second-Level 变空，则清除相应的位。

---

## 时间复杂度

- **分配 (`allocate(size)`)**：`O(1)`
- **释放 (`deallocate(ptr)`)**：`O(1)`
- **合并（Coalesce）**：`O(1)`

由于使用 **位运算** 和 **索引表**，TLSF 可以在 **常数时间 O(1)** 内完成分配与释放，并且大幅减少碎片，提高了实时性。


```c++
// allocator.hpp
// 该头文件定义了一个基于TLSF算法的C++内存分配器接口
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
        // 构成一个位图，表示每个 Second-Level 是否有空闲块
        // 例如：`sliBitmaps[0]` 表示第一个 First-Level 中的所有 Second-Level 的空闲状态，以二进制形式存储
        // 例如：`sliBitmaps[0] = 0b00001111` 表示第一个 First-Level 中的前四个 Second-Level 块是空闲的
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

```

### Input Format

See the problem description above.

### Output Format

See the problem description above.

### Samples

No sample data provided for this problem.

### Data Constraints

See the problem description for constraints.

## Per-Testcase Resource Limits

- **Time Limit (per test case)**: 1000 ms
- **Memory Limit (per test case)**: 512 MiB
- **Disk Usage**: No disk usage is permitted.

## Test Data

The test data for this problem is located at `/workspace/data/068/data_test/`.

## Submission Requirements

### OJ Git Repository Compilation Process

For Git compilation, we will first clone the repository using a command similar to:
```bash
git clone <repo_url> . --depth 1 --recurse-submodules --shallow-submodules --no-local
```

Then we check if there is a `CMakeLists.txt` file. If it exists, we run (if not, a warning message will be displayed):
```bash
cmake .
```

Finally, we check if there is any of `GNUmakefile`/`makefile`/`Makefile` (if cmake was run previously, this will be the generated Makefile). If it exists, we run (if not, a warning message will be displayed):
```bash
make
```

After this process is complete, we will use the `code` file in the project root directory as the compilation result.

A `CMakeLists.txt` file is provided in the project. You can use or modify it as needed. The local environment has gcc-13 and g++-13 available.

### Git Configuration Requirements

**IMPORTANT**: You must create a `.gitignore` file in your project root directory to avoid OJ evaluation conflicts.

The `.gitignore` file should include at least the following entries:

```gitignore
CMakeFiles/
CMakeCache.txt
```

### Submission Guidelines

- The submitted code must be able to compile successfully through the above compilation process
- The compiled executable file name must be `code`
- The program needs to be able to read data from standard input and write results to standard output
- Please ensure the code runs correctly within the given time and space limits
- **You must use C++ or C language** to implement this assignment

### Evaluation Notes

- The evaluation system will test your program using the provided test data
- The program output must exactly match the expected output (including format)
- Exceeding time or memory limits will be judged as the corresponding error type
- Please pay attention to the overall time performance of your code and the time complexity of each part of your algorithm.

### Academic Integrity

If any violations are found during evaluation or code review (including but not limited to using unconventional methods to pass test cases), your final score may be significantly reduced or become **0 points**.
