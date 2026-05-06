#include "PageCache.h"
#include <windows.h>
#include <cstring>

namespace Kama_memoryPool
{

void* PageCache::allocateSpan(size_t numPages)
{
    EnterCriticalSection(&cs_);

    // 查找合适的空闲span
    auto it = freeSpans_.lower_bound(numPages);
    if (it != freeSpans_.end())
    {
        Span* span = it->second;

        if (span->next)
        {
            freeSpans_[it->first] = span->next;
        }
        else
        {
            freeSpans_.erase(it);
        }

        if (span->numPages > numPages)
        {
            Span* newSpan = new Span;
            newSpan->pageAddr = static_cast<char*>(span->pageAddr) +
                                numPages * PAGE_SIZE;
            newSpan->numPages = span->numPages - numPages;
            newSpan->next = nullptr;

            auto& list = freeSpans_[newSpan->numPages];
            newSpan->next = list;
            list = newSpan;

            span->numPages = numPages;
        }

        spanMap_[span->pageAddr] = span;
        LeaveCriticalSection(&cs_);
        return span->pageAddr;
    }

    // 没有合适的span，向系统申请
    void* memory = systemAlloc(numPages);
    if (!memory) {
        LeaveCriticalSection(&cs_);
        return nullptr;
    }

    Span* span = new Span;
    span->pageAddr = memory;
    span->numPages = numPages;
    span->next = nullptr;

    spanMap_[memory] = span;
    LeaveCriticalSection(&cs_);
    return memory;
}

void PageCache::deallocateSpan(void* ptr, size_t numPages)
{
    EnterCriticalSection(&cs_);

    auto it = spanMap_.find(ptr);
    if (it == spanMap_.end()) {
        LeaveCriticalSection(&cs_);
        return;
    }

    Span* span = it->second;

    // 尝试合并相邻的span
    void* nextAddr = static_cast<char*>(ptr) + numPages * PAGE_SIZE;
    auto nextIt = spanMap_.find(nextAddr);

    if (nextIt != spanMap_.end())
    {
        Span* nextSpan = nextIt->second;
        bool found = false;
        auto& nextList = freeSpans_[nextSpan->numPages];

        if (nextList == nextSpan)
        {
            nextList = nextSpan->next;
            found = true;
        }
        else if (nextList)
        {
            Span* prev = nextList;
            while (prev->next)
            {
                if (prev->next == nextSpan)
                {
                    prev->next = nextSpan->next;
                    found = true;
                    break;
                }
                prev = prev->next;
            }
        }

        if (found)
        {
            span->numPages += nextSpan->numPages;
            spanMap_.erase(nextAddr);
            delete nextSpan;
        }
    }

    auto& list = freeSpans_[span->numPages];
    span->next = list;
    list = span;

    LeaveCriticalSection(&cs_);
}

void* PageCache::systemAlloc(size_t numPages)
{
    size_t size = numPages * PAGE_SIZE;
    void* ptr = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!ptr) return nullptr;
    memset(ptr, 0, size);
    return ptr;
}

} // namespace memoryPool
