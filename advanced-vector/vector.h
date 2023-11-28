#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <iostream>
#include <type_traits>
#include <algorithm>
#include <iostream>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }
    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept : buffer_(std::move(other.buffer_)), capacity_(std::move(other.capacity_)) {}
    RawMemory& operator=(RawMemory&& rhs) noexcept  {
        buffer_   = std::move(rhs.buffer_); 
        capacity_ = std::move(rhs.capacity_);
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }
    size_t& GetCapacity()  {
        return capacity_;
    }
    const size_t& GetCapacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};


template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;
    Vector() = default;
    explicit Vector(size_t size): data_(size), size_(size) {  std::uninitialized_value_construct_n ( data_.GetAddress(), size );}
    Vector(const Vector& other): data_(other.size_), size_(other.size_) {
                                std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }
    Vector(Vector&& other) noexcept : data_(other.size_) {Swap (other);}
    iterator begin() noexcept;
    iterator end() noexcept;
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;
    const_iterator cbegin() const noexcept;
    const_iterator cend() const noexcept;
    Vector& operator=(const Vector& rhs) noexcept {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                /* Применить copy-and-swap */
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                /* Скопировать элементы из rhs, создав при необходимости новые
                   или удалив существующие */
                if (rhs.size_ > size_) {
                    std::uninitialized_copy_n(rhs.data_.GetAddress(), (rhs.size_ - size_), data_.GetAddress() + size_);
                    for (size_t i = 0; i != (rhs.size_ - size_); ++i) {
                        *(data_.GetAddress() + i) = rhs.data_[i];
                    }
                }
                else {
                    for (size_t i = 0; i != rhs.size_; ++i) {
                        *(data_.GetAddress() + i) = rhs.data_[i];
                    }
                }
                
                if (size_ > rhs.size_) {
                    std::destroy_n(data_.GetAddress() + rhs.size_, (size_ - rhs.size_));
                }
               
                size_= rhs.size_;
                
            }
        }
        return *this;
    }
    Vector& operator=(Vector&& rhs) noexcept {
         if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    void Reserve(size_t new_capacity);

    size_t Size() const noexcept { return size_; }

    size_t Capacity() const noexcept { return data_.Capacity(); }

    const T& operator[](size_t index) const noexcept {return const_cast<Vector&>(*this)[index];}

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    ~Vector() {std::destroy_n(data_.GetAddress(), size_);}

    void Swap(Vector& other) noexcept {
         data_.Swap(other.data_);
         std::swap(size_, other.size_);
    }
    void Resize(size_t new_size);
    void PopBack() noexcept;

    template <typename FR>
    void PushBack(FR&& value); 

    template <typename... Args>
    T& EmplaceBack(Args&&... args);
    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args);
    iterator Insert(const_iterator pos, const T& value);
    iterator Insert(const_iterator pos, T&& value);
    iterator Erase(const_iterator pos);

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};



template <typename T>
void Vector<T>::Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
        data_.GetCapacity() = new_capacity;
}

template <typename T>
void Vector<T>::Resize(size_t new_size) {

    if (new_size == size_) {return;}
    if (new_size > size_){
        Reserve(new_size);
        std::uninitialized_value_construct_n ( data_.GetAddress() + size_, new_size - size_ );
        size_ = new_size;
    }
    else {
        std::destroy_n(data_.GetAddress() + new_size, (size_ - new_size));
        size_ = new_size;
    }
}



//forward reference
 template <typename T>
 template <typename FR>
void Vector<T>::PushBack(FR&& value) {
    using forw_type = decltype(std::forward<FR>(value));
   // auto typr_value = decltype(std::forward<FR>(value));
    if (size_ == Capacity()) {
        auto new_size = size_ == 0 ? 1 : size_ * 2;    
        RawMemory<T> new_data( new_size );
        
        if constexpr ((std::is_rvalue_reference_v<forw_type>) && (std::is_nothrow_move_constructible_v<forw_type> || !std::is_copy_constructible_v<forw_type>)) {
            std::uninitialized_move_n(&value, 1, new_data.GetAddress() + size_); 
        } else {
            std::uninitialized_copy_n(&value, 1, new_data.GetAddress() + size_);
        }


        
        if constexpr ( (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)) { //|| !std::is_copy_constructible_v<forw_type>)
            
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());   
        }

        data_.Swap(new_data);
        std::destroy_n(new_data.GetAddress(), size_);
        
        data_.GetCapacity() = new_size;
        ++size_;
        return;
    }
    // std::cout << "std::is_lvalue_reference<FR>::value "    << (std::is_lvalue_reference<forw_type>::value)  <<std::endl; 
    // std::cout << "(std::is_rvalue_reference_v<FR>) "       << (std::is_rvalue_reference_v<forw_type>)       <<std::endl; 
    // std::cout << "std::is_nothrow_move_constructible_v<T> " << std::is_nothrow_move_constructible_v<forw_type> <<std::endl; 
    // std::cout << "!std::is_copy_constructible_v<T> "        << !std::is_copy_constructible_v<forw_type>        <<std::endl; 
    if constexpr (( std::is_rvalue_reference_v<forw_type> ) 
                        && (std::is_nothrow_move_constructible_v<forw_type>
                        || !std::is_copy_constructible_v<forw_type>)) {
            
            std::uninitialized_move_n(&value, 1, data_.GetAddress() + size_);
             ++size_;
    } else {
            std::uninitialized_copy_n(&value, 1, data_.GetAddress() + size_);
             ++size_;
    }

   
}


