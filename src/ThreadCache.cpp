#include "../include/ThreadCache.h"
#include "../include/CentralCache.h"

namespace easyMemoryPool {
    void* ThreadCache::allocate(size_t bytes){
        if (bytes > MAX_BYTES) { // 超过256KB，直接调用CentralCache的allocate方法
            return malloc(bytes);
        }
        if (bytes == 0){
            bytes = ALIGNMENT; // 至少分配8字节
        }

        size_t index = SizeClass::getIndex(bytes); // 计算自由链表的索引
        if (void* p = freeList[index] ){
            freeList[index] = *reinterpret_cast<void**>(p);
            freeListSize[index]--; // 自由链表大小减1
            return p; // 返回分配的内存
        }
        else{
            // 从CentralCache获取内存
            return fetchFromCentralCache(index);
        }
    }

    void ThreadCache::deallocate(void* p, size_t bytes){
        if (bytes > MAX_BYTES) { // 超过256KB，直接调用CentralCache的deallocate方法
            return free(p);
        }
        size_t index = SizeClass::getIndex(bytes); // 计算自由链表的索引
        *reinterpret_cast<void**>(p) = freeList[index]; // 将p插入到自由链表的头部
        freeList[index] = p; // 更新自由链表的头部
        freeListSize[index]++; // 自由链表大小加1

        if (shouldReturnToCentralCache(index)){
            returnToCentralCache(freeList[index], bytes);
        }
    }
    void* ThreadCache::fetchFromCentralCache(size_t index){
        size_t size = (index + 1) * ALIGNMENT; // 计算批量获取内存块的大小
        size_t batchNum = getBatchNum(size); // 计算批量获取内存块的数量
        void* start = CentralCache::getInstance()->fetchRange(index, batchNum); // 从CentralCache获取内存
        if (!start) return nullptr;
        void* next = *reinterpret_cast<void**>(start); 
        freeList[index] = next; // 更新自由链表的头部
        *reinterpret_cast<void**>(start) = nullptr; // 将start的下一个指针置为空
        freeListSize[index] = batchNum - 1; // 自由链表大小减1
        return start;
    }
    void ThreadCache::returnToCentralCache(void* p, size_t size){
        size_t index = SizeClass::getIndex(size); // 计算自由链表的索引

        size_t batchNum = freeListSize[index]; // 计算批量获取内存块的数量
        if (batchNum <= 1) return;
        size_t keepNum = std::max(size_t(1), batchNum / 4); 
        size_t returnNum = batchNum - keepNum;
        void* end = p;
        for (size_t i = 0; i < returnNum - 1; i++){ 
            end = *reinterpret_cast<void**>(end); 
        }
        freeList[index] = *reinterpret_cast<void**>(end); 
        freeListSize[index] = keepNum; // 自由链表大小减1
        *reinterpret_cast<void**>(end) = nullptr; // 将end的下一个指针置为空
        CentralCache::getInstance()->returnRange(p, returnNum, index);
        
    }
    size_t ThreadCache::getBatchNum(size_t size){
            // 基准：每次批量获取不超过4KB内存
        constexpr size_t MAX_BATCH_SIZE = 4 * 1024; // 4KB

        // 根据对象大小设置合理的基准批量数
        size_t baseNum;
        if (size <= 32) baseNum = 64;    // 64 * 32 = 2KB
        else if (size <= 64) baseNum = 32;  // 32 * 64 = 2KB
        else if (size <= 128) baseNum = 16; // 16 * 128 = 2KB
        else if (size <= 256) baseNum = 8;  // 8 * 256 = 2KB
        else if (size <= 512) baseNum = 4;  // 4 * 512 = 2KB
        else if (size <= 1024) baseNum = 2; // 2 * 1024 = 2KB
        else baseNum = 1;                   // 大于1024的对象每次只从中心缓存取1个

        // 计算最大批量数
        size_t maxNum = std::max(size_t(1), MAX_BATCH_SIZE / size);

        // 取最小值，但确保至少返回1
        return std::max(size_t(1), std::min(maxNum, baseNum));
    }
    bool ThreadCache::shouldReturnToCentralCache(size_t index){
        // 设定阈值，例如：当自由链表的大小超过一定数量时
        size_t threshold = 64; // 例如，64个内存块
        return (freeListSize[index] > threshold);
    }
}