#ifndef __MEMORY_BUFFER_H__
#define __MEMORY_BUFFER_H__

#include <fstream>
#include "gpu_macros.hpp"

template <typename T>
class MemoryBuffer {

    private:
    T* _data = nullptr;
    size_t n {0};
    bool _on_gpu {false};
    bool _pinned {false};

    public:
    /**
     * @brief Create a new MemoryBuffer object which can hold a pointer to GPU or CPU allocated memory.
     * @param n_elements Number of elements to allocate space for in the buffer.
     * @param pinned Indicate whether the memory must be pinned (only for GPU enabled installations).
     * @param on_gpu Indicate whether to allocate memory on GPU (`true`) or CPU (`false`). 
     * 
    */
    MemoryBuffer(size_t n_elements, bool pinned, bool on_gpu) {
        allocate(n_elements, pinned, on_gpu);
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
     * @param pinned Indicate whether the memory is pinned (only for GPU enabled installations).
     * @param on_gpu Indicate whether the memory is allocated on GPU (`true`) or CPU (`false`). 
     */
    MemoryBuffer(T *buffer, size_t n_elements, bool pinned, bool on_gpu){
        #ifndef __GPU__
        if(on_gpu || pinned)
            throw std::invalid_argument { "MemoryBuffer constructor: cannot use `pinned` or `on_gpu` "
            "on a CPU only build of the software." };
        #endif
        if(on_gpu && pinned)
            throw std::invalid_argument { "MemoryBuffer constructor: gpu memory cannot be pinned." };
        if(n_elements == 0) throw std::invalid_argument {"MemoryBuffer constructor: `n_elements` "
        "must be a positive number."};
        if(!buffer) throw std::invalid_argument {"MemoryBuffer constructor: won't accept a null pointer."};
        this->_data = buffer;
        this->n = n_elements;
        this->_pinned = pinned;
        this->_on_gpu = on_gpu;
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
     * @param pinned Indicate whether the memory must be pinned (only for GPU enabled installations).
     * @param on_gpu Indicate whether to allocate memory on GPU (`true`) or CPU (`false`). 
     * 
    */
    void allocate(size_t n_elements, bool pinned, bool on_gpu){
        this->~MemoryBuffer();
        #ifndef __GPU__
        if(on_gpu || pinned)
            throw std::invalid_argument { "MemoryBuffer::allocate: cannot use `pinned` or `on_gpu` "
            "on a CPU only build of the software." };
        #endif
        if(on_gpu && pinned)
            throw std::invalid_argument { "MemoryBuffer::allocate: gpu memory cannot be pinned." };
        if(n_elements == 0) throw std::invalid_argument {"MemoryBuffer::allocate: `n_elements` "
        "must be a positive number."};
        #ifdef __GPU__
        if(pinned) {
            gpuHostAlloc(&this->_data, sizeof(T) * n_elements);
        }else if(on_gpu){
            gpuMalloc(&this->_data, sizeof(T) * n_elements);
        }
        #endif
        if(!pinned && !on_gpu){
            this->_data = new T[n_elements];
        }
        this->n = n_elements;
        this->_pinned = pinned;
        this->_on_gpu = on_gpu;
    }

    /**
     * @brief Transfer data to CPU.
    */
    void to_cpu(bool pinned = false) {
        #ifdef __GPU__
        if(_on_gpu && _data){
            T* tmp;
            if(_pinned) gpuHostAlloc(&tmp, sizeof(T) * n);
            else tmp = new T[n];
            gpuMemcpy(tmp, _data, sizeof(T) * n, gpuMemcpyDeviceToHost);
            gpuFree(_data);
            _data = tmp;
            _on_gpu = false;
        }
        #endif
    }

    /**
     * @brief Transfer data to GPU.
    */
    void to_gpu(){
        #ifdef __GPU__
        if(!_on_gpu && _data){
            T* tmp;
            gpuMalloc(&tmp, sizeof(T) * n);
            gpuMemcpy(tmp, _data, sizeof(T) * n, gpuMemcpyHostToDevice);
            if(_pinned) gpuHostFree(_data);
            else delete[] _data;
            _data = tmp;
            _on_gpu = true;
        }
        #endif
    }


    /**
     * @brief Dump contents to a binary file.
     */
    void dump(std::string filename) const {
        std::ofstream outfile;
        outfile.open(filename, std::ofstream::binary);
        outfile.write(_data, n * sizeof(T));
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
        return MemoryBuffer<T> {reinterpret_cast<T>(buffer), size / sizeof(T), false, false};
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
    bool on_gpu() const {return _on_gpu;}

    /**
     * @return `true` if memory has been allocated as pinned, `false` otherwise.
    */
    bool pinned() const {return _pinned;}
    /**
     * @brief return the number of elements in the buffer.
    */
    size_t size() const {return n;};

    MemoryBuffer(const MemoryBuffer& other){
        n = other.n;
        _on_gpu = other._on_gpu;
        _pinned = other._pinned;
        _data = nullptr;
        if(!_pinned && !_on_gpu && other._data){
            _data = new T[n];
            memcpy(_data, other._data, n * sizeof(T));
        }
        #ifdef __GPU__
        if(_pinned && other._data){
            gpuHostAlloc(&_data, n * sizeof(T));
            memcpy(_data, other._data, n * sizeof(T));
        }else if(_on_gpu && other._data){
            gpuMalloc(&_data, n * sizeof(T));
            gpuMemcpy(_data, other._data, n * sizeof(T), gpuMemcpyDeviceToDevice);
        }
        #endif
    }

    MemoryBuffer(MemoryBuffer&& other) : n {other.n}, _on_gpu {other._on_gpu},
        _pinned {other._pinned}, _data {other._data}
    {
        other._data = nullptr;
    }

    MemoryBuffer& operator=(const MemoryBuffer& other){
        if(this == &other) return *this;
        n = other.n;
        _on_gpu = other._on_gpu;
        _pinned = other._pinned;
        this->~MemoryBuffer();
        if(!_pinned && !_on_gpu && other._data){
            _data = new T[n];
            memcpy(_data, other._data, n * sizeof(T));
        }
        #ifdef __GPU__
        if(_pinned && other._data){
            gpuHostAlloc(&_data, n * sizeof(T));
            memcpy(_data, other._data, n * sizeof(T));
        }else if(_on_gpu && other._data){
            gpuMalloc(&_data, n * sizeof(T));
            gpuMemcpy(_data, other._data, n * sizeof(T), gpuMemcpyDeviceToDevice);
        }
        #endif
        return *this;
    }

    MemoryBuffer& operator=(MemoryBuffer&& other){
        this->~MemoryBuffer();
        n = other.n;
        _on_gpu = other._on_gpu;
        _pinned = other._pinned;
        _data = other._data;
        other._data = nullptr;
        return *this;
    }

    T& operator[](int i){ return _data[i]; }
    const T& operator[](int i) const { return _data[i]; }

    ~MemoryBuffer(){
        if(!_pinned && !_on_gpu && _data) delete[] _data;
        #ifdef __GPU__
        if(_pinned && _data) gpuHostFree(_data);
        if(_on_gpu && _data) gpuFree(_data);
        #endif
    }
};

#endif
