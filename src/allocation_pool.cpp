#include "allocation_pool.hpp"

#ifdef _DISABLE_ALLOCPOOL
const bool disable_allocpool = true;
#else
const bool disable_allocpool = false;
#endif

void AllocationPool::set_max_size(size_t max) {
    std::unique_lock lock(mut);
    this->_max = max;
}


size_t AllocationPool::get_max_size() const {
    std::shared_lock lock(mut);
    return this->_max;
}


bool AllocationPool::empty() const {
    std::shared_lock lock(mut);
    return unused.empty();
}

size_t AllocationPool::total_bytes() const {
    std::shared_lock lock(mut);
    return _total;
}

size_t AllocationPool::unused_bytes() const {
    std::shared_lock lock(mut);
    size_t num = 0;

    for (auto& [size, vec] : unused) {
        num += size * vec.size();
    }

    return num;
}

char* AllocationPool::allocate(size_t n_bytes) {
    if (n_bytes == 0) {
        throw std::invalid_argument{
            "AllocationPool::allocate : n_bytes must be greater than zero"
        };
    }

    if (disable_allocpool) {
        return alloc(n_bytes);
    }

    std::unique_lock lock(mut);

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

void AllocationPool::deallocate(char* ptr, size_t n_bytes) {
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

    std::unique_lock lock(mut);

    if (_max == 0 || _total <= _max) {
        unused[n_bytes].push_back(ptr);
    } else {
        // if we're low on memory, just free the buffer
        this->free(ptr, n_bytes);
        _total -= std::min(n_bytes, _total); // avoid an overflow
    }
};

void AllocationPool::clear() {
    if (disable_allocpool) return;

    std::unique_lock lock(mut);

    for (auto& [size, vec] : unused) {
        for (auto& mem : vec) {
            this->free(mem, size);
            _total -= size;
        }
        vec.clear();
    }
    unused.clear();
};

size_t AllocationPool::cleanup() {
    size_t freeable = unused_bytes();

    std::unique_lock lock(mut);

    if (_max == 0 || _total <= _max) return 0;

    size_t to_free = std::min(_total - _max, freeable);
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