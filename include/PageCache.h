#pragma once
#include "Common.h"
#include <map>
#include <mutex>

namespace easyMemoryPool {
    
    class PageCache {
       public:

       static const size_t Page_Size = 4096; // 页大小，单位为字节
       static PageCache* getInstance(){
           static PageCache instance; // 静态局部变量，只会在第一次调用时初始化一次
           return &instance; 
       }

       void* allocateSpan(size_t numPage);
    //    void deallocateSpan(void* p, size_t numPage){};

       private:
       PageCache(){};
       void* fetchFromSystem(size_t numPage);

       private:
       struct Span
       {
           void*  pageAddr; // 页起始地址
           size_t numPages; // 页数
           Span*  next;     // 链表指针
       };
   
       // 按页数管理空闲span，不同页数对应不同Span链表
       std::map<size_t, Span*> freeSpans;
       // 页号到span的映射，用于回收
       std::map<void*, Span*> spanMap;
       std::mutex mutex;

    };

}