#ifndef __MEMORY_BUFFER_H__
#define __MEMORY_BUFFER_H__

#include <fstream>
#include <iostream>
#include "gpu_macros.hpp"
#include "allocation_pool.hpp"

enum class MemoryType {
    PAGEABLE, // memory allocated with malloc or new[]
    PINNED, // memory allocated with gpuHostAlloc
    DEVICE, // GPU memory
    MANAGED // Host memory accessible by GPUs
};

template <typename T>
class MemoryBuffer {

    private:
    T* _data = nullptr;
    size_t n {0};
    MemoryType mem_type;

    protected:

    /**
     * @brief Get AllocationPool for given memory type.
     *
     * Calls AllocationPool::instance() to get a reference to a singleton object.
     * These singletons are static function-locals of instance(), making
     *  them inherently thread-safe and removing the need to make them static
     *  here too.
     *
     * @exception std::invalid_argument GPU is not enabled and mem_type is not
     *  pageable
     */
    inline static AllocationPool& alloc_pool(MemoryType mem_type) {
        switch (mem_type) {
            case MemoryType::PAGEABLE:
                return PageableAllocationPool::instance();
        #ifdef __GPU__
            case MemoryType::PINNED:
                return PinnedAllocationPool::instance();
            case MemoryType::DEVICE:
                return DeviceAllocationPool::instance();
            case MemoryType::MANAGED:
                return ManagedAllocationPool::instance();
        #else
            default:
                throw std::invalid_argument{
                    "MemoryBuffer: GPU not enabled, must use pageable memory."
                };
        #endif
        }
    }

    /**
     * @brief allocate new buffers without side-effects.
     */
    static T* _allocate(MemoryType mtype, size_t n) {
        auto& pool = alloc_pool(mtype);
        char* cptr = pool.allocate(n * sizeof(T));
        return reinterpret_cast<T*>(cptr);
    }

    /**
     * @brief deallocate buffer without side-effects.
     */
    static void _deallocate(MemoryType mtype, T* ptr, size_t n) {
        if (ptr == nullptr) return;

        auto& pool = alloc_pool(mtype);
        pool.deallocate(
            reinterpret_cast<char*>(ptr),
            n * sizeof(T)
        );
    }

    void reset() {
        if (_data != nullptr) {
            _deallocate(mem_type, _data, n);
            _data = nullptr;
        }
    }

    public:
    /**
     * @brief Create a new MemoryBuffer object which can hold a pointer to GPU or CPU allocated memory.
     * @param n_elements Number of elements to allocate space for in the buffer.
     * @param mem_type type of memory to be allocated. See `MemoryType`.
     *
    */
    MemoryBuffer(size_t n_elements, MemoryType mem_type = MemoryType::PAGEABLE) {
        allocate(n_elements, mem_type);
    }

    /**
     * @brief Default MemoryBuffer constructor creates a "null" object, with no memory allocation.
     * Memory can be allocated later with `realloc`.
     */
    MemoryBuffer(){}

    /**
     * @brief Create a new MemoryBuffer object by taking ownership of a pre-allocated array.
     * @param buffer Pointer to a pre-allocated memory location the MemoryObject will handle.
     * @param n_elements Number of elements in the buffer.
     * @param mem_type type of memory to be allocated. See `MemoryType`.
     *
     * @throws std::invalid_argument if `mem_type` is not pageable on a CPU-only build
     * @throws std::invalid_argument if `n_elements` is zero
     * @throws std::invalid_argument if `buffer` is nullptr
     */
    MemoryBuffer(T *buffer, size_t n_elements, MemoryType mem_type){
        #ifndef __GPU__
        if(mem_type != MemoryType::PAGEABLE)
            throw std::invalid_argument {
                "MemoryBuffer constructor: cannot use anything other than "
                "pageable memory on a CPU only build of the software"
            };
        #endif
        if(n_elements == 0)
            throw std::invalid_argument {
                "MemoryBuffer constructor: `n_elements` must be a positive number."
            };
        if(buffer == nullptr)
            throw std::invalid_argument {
                "MemoryBuffer constructor: won't accept a null pointer."
            };

        this->_data = buffer;
        this->n = n_elements;
        this->mem_type = mem_type;
    }

    /**
     * This conversion method allows MemoryBuffer objects to be tested in if statements.
     * For instance, if(!mem_buffer) mem_buffer.allocate(...)
     */
    explicit operator bool() const {
        return _data != nullptr;
    }

    /**
     * @brief Allocates memory space for the `MemoryBuffer` object.
     *
     * If the object is already associated with previously allocated memory,
     * that memory allocation is deleted.
     *
     * @param n_elements Number of elements to allocate space for in the buffer.
     * @param mem_type Type of memory to be allocated. See `MemoryType`.
     *
     * @exception std::invalid_argument if `n_elements` is less than zero.
     * @exception std::invalid_argument if CPU-only build and `mem_type` is not pageable.
    */
    void allocate(size_t n_elements, MemoryType mem_type = MemoryType::PAGEABLE) {
        reset();

        if (n_elements == 0) {
            throw std::invalid_argument {
                "MemoryBuffer::allocate: `n_elements` must be greater than zero."
            };
        }

        this->_data = _allocate(mem_type, n_elements);
        this->n = n_elements;
        this->mem_type = mem_type;
    }

