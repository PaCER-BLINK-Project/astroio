#ifndef __ALLOCATION_POOL_H__
#define __ALLOCATION_POOL_H__

#include <map>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include "gpu_macros.hpp"

extern const bool disable_allocpool;

class AllocationPool {

    mutable std::shared_mutex mut;
    std::map<size_t, std::vector<char*>> unused;

    size_t _max = 0;
    size_t _total = 0;

protected:

    /**
     * @brief Abstract method for allocating new memory buffers
     * @param n size of buffer to allocate
     * @return char* pointer to newly allocated memory
     */
    virtual char* alloc(size_t n) const = 0;

    /**
     * @brief Abstract method for freeing memory
     */
    virtual void free(char *ptr, size_t n) const = 0;

public:
    AllocationPool() {}

    AllocationPool(AllocationPool &other) = delete;
    void operator=(const AllocationPool &) = delete;

    /**
     * @brief Set the max number of unused bytes the pool will keep track of.
     */
    void set_max_size(size_t max);

    /**
     * @brief Get the max number of unused bytes the pool will keep track of.
     */
    size_t get_max_size() const;

    /**
     * @brief Returns whether any memory buffers are currently unused
     */
    bool empty() const;

    /**
     * @brief Returns the total bytes currently tracked.
     */
    size_t total_bytes() const;

    size_t unused_bytes() const;

    /**
     * @brief Get a memory buffer from pool of size `n`, allocates new memory if necessary.
     * @param n_bytes Requested size of memory buffer.
     * @return Pointer to memory buffer.
     */
    char* allocate(size_t n_bytes);

    /**
     * @brief Returns memory allocation to the pool.
     */
    void deallocate(char* ptr, size_t n_bytes);

    /**
     * @brief Frees all currently unused buffers.
     */
    void clear();

    /**
     * @brief Free unused buffers until pool is under max size or until all
     *  unused buffers are free.
     * @returns Number of bytes successfully freed.
     */
    size_t cleanup();
};

template <typename PoolType>
class PoolSingleton {
public:
    /**
     * @brief Provides an instance of templated type, an implementation of the
     * "meyers singleton" pattern.
     */
    static PoolType& instance() {
        static PoolType single;
        return single;
    }
protected:
    PoolSingleton() = default;
    ~PoolSingleton() = default;
};

class PageableAllocationPool:
    public AllocationPool,
    public PoolSingleton<PageableAllocationPool> {
protected:
    char* alloc(size_t n) const override {
        return new char[n];
    }

    void free(char *ptr, size_t n) const override {
        delete[] ptr;
    }

public:
    ~PageableAllocationPool() {this->clear();}
};

#ifdef __GPU__
class DeviceAllocationPool :
    public AllocationPool,
    public PoolSingleton<DeviceAllocationPool> {
protected:
    char* alloc(size_t n) const override {
        char* ptr;
        gpuMalloc(&ptr, n);
        return ptr;
    }

    void free(char* ptr, size_t n) const override {
        gpuFree(ptr);
    }

public:
    ~DeviceAllocationPool() {this->clear();}
};

class PinnedAllocationPool :
    public AllocationPool,
    public PoolSingleton<PinnedAllocationPool> {
protected:
    char* alloc(size_t n) const override {
        char* ptr;
        gpuHostAlloc(&ptr, n);
        return ptr;
    }

    void free(char *ptr, size_t n) const override {
        gpuHostFree(ptr);
    }

public:
    ~PinnedAllocationPool() {this->clear();}
};

class ManagedAllocationPool :
    public AllocationPool,
    public PoolSingleton<ManagedAllocationPool> {
protected:
    char* alloc(size_t n) const override {
        char* ptr;
        gpuMallocManaged(&ptr, n);
        return ptr;
    }

    void free(char *ptr, size_t n) const override {
        gpuFree(ptr);
    }

public:
    ~ManagedAllocationPool() {this->clear();}
};
#endif

#endif