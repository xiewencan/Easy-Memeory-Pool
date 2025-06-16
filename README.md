# EASY MEMORY POOL
## 结构
内存池的架构如图所示，由ThreadCache,CentralCache,PageCache三层组成。

![0u1ao3wf.ags.png](https://spade-photos.oss-cn-beijing.aliyuncs.com/202506161603450.png)
### ThreadCache
```
	std::array<void*, FREE_LIST_SIZE> freeList_;    
    std::array<size_t, FREE_LIST_SIZE> freeListSize_;
```
+ 侵入式链表
+ 单例模式
+ `freeList_`的结构
![[../Excalidraw/ThreadCache.excalidraw]]
+ `freeListSize_`:存储每个自由链表的长度
### CentralCache
```
    // 中心缓存的自由链表
    // 经测试，void*与std::atomic<void*>效果相同
    std::array<std::atomic<void*>, FREE_LIST_SIZE> centralFreeList_;
    // 用于同步的自旋锁
    std::array<std::atomic_flag, FREE_LIST_SIZE> locks_;
```
+ `centralFreeList_`：空闲页面，结构与`freeList_`相同
+ `lock_`：自旋锁数组，给每个大小的空闲页添加自旋锁，只有获取了锁才能访问自由链表
### PageCache
```
    // 按页数管理空闲span，不同页数对应不同Span链表
    std::map<size_t, Span*> freeSpans_;
    // 页号到span的映射，用于回收
    std::map<void*, Span*> spanMap_;
    // 全局锁
    std::mutex mutex_;
```
+ `freeSpans_`：不同大小空闲页面的哈希表，有序，用来分配页面，与`freeList_`有点类似，不过由于页面数量的种类不会太多，采用`map`更节约，且大小不会随意改变
+ `spanMap_`： 用来记录所有从系统申请的内存的地址和大小，包含空闲与占用，用于回收
+ `mutex_`：全局锁

#### allocate 流程：
1. `EasyMemoryPool::allocate(bytes) `
   - 这是用户调用的入口点。它会获取当前线程的 `ThreadCache` 实例。
   - 调用 `ThreadCache::getInstance()->allocate(bytes)` 。

2.` ThreadCache::allocate(bytes)` 
   - 根据请求的 `bytes` 大小，通过 `SizeClass::getIndex(bytes)` 计算出对应的自由链表索引。
   - 尝试从当前线程的` freeList[index] `中获取一个内存块。
   - 如果自由链表中有可用内存块 ：直接返回该内存块。
   - 如果自由链表为空 ：
     - 调用 `ThreadCache::fetchFromCentralCache(index, num)` 从 `CentralCache` 获取一批内存块。
     - num 是根据当前自由链表大小动态计算的批量数量。
     - 将获取到的内存块中的第一个返回给用户，其余的添加到` ThreadCache` 的对应自由链表中。

3. `ThreadCache::fetchFromCentralCache(index, num)` 
   - 调用 `CentralCache::getInstance()->fetchRange(index, num) `从` CentralCache` 获取内存块。
   - `CentralCache` 会返回一个内存块链表的头部。
   - 将这些内存块添加到 `ThreadCache` 的自由链表中。

4. `CentralCache::fetchRange(index, num)`
   - 根据 `index` 找到 `CentralCache` 中对应的自由链表 `freeList[index] `。
   - 如果 `freeList[index]` 中有足够的内存块 ：
     - 从链表中取出 `num `个内存块，并返回它们的头部和尾部。
   - 如果 `freeList[index]` 中内存块不足或为空 ：
     - 调用 `CentralCache::fetchFromPageCache(size)` 从 PageCache 获取一个 Span （大块连续内存）。
     - Span 会被切分成多个小内存块，并链接成一个自由链表。
     - 将这些小内存块添加到的 `freeList[index]` 中。
     - 然后从 `freeList[index]` 中取出 num 个内存块并返回。

5. ` CentralCache::fetchFromPageCache(size)` 
   - 根据 index 计算出需要从 PageCache 分配的页数。
   - 调用 `PageCache::getInstance()->allocateSpan(numPages)` 从 PageCache 获取至少一个Span 。
   - 将分配的内存按 index 对应的大小进行切分，并构建成一个自由链表返回。

6. ` PageCache::allocateSpan(numPages)` 
   
   - 尝试从` PageCache` 内部的 `freeSpans` 中查找一个大小合适的 `Span` 。
   - 如果找到 ：
     - 如果找到的 `Span`的 `pageSizse` 大于请求的 `numPages` ，则将其分割，一部分返回，另一部分放回 freeSpans 。
     - 返回找到的 Span 。
   - 如果未找到 ：
     - 调用 `PageCache::fetchFromSystem(numPages) `直接向操作系统申请内存（通常通过 mmap ）。
     - 将获取到的内存封装成 Span 返回。

### deallocate 流程

1. `EasyMemoryPool::deallocate(p, bytes)` : 
   - 这是用户调用的入口点。它会获取当前线程的 ThreadCache 实例。
   - 调用 `ThreadCache::getInstance()->deallocate(p, bytes)` 。

2. `ThreadCache::deallocate(p, bytes)` 
   - 根据 bytes 大小，通过` SizeClass::getIndex(bytes)` 计算出对应的自由链表索引。
   - 将内存块 p 添加到 `ThreadCache `的 `freeList[index]` 中。
   - 如果 ThreadCache 的 freeList[index] 中的内存块数量达到阈值 （例如，超过一个批次的大小）：
     - 调用 `ThreadCache::returnToCentralCache(index, num) `将一批内存块返回给 `CentralCache` 。

3. `ThreadCache::returnToCentralCache(index, num) `
   - 从 `ThreadCache 的 freeList[index] `中取出 num 个内存块。
   - 调用 `CentralCache::getInstance()->returnRange(p, num, index) `将这些内存块返回给 `CentralCache` 。

4. `CentralCache::returnRange(p, num, index)` 
   - 将返回的内存块 p （以及后续的 num-1 个块）添加到 CentralCache 的 `freeList[index]` 中。
   - 如果` CentralCache` 的 `freeList[index]` 中的内存块数量达到阈值 （例如，一个 Span 的大小）：
     - 调用 `CentralCache::releaseSpanToPageCache(index)` 将一个完整的 Span 返回给 `PageCache` 。
***
下面流程待实现：
4. ` CentralCache::releaseSpanToPageCache(index)` :
   - 从 CentralCache 的 freeList[index] 中找到一个完整的 Span 所包含的所有内存块。
   - 将这些内存块从 freeList[index] 中移除。
   - 调用 PageCache::getInstance()->deallocateSpan(span) 将 Span 返回给 PageCache 。

6. ` PageCache::deallocateSpan(span)` :
   - 将 Span 添加到 PageCache 的 freeSpans 中。
   - 尝试与相邻的空闲 Span 进行合并，形成更大的空闲 Span 。
   - 如果合并后的 Span 达到一定大小（例如，非常大），可能会考虑将其归还给操作系统（通过 munmap ）。
