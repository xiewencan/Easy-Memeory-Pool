#pragma once
#include "Common.h"
#include <mutex>

namespace easyMemoryPool {
    class CentralCache{
        public:
        static CentralCache* getInstance(){
            static CentralCache instance;
            return &instance;
        }
        void* fetchRange(size_t index, size_t batchNum);
        void returnRange(void *start, size_t returnNum, size_t index);

        private:
        CentralCache() {
            for (size_t i = 0; i < FREE_LIST_SIZE; ++i) {
                freeList[i].store(nullptr,std::memory_order_relaxed); // 初始化自由链表为空
                lockList[i].clear(); // 初始化锁为未锁定状态
            }
        }
        
        void* fetchFromPageCache(size_t size);

        private:
        std::array<std::atomic<void*>, FREE_LIST_SIZE> freeList; // 自由链表数组，每个元素是一个自由链表的头指针
        std::array<std::atomic_flag, FREE_LIST_SIZE> lockList; // 自由链表数组，每个元素是一个自由链表的锁

    };
}