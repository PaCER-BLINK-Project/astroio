#ifndef __MEMORY_BUFFER_H__
#define __MEMORY_BUFFER_H__

#include <fstream>
#include "gpu_macros.hpp"
#include <iostream>

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
     */
    MemoryBuffer(T *buffer, size_t n_elements, MemoryType mem_type){
        #ifndef __GPU__
        if(mem_type != MemoryType::PAGEABLE)
            throw std::invalid_argument { "MemoryBuffer constructor: cannot use anything other than pageable memory "
            "on a CPU only build of the software." };
        #endif
        if(n_elements == 0) throw std::invalid_argument {"MemoryBuffer constructor: `n_elements` "
        "must be a positive number."};
        if(!buffer) throw std::invalid_argument {"MemoryBuffer constructor: won't accept a null pointer."};
        this->_data = buffer;
        this->n = n_elements;
        this->mem_type = mem_type;
    }

    /**
     * This conversion method allows MemoryBuffer objects to be tested in if statements.
     * For instance, if(!mem_buffer) mem_buffer.allocate(...)
     */
    explicit operator bool() const {
        return (_data == nullptr ? false : true);
    }

    /**
     * @brief Allocates memory space for the `MemoryBuffer` object. If the object is already associated 
     * with previously allocated memory, that memory allocation is deleted.
     * @param n_elements Number of elements to allocate space for in the buffer.
     * @param on_gpu Indicate whether to allocate memory on GPU (`true`) or CPU (`false`). 
     * @param pinned Indicate whether the memory must be pinned (only for GPU enabled installations).
     * 
    */
    void allocate(size_t n_elements, MemoryType mem_type = MemoryType::PAGEABLE){
        if(_data) this->~MemoryBuffer();
        #ifndef __GPU__
        if(mem_type != MemoryType::PAGEABLE)
            throw std::invalid_argument { "MemoryBuffer constructor: cannot use anything other than pageable memory "
            "on a CPU only build of the software." };
        #endif
        if(n_elements == 0) throw std::invalid_argument {"MemoryBuffer::allocate: `n_elements` "
        "must be a positive number."};
        #ifdef __GPU__
        if(mem_type == MemoryType::PINNED) {
            gpuHostAlloc(&this->_data, sizeof(T) * n_elements);
        }else if(mem_type == MemoryType::DEVICE){
            gpuMalloc(&this->_data, sizeof(T) * n_elements);
        }else if(mem_type == MemoryType::MANAGED){
            gpuMallocManaged(&this->_data, sizeof(T) * n_elements);
        }
        #endif
        if(mem_type == MemoryType::PAGEABLE){
            this->_data = new T[n_elements];
        }
        this->n = n_elements;
        this->mem_type = mem_type;
    }

    /**
     * @brief Transfer data to CPU.
    */
    void to_cpu(MemoryType to_type = MemoryType::PAGEABLE) {
        #ifdef __GPU__
        if(mem_type == MemoryType::DEVICE && _data){
            T* tmp;
            if(to_type == MemoryType::PINNED) {
                gpuHostAlloc(&tmp, sizeof(T) * n);
                mem_type =  MemoryType::PINNED;
            } else {
                tmp = new T[n];
                mem_type = MemoryType::PAGEABLE;
            }
            gpuMemcpy(tmp, _data, sizeof(T) * n, gpuMemcpyDeviceToHost);
            gpuFree(_data);
            _data = tmp;
        }
        #endif
    }

    /**
     * @brief Transfer data to GPU.
    */
    void to_gpu(){
        #ifdef __GPU__
        if(mem_type != MemoryType::DEVICE && _data){
            T* tmp;
            gpuMalloc(&tmp, sizeof(T) * n);
            gpuMemcpy(tmp, _data, sizeof(T) * n, gpuMemcpyHostToDevice);
            if(mem_type == MemoryType::PINNED) gpuHostFree(_data);
            else if(mem_type == MemoryType::MANAGED) gpuFree(_data);
            else delete[] _data;
            _data = tmp;
            mem_type = MemoryType::DEVICE;
        }
        #endif
    }


    /**
     * @brief Dump contents to a binary file.
     */
    void dump(std::string filename) {
        this->to_cpu();
        std::ofstream outfile;
        outfile.open(filename, std::ofstream::binary);
        outfile.write(reinterpret_cast<char*>(_data), n * sizeof(T));
        if(!outfile){
            throw std::runtime_error {"MemoryBuffer: error while dumping data to binary file."};
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
        char* buffer = new char[size];
        infile.read (buffer, size);
        infile.close();
        return MemoryBuffer<T> {reinterpret_cast<T*>(buffer), size / sizeof(T), MemoryType::PAGEABLE};
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

    MemoryBuffer(const MemoryBuffer& other){
        n = other.n;
        mem_type = other.mem_type;
        _data = nullptr;
        if(mem_type == MemoryType::PAGEABLE && other._data){
            _data = new T[n];
            memcpy(_data, other._data, n * sizeof(T));
        }
        #ifdef __GPU__
        if(mem_type == MemoryType::PINNED && other._data){
            gpuHostAlloc(&_data, n * sizeof(T));
            memcpy(_data, other._data, n * sizeof(T));
        }else if(mem_type == MemoryType::DEVICE && other._data){
            gpuMalloc(&_data, n * sizeof(T));
            gpuMemcpy(_data, other._data, n * sizeof(T), gpuMemcpyDeviceToDevice);
        }else if(mem_type == MemoryType::MANAGED && other._data){
            gpuMallocManaged(&_data, n * sizeof(T));
            memcpy(_data, other._data, n * sizeof(T));
        }
        #endif
    }

    MemoryBuffer(MemoryBuffer&& other) : n {other.n}, mem_type {other.mem_type},
        _data {other._data}
    {
        other._data = nullptr;
    }

    MemoryBuffer& operator=(const MemoryBuffer& other){
        if(this == &other) return *this;
        if(_data) this->~MemoryBuffer();
        n = other.n;
        mem_type = other.mem_type;
        if(mem_type == MemoryType::PAGEABLE && other._data){
            _data = new T[n];
            memcpy(_data, other._data, n * sizeof(T));
        }
        #ifdef __GPU__
        if(mem_type == MemoryType::PINNED && other._data){
            gpuHostAlloc(&_data, n * sizeof(T));
            memcpy(_data, other._data, n * sizeof(T));
        }else if(mem_type == MemoryType::DEVICE && other._data){
            gpuMalloc(&_data, n * sizeof(T));
            gpuMemcpy(_data, other._data, n * sizeof(T), gpuMemcpyDeviceToDevice);
        }else if(mem_type == MemoryType::MANAGED && other._data){
            gpuMallocManaged(&_data, n * sizeof(T));
            memcpy(_data, other._data, n * sizeof(T));
        }
        #endif
        return *this;
    }

    MemoryBuffer& operator=(MemoryBuffer&& other){
        if(_data) this->~MemoryBuffer();
        n = other.n;
        mem_type = other.mem_type;
        _data = other._data;
        other._data = nullptr;
        return *this;
    }

    T& operator[](int i){ return _data[i]; }
    const T& operator[](int i) const { return _data[i]; }

    ~MemoryBuffer(){
        if(mem_type == MemoryType::PAGEABLE && _data) delete[] _data;
        #ifdef __GPU__
        if(mem_type == MemoryType::PINNED && _data) gpuHostFree(_data);
        if((mem_type == MemoryType::DEVICE || 
            mem_type == MemoryType::MANAGED) && _data) gpuFree(_data);
        #endif
    }
};

#endif
