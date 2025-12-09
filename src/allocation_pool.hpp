#ifndef __ALLOCATION_POOL_H__
#define __ALLOCATION_POOL_H__

#include <map>
#include <vector>
#include "gpu_macros.hpp"

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
    virtual char* alloc(size_t n) = 0;

    /**
     * @brief Abstract method for freeing memory
     */
    virtual void free(char* ptr, size_t n) = 0;

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
     * @return char* Pointer to memory buffer.
     */
    char* allocate(size_t n_bytes) {
        if (n_bytes == 0) {
            throw std::invalid_argument{
                "AllocationPool::allocate : n_bytes must be greater than zero"
            };
        }

        char* ptr;
        auto it = unused.find(n_bytes);

        // check if key exists in map and the value (vector) is not empty
        if (it != unused.end() && !it->second.empty()) {

            ptr = it->second.back();
            it->second.pop_back();

            // remove the key from the map if we've just emptied the vector
            if (it->second.empty()) {
                unused.erase(it);
            }
        } else { // no unused buffer with size `n_bytes`
            ptr = alloc(n_bytes);
            _total += n_bytes;
        }

        return ptr;
    };

    /**
     * @brief returns memory allocation to the pool.
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

        if (_max == 0 || _total <= _max) {
            unused[n_bytes].push_back(ptr);
        } else {
            // if we're low on memory, just free the buffer
            this->free(ptr, n_bytes);
            _total -= n_bytes;
        }
    };

    /**
     * @brief Clears all currently tracked blocks.
     */
    void clear() {
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
     * @brief Free buffers until pool is under max size.
     * @returns Number of bytes successfully freed.
     */
    size_t cleanup() {
        if (_max == 0 || _total <= _max) return 0;

        size_t to_free = std::min(_total - _max, unused_bytes());
        size_t freed = 0;

        for (auto it = unused.begin(); it != unused.end(); ) {
            size_t key = it->first;
            auto &vec = it->second;

            while (!vec.empty() && freed < to_free) {
                auto* ptr = vec.back();
                vec.pop_back();
                this->free(ptr, key);
                freed += key;
            }

            if (vec.empty()) {
                it = unused.erase(it);
            } else {
                ++it;
            }

            if (freed >= to_free) {
                break;
            }
        }

        return freed;
    }

    /**
     * @brief For debugging
     */
    std::map<size_t, size_t> summary() const {
        std::map<size_t, size_t> sum;

        for (auto& [key, vec] : unused) {
            sum[key] = vec.size();
        }

        return sum;
    }
};

class PageableAllocationPool : public AllocationPool {
protected:
    char* alloc(size_t n) override {
        std::clog << "PageableAllocationPool allocated " << n << " bytes\n";
        return new char[n];
    }

    void free(char* ptr, size_t n) override {
        std::clog << "PageableAllocationPool freed " << n << " bytes\n";
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
    char* alloc(size_t n) override {
        std::clog << "DeviceAllocationPool allocated " << n << " bytes\n";
        char* ptr;
        gpuMalloc(&ptr, n);
        return ptr;
    }

    void free(char* ptr, size_t n) override {
        std::clog << "DeviceAllocationPool freed " << n << " bytes\n";
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
    char* alloc(size_t n) override {
        std::clog << "PinnedAllocationPool allocated " << n << " bytes\n";
        char* ptr;
        gpuHostAlloc(&ptr, n);
        return ptr;
    }

    void free(char* ptr, size_t n) override {
        std::clog << "PinnedAllocationPool freed " << n << " bytes\n";
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
    char* alloc(size_t n) override {
        std::clog << "ManagedAllocationPool allocated " << n << " bytes\n";
        char* ptr;
        gpuMallocManaged(&ptr, n);
        return ptr;
    }

    void free(char* ptr, size_t n) override {
        std::clog << "ManagedAllocationPool freed " << n << " bytes\n";
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