#ifndef __ALLOCATION_POOL_H__
#define __ALLOCATION_POOL_H__

#include <map>
#include <vector>
#include <stdexcept>
#include "gpu_macros.hpp"

#ifdef _ALLOCPOOL_OFF
static constexpr bool disable_allocpool = true;
#else
static constexpr bool disable_allocpool = false;
#endif

class AllocationPool {

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
    void set_max_size(size_t max) { this->_max = max; }

    /**
     * @brief Get the max number of unused bytes the pool will keep track of.
     */
    size_t get_max_size() const { return this->_max; }

    /**
     * @brief Returns whether any memory buffers are currently unused
     */
    bool empty() const { return unused.empty(); }

    /**
     * @brief Returns the total bytes currently tracked.
     */
    size_t total_bytes() const { return _total; }

    size_t unused_bytes() const {
        size_t num = 0;

        for (auto& [size, vec] : unused) {
            num += size * vec.size();
        }

        return num;
    }

    /**
     * @brief Get a memory buffer from pool of size `n`, allocates new memory if necessary.
     * @param n_bytes Requested size of memory buffer.
     * @return Pointer to memory buffer.
     */
    char* allocate(size_t n_bytes) {
        if (n_bytes == 0) {
            throw std::invalid_argument{
                "AllocationPool::allocate : n_bytes must be greater than zero"
            };
        }

        if (disable_allocpool) {
            return alloc(n_bytes);
        }

        char* ptr;
        auto it = unused.find(n_bytes);

        if (it == unused.end() || it->second.empty()) {

            ptr = alloc(n_bytes);
            _total += n_bytes;

        } else {

            auto& vec = it->second;

            ptr = vec.back();
            vec.pop_back();

            // dont leave an empty vector hanging around
            if (vec.empty()) {
                unused.erase(it);
            }
        }

        return ptr;
    };

    /**
     * @brief Returns memory allocation to the pool.
     */
    void deallocate(char* ptr, size_t n_bytes) {
        if (ptr == nullptr) {
            throw std::invalid_argument{
                "AllocationPool::deallocate : argument ptr cannot be null"
            };
        }
        if (n_bytes == 0) {
            throw std::invalid_argument{
                "AllocationPool::deallocate : n_bytes must be greater than zero"
            };
        }

        if (disable_allocpool) {
            this->free(ptr, n_bytes);
            return;
        }

        if (_max == 0 || _total <= _max) {
            unused[n_bytes].push_back(ptr);
        } else {
            // if we're low on memory, just free the buffer
            this->free(ptr, n_bytes);
            _total -= std::min(n_bytes, _total); // avoid an overflow
        }
    };

    /**
     * @brief Frees all currently unused buffers.
     */
    void clear() {
        if (disable_allocpool) return;

        for (auto& [size, vec] : unused) {
            for (auto& mem : vec) {
                this->free(mem, size);
                _total -= size;
            }
            vec.clear();
        }
        unused.clear();
    };

    /**
     * @brief Free unused buffers until pool is under max size or until all
     *  unused buffers are free.
     * @returns Number of bytes successfully freed.
     */
    size_t cleanup() {
        if (_max == 0 || _total <= _max) return 0;

        size_t to_free = std::min(_total - _max, unused_bytes());
        size_t freed = 0;

        auto it = unused.begin();

        while (freed < to_free && it != unused.end()) {
            auto& [key, vec] = *it;

            while (freed < to_free && !vec.empty()) {
                char* ptr = vec.back();
                vec.pop_back();

                this->free(ptr, key);
                _total -= key;
                freed += key;
            }

            // if we've freed everything in that vector, erase entry in the map
            if (vec.empty()) {
                it = unused.erase(it);
            } else {
                ++it;
            }
        }

        return freed;
    }
};

class PageableAllocationPool : public AllocationPool {
protected:
    char* alloc(size_t n) const override {
        return new char[n];
    }

    void free(char *ptr, size_t n) const override {
        delete[] ptr;
    }
public:
    // static method for getting singleton instance
    static PageableAllocationPool& instance() {
        static PageableAllocationPool single;
        return single;
    }

    PageableAllocationPool() {};

    ~PageableAllocationPool() {
        this->clear();
    }
};

#ifdef __GPU__
class DeviceAllocationPool : public AllocationPool {
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
    // static method for getting singleton instance
    static DeviceAllocationPool& instance() {
        static DeviceAllocationPool single;
        return single;
    }

    DeviceAllocationPool() {};

    ~DeviceAllocationPool() {
        this->clear();
    }
};

class PinnedAllocationPool : public AllocationPool {
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
    // static method for getting singleton instance
    static PinnedAllocationPool& instance() {
        static PinnedAllocationPool single;
        return single;
    }

    PinnedAllocationPool() {};

    ~PinnedAllocationPool() {
        this->clear();
    }
};

class ManagedAllocationPool : public AllocationPool {
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
    // static method for getting singleton instance
    static ManagedAllocationPool& instance() {
        static ManagedAllocationPool single;
        return single;
    }

    ManagedAllocationPool() {};

    ~ManagedAllocationPool() {
        this->clear();
    }
};
#endif

#endif