    /**
     * @brief Transfer data to CPU.
     * @param to_type Type of memory to convert to
    */
    void to_cpu(MemoryType to_type = MemoryType::PAGEABLE) {
        #ifdef __GPU__
        if (mem_type == MemoryType::DEVICE && _data != nullptr) {

            T* tmp = _allocate(to_type, n);

            gpuMemcpy(tmp, _data, n * sizeof(T), gpuMemcpyDeviceToHost);

            _deallocate(mem_type, _data, n);

            _data = tmp;
            mem_type = to_type;
        }
        #endif
    }

    /**
     * @brief Transfer data to GPU.
    */
    void to_gpu() {
        #ifdef __GPU__
        if(mem_type != MemoryType::DEVICE && _data != nullptr) {

            T* tmp = _allocate(MemoryType::DEVICE, n);

            gpuMemcpy(tmp, _data, n * sizeof(T), gpuMemcpyHostToDevice);

            _deallocate(mem_type, _data, n);

            _data = tmp;
            mem_type = MemoryType::DEVICE;
        }
        #endif
    }


    /**
     * @brief Dump contents to a binary file.
     */
    void dump(std::string filename) const {
        this->to_cpu();
        std::ofstream outfile;

        outfile.open(filename, std::ofstream::binary);
        outfile.write(reinterpret_cast<char*>(_data), n * sizeof(T));

        if(!outfile) {
            throw std::runtime_error {
                "MemoryBuffer: error while dumping data to binary file."
            };
        }

        outfile.close();
    }

    /**
     * @brief load data from binary file and instantiate a new class of MemoryBuffer.
     */
    static MemoryBuffer<T> from_dump(std::string filename) {
        std::ifstream infile (filename, std::ifstream::binary);
        // get size of file
        infile.seekg(0, infile.end);
        size_t size = infile.tellg();
        infile.seekg(0);

        char* buffer = alloc_pool(MemoryType::PAGEABLE).allocate(size);

        infile.read(buffer, size);
        infile.close();

        return MemoryBuffer<T> {
            reinterpret_cast<T*>(buffer),
            size / sizeof(T),
            MemoryType::PAGEABLE
        };
    }

    /**
     * @return pointer to the raw array.
    */
	#ifdef __GPU__
	__host__ __device__
	#endif
    T* data() {return _data;}
	#ifdef __GPU__
	__host__ __device__
	#endif
    const T* data() const {return _data;}

    /**
     * @return `true` if memory resides on GPU, `false` otherwise.
    */
    bool on_gpu() const {return mem_type == MemoryType::DEVICE;}

    /**
     * @return `true` if memory has been allocated as pinned, `false` otherwise.
    */
    bool pinned() const {return mem_type == MemoryType::PINNED;}

    /**
     * @brief return the number of elements in the buffer.
    */
    size_t size() const {return n;};

    size_t bytes() const {return n * sizeof(T);}

    MemoryBuffer(const MemoryBuffer& other) {

        n = other.n;
        mem_type = other.mem_type;
        _data = nullptr;

        if (other) {
            allocate(n, mem_type);

            if (mem_type == MemoryType::PAGEABLE
             || mem_type == MemoryType::PINNED
             || mem_type == MemoryType::MANAGED) {
                memcpy(_data, other._data, n * sizeof(T));
            }
            else if (mem_type == MemoryType::DEVICE) {
                gpuMemcpy(_data, other._data, n * sizeof(T), gpuMemcpyDeviceToDevice);
            }
        }
    }

    /// @todo other.n should probably be set to 0
    MemoryBuffer(MemoryBuffer&& other) : n {other.n}, mem_type {other.mem_type},
        _data {other._data}
    {
        other._data = nullptr;
    }

    MemoryBuffer& operator=(const MemoryBuffer& other){
        if(this == &other) return *this;
        reset();

        n = other.n;
        mem_type = other.mem_type;
        _data = nullptr;

        if (other) {
            allocate(n, mem_type);

            if (mem_type == MemoryType::PAGEABLE
             || mem_type == MemoryType::PINNED
             || mem_type == MemoryType::MANAGED) {
                memcpy(_data, other._data, n * sizeof(T));
            }
            else if (mem_type == MemoryType::DEVICE) {
                gpuMemcpy(_data, other._data, n * sizeof(T), gpuMemcpyDeviceToDevice);
            }
        }

        return *this;
    }

    MemoryBuffer& operator=(MemoryBuffer&& other){
        reset();
        n = other.n;
        mem_type = other.mem_type;
        _data = other._data;
        other._data = nullptr;
        return *this;
    }

    T& operator[](int i){ return _data[i]; }
    const T& operator[](int i) const { return _data[i]; }

    ~MemoryBuffer() {
        this->reset();
    }
};

#endif