template <typename T>
void Vector<T>::PopBack() noexcept {
    std::destroy_n(data_.GetAddress() + size_-1, 1);
    --size_;
}

template <typename T>
template <typename... Args>
T& Vector<T>::EmplaceBack(Args&&... args) {
    if (size_ == Capacity()) {
        auto new_size = size_ == 0 ? 1 : size_ * 2; 
       
        RawMemory<T> new_data(new_size);
        auto iter = new(new_data.GetAddress()+size_) T(std::forward<Args>(args)...);

        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
        data_.GetCapacity() = new_size;
        ++size_;
        return *iter;

    } else {
        auto iter = new(data_.GetAddress()+size_) T(std::forward<Args>(args)...);
        ++size_;
        return *iter;
    }
    
}


template <typename T>
typename Vector<T>::iterator Vector<T>::begin() noexcept {
    return data_.GetAddress();
}
template <typename T>
typename Vector<T>::iterator Vector<T>::end() noexcept {
    return (data_.GetAddress()+size_);
}
template <typename T>
typename Vector<T>::const_iterator Vector<T>::begin() const noexcept {
    return const_iterator (data_.GetAddress());
}
template <typename T>
typename Vector<T>::const_iterator Vector<T>::end() const noexcept {
    return const_iterator (data_.GetAddress()+size_);
}
template <typename T>
typename Vector<T>::const_iterator Vector<T>::cbegin() const noexcept {
    return data_.GetAddress();
}
template <typename T>
typename Vector<T>::const_iterator Vector<T>::cend() const noexcept {
    return (data_.GetAddress()+size_);
}

template <typename T>
template <typename... Args>
typename Vector<T>::iterator Vector<T>::Emplace(const_iterator pos, Args&&... args) {
    auto index = pos - data_.GetAddress();
    if (size_ < Capacity()) {
        if (size_ == 0) {
            auto iter = new(data_.GetAddress()) T(std::forward<Args>(args)...);
            ++size_;
            return iter;
        }
        
        new (data_.GetAddress() + size_) T(std::move(*(data_.GetAddress() + size_ - 1)));
        //const_iterator end = this->end();
                // if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    try {
                        std::move_backward( data_.GetAddress()+index, data_.GetAddress() + size_, (data_.GetAddress() + size_ + 1));
                    }
                    catch (...) {
                        std::destroy_n(data_.GetAddress() + size_, 1);
                        throw;
                    }


                // }
                // else {
                //     std::copy_backward( pos, end, &data_[size_+1]);
                // }
                std::destroy_n(data_.GetAddress() + index, 1);
        
        ++size_;
        
        return (new (data_.GetAddress() + index) T(std::forward<Args>(args)...));
    }
    else {
        
        auto new_size = size_ == 0 ? 1 : size_ * 2; 
        RawMemory<T> new_data(new_size);
        auto index = pos - data_.GetAddress();
        new(new_data.GetAddress()+index) T(std::forward<Args>(args)...);
        
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), index, new_data.GetAddress());
            std::uninitialized_move_n(data_.GetAddress() + index, (size_ - index), new_data.GetAddress()+index+1);
        } else {
            try {
                std::uninitialized_copy_n(data_.GetAddress(), index, new_data.GetAddress());
                std::uninitialized_copy_n(data_.GetAddress() + index, (size_ - index), new_data.GetAddress()+index+1);
            }
            catch (...){
                //удалить одиночный 
                std::destroy_n(new_data.GetAddress()+index, 1);
                throw;
            }
        }
        
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
        data_.GetCapacity() = new_size;
        ++size_;
        return &data_[index];
    }
}

template <typename T>
typename Vector<T>::iterator Vector<T>::Insert(const_iterator pos, const T& value) {
    return this->Emplace(pos, value);
}

template <typename T>
typename Vector<T>::iterator Vector<T>::Insert(const_iterator pos, T&& value) {
    return this->Emplace(pos, std::move(value));
}

template <typename T>
typename Vector<T>::iterator Vector<T>::Erase(const_iterator pos) {
    if (size_ == 0) {return this->end();}
    else if (size_ == 1) {
        std::destroy_n(data_.GetAddress() + size_- 1, 1);
        --size_;
        return this->end();
    }
    else {
        
        auto index = pos - data_.GetAddress();    
        std::move(data_.GetAddress() + index + 1, data_.GetAddress() + size_, data_.GetAddress() + index); 
        std::destroy_n(data_.GetAddress() + size_ - 1, 1);
        --size_;
        
        return &data_[index];
    }
}