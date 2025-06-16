#include "../include/PageCache.h"
#include <sys/mman.h>
#include <cstring>

namespace easyMemoryPool{

    void* PageCache::allocateSpan(size_t numPage){
        std::lock_guard<std::mutex> lock(mutex); // 加锁保护

        auto it = freeSpans.lower_bound(numPage); // 查找第一个大于等于numPage的Span链表
        if (it != freeSpans.end()){
            Span* span = it->second;
            if (span->next){
                freeSpans[span->numPages] = span->next; // 更新Span链表
                span->next = nullptr; // 断开与下一个Span的连接
            }else{
                freeSpans.erase(it); 
            }

            if (span->numPages  > numPage){
                Span* newSpan = new Span;
                newSpan->pageAddr = static_cast<char*>(span->pageAddr) + numPage * Page_Size; 
                newSpan->numPages = span->numPages - numPage;
                newSpan->next = nullptr;
                span->numPages = numPage;
                
                auto &node = freeSpans[newSpan->numPages]; 
                newSpan->next = node;
                node = newSpan;
            }
            spanMap[span->pageAddr] = span; // 更新spanMap
            return span->pageAddr; // 返回分配的内存块
        }else{
            void* memory = fetchFromSystem(numPage); 
            if (memory){
                Span* span = new Span; // 创建新的Span
                span->pageAddr = memory; // 设置Span的页起始地址
                span->numPages = numPage; // 设置Span的页数
                span->next = nullptr;
                spanMap[memory] = span;
                return memory;
            }else{
                return nullptr;
            }
        }

    }
    void* PageCache::fetchFromSystem(size_t numPage){
        void* memory = mmap(nullptr, numPage * Page_Size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); // 从系统中分配内存
        if (memory == MAP_FAILED){ 
            return nullptr; 
        }else{ 
            return memory; 
        }
    }
}