#include "../include/PageCache.h"
#include "../include/CentralCache.h"
#include <cassert>
#include <thread>

namespace easyMemoryPool {
    static const size_t SPAN_PAGES = 8;

    void* CentralCache::fetchRange(size_t index,size_t batchNum){
        if (index >= FREE_LIST_SIZE || batchNum == 0) 
        return nullptr;

        while(lockList[index].test_and_set(std::memory_order_acquire)){ // 尝试获取锁，失败则等待
            std::this_thread::yield(); // 等待其他线程释放锁
        }

        void* result;
        try{
            result = freeList[index].load(std::memory_order_relaxed); // 尝试从自由链表中获取内存块
            if(!result){
                size_t size = (index + 1) * ALIGNMENT; // 计算需要分配的内存大小
                void* new_memory = fetchFromPageCache(size); // 从页缓存中获取内存块
                if(!new_memory){
                    lockList[index].clear(std::memory_order_release); // 释放锁
                    return nullptr;
                }
                char* current_block = static_cast<char*>(new_memory);
                size_t totalBlocks = (PageCache::Page_Size * SPAN_PAGES) / size; // 计算需要分配的内存块数量
                size_t allocBlocks = std::min(totalBlocks, batchNum); // 计算实际分配的内存块数量
                
                // Link allocated blocks
                if (allocBlocks > 0) {
                    for (size_t i = 0; i < allocBlocks - 1; ++i) {
                        *reinterpret_cast<void**>(current_block) = current_block + size;
                        current_block += size;
                    }
                    *reinterpret_cast<void**>(current_block) = nullptr; // Last allocated block points to null
                    
                }
                result = new_memory; // Set result to the start of allocated blocks
                // Link remaining blocks to freeList
                size_t remainBlocks = totalBlocks - allocBlocks;
                if (remainBlocks > 0) {
                    char* remainStart = static_cast<char*>(new_memory) + allocBlocks * size;
                    char* current_remain_block = remainStart;
                    for (size_t i = 0; i < remainBlocks - 1; ++i) {
                        *reinterpret_cast<void**>(current_remain_block) = current_remain_block + size;
                        current_remain_block += size;
                    }
                    *reinterpret_cast<void**>(current_remain_block) = nullptr;
                    freeList[index].store(remainStart, std::memory_order_relaxed);
                } else {
                    freeList[index].store(nullptr, std::memory_order_relaxed);
                }

            }
            else{
                void* cur = static_cast<char*>(result); 
                void* pre = nullptr;
                size_t count = 0;
                while(cur && count < batchNum){
                    pre = cur;
                    cur = *reinterpret_cast<void**>(cur);
                    count++;
                }
                if (count == batchNum){ // 分配成功
                    if (pre){ // 还有剩余的内存块
                        freeList[index].store(cur,std::memory_order_relaxed); // 更新自由链表的头部
                        *reinterpret_cast<void**>(pre) = nullptr; // 将pre的下一个指针置为空
                    } else { // No remaining blocks in the current free list
                        freeList[index].store(nullptr, std::memory_order_relaxed);
                    }
                } else { // Not enough blocks in the current free list
                    lockList[index].clear(std::memory_order_release); // 释放锁
                    return nullptr; // Indicate failure to allocate enough blocks
                }
            }

        }
        catch(...){ // 捕获异常，释放锁
            lockList[index].clear(std::memory_order_release); // 释放锁
            throw; // 重新抛出异常
        }
        lockList[index].clear(std::memory_order_release); // 释放锁
        return result;
    }
    void CentralCache::returnRange(void *start, size_t returnNum, size_t index){
        if (index >= FREE_LIST_SIZE || returnNum == 0) return;

        while(lockList[index].test_and_set(std::memory_order_acquire)){ // 尝试获取锁，失败则等待
            std::this_thread::yield(); // 等待其他线程释放锁
        }

        try{
            void* cur = start;
            // Traverse to the end of the returned list
            for (size_t i = 0; i < returnNum - 1; ++i) {
                if (!cur) break; // Avoid dereferencing nullptr
                cur = *reinterpret_cast<void**>(cur);
            }
            
            // Link the returned list to the existing free list
            if (cur) { // Ensure cur is not nullptr before dereferencing
                *reinterpret_cast<void**>(cur) = freeList[index].load(std::memory_order_relaxed);
            }
            freeList[index].store(start,std::memory_order_relaxed); // 更新自由链表的头部

        }
        catch (...) 
        {
            lockList[index].clear(std::memory_order_release);
            throw;
        }
    
        lockList[index].clear(std::memory_order_release);
    }
    void* CentralCache::fetchFromPageCache(size_t size){
        // 1. 计算实际需要的页数
        size_t numPages = (size + PageCache::Page_Size - 1) / PageCache::Page_Size;

        // 2. 根据大小决定分配策略
        if (size <= SPAN_PAGES * PageCache::Page_Size) 
        {
            // 小于等于32KB的请求，使用固定8页
            return PageCache::getInstance()->allocateSpan(SPAN_PAGES);
        } 
        else 
        {
            // 大于32KB的请求，按实际需求分配
            return PageCache::getInstance()->allocateSpan(numPages);
        }
    }



}