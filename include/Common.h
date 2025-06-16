#pragma once
#include <cstddef>
#include <atomic>
#include <array>

namespace easyMemoryPool {

    constexpr size_t ALIGNMENT = 8;
    constexpr size_t MAX_BYTES = 256*1024;
    constexpr size_t FREE_LIST_SIZE = MAX_BYTES / ALIGNMENT;

    class SizeClass {
       public:
       // 向上取整到8的倍数
        static size_t roundUp(size_t bytes) { return ((bytes + ALIGNMENT - 1) & ~(ALIGNMENT - 1)); } 
        // 根据大小判断应该使用哪个自由链表
        static size_t getIndex(size_t bytes) {
            bytes = std::max(bytes, ALIGNMENT); // 至少要8字节
            return ((bytes + ALIGNMENT - 1) / ALIGNMENT - 1); // 向上取整到8的倍数
        } 
    };

}