# Kama MemoryPool

A high-performance C++ memory pool featuring a three-tier caching architecture designed for multithreaded applications.

## Architecture

```
allocate() / deallocate()
        │
        ▼
┌─────────────────┐
│   ThreadCache    │  ← Per-thread, lock-free
│  (TLS storage)   │
└────────┬────────┘
         │ batch fetch / return
         ▼
┌─────────────────┐
│  CentralCache    │  ← Shared, spinlock per size class
│   (singleton)    │
└────────┬────────┘
         │ span allocation
         ▼
┌─────────────────┐
│   PageCache      │  ← CRITICAL_SECTION, manages 4K pages
│   (singleton)    │
└────────┬────────┘
         │
         ▼
    VirtualAlloc
```

- **ThreadCache** — Each thread holds a free list array indexed by size class (8–256KB). Allocations and deallocations are lock-free within a thread.
- **CentralCache** — Singleton shared across threads. Uses per-size-class spinlocks to distribute memory in batches from/to ThreadCaches, and to split pages into smaller blocks.
- **PageCache** — Manages spans (contiguous page ranges) at 4K page granularity. Handles span splitting, merging, and direct OS allocation via `VirtualAlloc`.

## Key Features

- **Thread-safe**: Per-thread cache eliminates contention for most operations; CentralCache uses fine-grained spinlocks.
- **Batch transfers**: ThreadCache fetches and returns memory in batches to reduce CentralCache traffic.
- **Span coalescing**: PageCache merges adjacent free spans to reduce fragmentation.
- **Size class alignment**: All allocations rounded up to 8-byte alignment; up to 256 KB per block. Larger allocations fall back to `malloc`/`free`.

## Build

### Prerequisites

- CMake ≥ 3.10
- C++17 compiler (MSVC / MinGW)
- Windows

```bash
mkdir build && cd build
cmake ..
make
```

### Run Tests

```bash
make test    # unit tests
make perf    # performance benchmarks (vs. new/delete)
```

## Usage

```cpp
#include "MemoryPool.h"
using namespace Kama_memoryPool;

void* ptr = MemoryPool::allocate(128);   // 128 bytes
MemoryPool::deallocate(ptr, 128);
```

The `deallocate` call requires the original allocation size. Allocations larger than 256 KB bypass the pool and use the system allocator directly.

## Project Structure

```
.
├── include/
│   ├── Common.h          # BlockHeader, SizeClass, constants
│   ├── ThreadCache.h     # Per-thread cache
│   ├── CentralCache.h    # Shared central cache
│   ├── PageCache.h       # Page-level span manager
│   └── MemoryPool.h      # Public API
├── src/
│   ├── ThreadCache.cpp
│   ├── CentralCache.cpp
│   └── PageCache.cpp
├── tests/
│   ├── UnitTest.cpp       # Correctness tests
│   └── PerformanceTest.cpp # Benchmarks vs. new/delete
└── CMakeLists.txt
```

## Performance

Performance tests benchmark the pool against raw `new`/`delete` across three scenarios:

| Scenario | Description |
|----------|-------------|
| Small allocations | 100,000 × 32-byte alloc/free cycles |
| Multi-threaded | 4 threads, 25,000 mixed-size allocs each |
| Mixed sizes | 50,000 allocs with random sizes from 16B–2KB |

Run `make perf` to see results on your machine.
