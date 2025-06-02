#pragma once
#include "ThreadCache.h"
namespace easyMemoryPool{

    class EasyMemoryPool {
        public:
        // 分配内存
        static void* allocate(size_t bytes){
            return ThreadCache::getInstance()->allocate(bytes);
        }
        // 回收内存
        static void deallocate(void* p, size_t bytes){
            ThreadCache::getInstance()->deallocate(p, bytes);
        }
    };
}
