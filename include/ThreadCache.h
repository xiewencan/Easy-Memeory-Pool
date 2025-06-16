#pragma once
#include "Common.h"

namespace easyMemoryPool {

    class ThreadCache{
        public:
        static ThreadCache* getInstance(){
            static thread_local ThreadCache instance;
            return &instance;
        }
        void* allocate(size_t bytes);
        void deallocate(void* p, size_t bytes);

        private:
        ThreadCache() = default;

        void* fetchFromCentralCache(size_t index);
        void returnToCentralCache(void* start, size_t size);
        // 计算批量获取内存块的数量
        size_t getBatchNum(size_t size);
        // 判断是否需要归还内存给中心缓存
        bool shouldReturnToCentralCache(size_t index);


        private:
        std::array<void*, FREE_LIST_SIZE> freeList; // 自由链表数组，每个元素是一个自由链表的头指针
        std::array<size_t, FREE_LIST_SIZE> freeListSize; // 自由链表数组，每个元素是一个自由链表的大小
    };
}