// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_EXTENSION_HPP
#define ORG_VLEPROJECT_IRRITATOR_EXTENSION_HPP

#include <irritator/core.hpp>

#include <iterator>
#include <type_traits>
#include <utility>

#include <cstdint>

namespace irt {

//! @brief A helper container to store Identifier -> T relation.
//! @tparam Identifier Any integer or enum type.
//! @tparam T Any type (trivial or not).
template<typename Identifier, typename T>
class table
{
public:
    static_assert(
      std::is_enum_v<Identifier> || std::is_integral_v<Identifier>,
      "Identifier must be a enumeration: enum class id : unsigned {};");

    struct value_type
    {
        value_type() noexcept = default;
        value_type(Identifier id, const T& value) noexcept;

        Identifier id;
        T          value;
    };

    using container_type  = vector<value_type>;
    using size_type       = typename container_type::size_type;
    using iterator        = typename container_type::iterator;
    using const_iterator  = typename container_type::const_iterator;
    using reference       = typename container_type::reference;
    using const_reference = typename container_type::const_reference;
    using pointer         = typename container_type::pointer;
    using const_pointer   = typename container_type::const_pointer;

    container_type data;

    constexpr table() noexcept  = default;
    constexpr ~table() noexcept = default;

    constexpr void     set(Identifier id, const T& value) noexcept;
    constexpr T*       get(Identifier id) noexcept;
    constexpr const T* get(Identifier id) const noexcept;
    constexpr void     erase(Identifier id) noexcept;
    constexpr void     sort() noexcept;

    constexpr unsigned size() const noexcept;
    constexpr int      ssize() const noexcept;
};

//! @brief A vector like class but without dynamic allocation.
//! @tparam T Any type (trivial or not).
//! @tparam length The capacity of the vector.
template<typename T, sz length>
class small_vector
{
public:
    static_assert(length >= 1);
    static_assert(std::is_nothrow_destructible_v<T> ||
                  std::is_trivially_destructible_v<T>);

    using size_type = small_storage_size_t<length>;

private:
    std::byte m_buffer[length * sizeof(T)];
    size_type m_size;

public:
    using iterator        = T*;
    using const_iterator  = const T*;
    using reference       = T&;
    using const_reference = const T&;
    using pointer         = T*;
    using const_pointer   = const T*;

    constexpr small_vector() noexcept;
    constexpr small_vector(const small_vector& other) noexcept;
    constexpr ~small_vector() noexcept;

    constexpr small_vector& operator=(const small_vector& other) noexcept;
    constexpr small_vector(small_vector&& other) noexcept = delete;
    constexpr small_vector& operator=(small_vector&& other) noexcept = delete;

    constexpr status resize(int capacity) noexcept;
    constexpr void   clear() noexcept;

    constexpr reference       front() noexcept;
    constexpr const_reference front() const noexcept;
    constexpr reference       back() noexcept;
    constexpr const_reference back() const noexcept;

    constexpr T*       data() noexcept;
    constexpr const T* data() const noexcept;

    constexpr reference       operator[](int index) noexcept;
    constexpr const_reference operator[](int index) const noexcept;

    constexpr iterator       begin() noexcept;
    constexpr const_iterator begin() const noexcept;
    constexpr iterator       end() noexcept;
    constexpr const_iterator end() const noexcept;

    constexpr bool     can_alloc(int number = 1) noexcept;
    constexpr int      available() const noexcept;
    constexpr unsigned size() const noexcept;
    constexpr int      ssize() const noexcept;
    constexpr int      capacity() const noexcept;
    constexpr bool     empty() const noexcept;
    constexpr bool     full() const noexcept;

    template<typename... Args>
    constexpr reference emplace_back(Args&&... args) noexcept;
    constexpr void      pop_back() noexcept;
    constexpr void      swap_pop_back(int index) noexcept;
};

//! @brief A ring-buffer based on a fixed size container. m_head point to the
//! first element can be dequeue while m_tail point to the first constructible
//! element in the ring.
//! @tparam T Any type (trivial or not).
template<class T>
class ring_buffer
{
public:
    using size_type       = u64;
    using reference       = T&;
    using const_reference = const T&;
    using pointer         = T*;
    using const_pointer   = const T*;

    static_assert(std::is_nothrow_constructible_v<T> ||
                  std::is_nothrow_move_constructible_v<T>);

private:
    // @todo Replace m_buffer and m_capacity with std::span<T>.
    T*  m_buffer   = nullptr;
    i32 m_head     = 0;
    i32 m_tail     = 0;
    i32 m_capacity = 0;

    constexpr i32 advance(i32 position) const noexcept;
    constexpr i32 back(i32 position) const noexcept;

public:
    class iterator;
    class const_iterator;

    class iterator
    {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = T;
        using pointer           = T*;
        using reference         = T&;
        using const_pointer     = const T*;
        using const_reference   = const T&;

        friend ring_buffer;

    private:
        ring_buffer* ring;
        i32          i;

        iterator(ring_buffer* ring_, i32 i_) noexcept
          : ring(ring_)
          , i(i_)
        {}

    public:
        ring_buffer* buffer() noexcept { return ring; }
        i32          index() noexcept { return i; }

        iterator() noexcept
          : ring(nullptr)
          , i(0)
        {}

        iterator(const iterator& other) noexcept
          : ring(other.ring)
          , i(other.i)
        {}

        iterator& operator=(const iterator& other) noexcept
        {
            if (this != &other) {
                ring = const_cast<ring_buffer*>(other.ring);
                i    = other.i;
            }

            return *this;
        }

        reference operator*() const noexcept { return ring->m_buffer[i]; }
        pointer   operator->() const noexcept { return &ring->m_buffer[i]; }

        iterator& operator++() noexcept
        {
            if (ring) {
                i = ring->advance(i);

                if (i == ring->m_tail)
                    reset();
            }

            return *this;
        }

        iterator operator++(int) noexcept
        {
            iterator orig(*this);
            ++(*this);

            return orig;
        }

        iterator& operator--() noexcept
        {
            if (ring) {
                i = ring->back(i);

                if (i == ring->m_tail)
                    reset();
            }

            return *this;
        }

        iterator operator--(int) noexcept
        {
            iterator orig(*this);
            --(*orig);

            return orig;
        }

        bool operator==(const_iterator rhs) const noexcept
        {
            return ring == rhs.ring && i == rhs.i;
        }

        bool operator==(iterator rhs) const noexcept
        {
            return ring == rhs.ring && i == rhs.i;
        }

        bool operator!=(const_iterator rhs) const noexcept
        {
            return !(ring == rhs.ring && i == rhs.i);
        }

        bool operator!=(iterator rhs) const noexcept
        {
            return !(ring == rhs.ring && i == rhs.i);
        }

        void reset() noexcept
        {
            ring = nullptr;
            i    = 0;
        }
    };

    class const_iterator
    {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = T;
        using pointer           = T*;
        using reference         = T&;
        using const_pointer     = const T*;
        using const_reference   = const T&;

        friend ring_buffer;

    private:
        const ring_buffer* ring;
        i32                i;

        const_iterator(const ring_buffer* ring_, i32 i_) noexcept
          : ring(ring_)
          , i(i_)
        {}

    public:
        const ring_buffer* buffer() const noexcept { return ring; }
        i32                index() const noexcept { return i; }

        const_iterator() noexcept
          : ring(nullptr)
          , i(0)
        {}

        const_iterator(const const_iterator& other) noexcept
          : ring(other.ring)
          , i(other.i)
        {}

        const_iterator& operator=(const const_iterator& other) noexcept
        {
            if (this != &other) {
                ring = const_cast<const ring_buffer*>(other.ring);
                i    = other.i;
            }

            return *this;
        }

        const_reference operator*() const noexcept { return ring->m_buffer[i]; }
        const_pointer operator->() const noexcept { return &ring->m_buffer[i]; }

        const_iterator& operator++() noexcept
        {
            if (ring) {
                i = ring->advance(i);

                if (i == ring->m_tail)
                    reset();
            }

            return *this;
        }

        const_iterator operator++(int) noexcept
        {
            const_iterator orig(*this);
            ++(*this);

            return orig;
        }

        const_iterator& operator--() noexcept
        {
            if (ring) {
                i = ring->back(i);

                if (i == ring->m_tail)
                    reset();
            }

            return *this;
        }

        const_iterator operator--(int) noexcept
        {
            const_iterator orig(*this);
            --(*orig);

            return orig;
        }

        bool operator==(const_iterator rhs) const noexcept
        {
            return ring == rhs.ring && i == rhs.i;
        }

        bool operator==(iterator rhs) const noexcept
        {
            return ring == rhs.ring && i == rhs.i;
        }

        bool operator!=(const_iterator rhs) const noexcept
        {
            return !(ring == rhs.ring && i == rhs.i);
        }

        bool operator!=(iterator rhs) const noexcept
        {
            return !(ring == rhs.ring && i == rhs.i);
        }

        void reset() noexcept
        {
            ring = nullptr;
            i    = 0;
        }
    };

    friend class iterator;
    friend class const_iterator;

    constexpr ring_buffer() noexcept = default;
    constexpr ring_buffer(T* buffer, int capacity) noexcept;
    constexpr ~ring_buffer() noexcept;

    constexpr ring_buffer(const ring_buffer& rhs) noexcept = delete;
    constexpr ring_buffer& operator=(const ring_buffer& rhs) noexcept = delete;
    constexpr ring_buffer(ring_buffer&& rhs) noexcept;
    constexpr ring_buffer& operator=(ring_buffer&& rhs) noexcept;

    constexpr void swap(ring_buffer& rhs) noexcept;
    constexpr void clear() noexcept;
    constexpr void reset(T* buffer, int capacity) noexcept;

    template<typename... Args>
    constexpr bool emplace_front(Args&&... args) noexcept;
    template<typename... Args>
    constexpr bool emplace_back(Args&&... args) noexcept;

    constexpr bool push_front(const T& item) noexcept;
    constexpr void pop_front() noexcept;
    constexpr bool push_back(const T& item) noexcept;
    constexpr void pop_back() noexcept;
    constexpr void erase_after(iterator not_included) noexcept;
    constexpr void erase_before(iterator not_included) noexcept;

    template<typename... Args>
    constexpr bool emplace_enqueue(Args&&... args) noexcept;

    template<typename... Args>
    constexpr void force_emplace_enqueue(Args&&... args) noexcept;

    constexpr void force_enqueue(const T& item) noexcept;
    constexpr bool enqueue(const T& item) noexcept;
    constexpr void dequeue() noexcept;

    constexpr T&       front() noexcept;
    constexpr const T& front() const noexcept;
    constexpr T&       back() noexcept;
    constexpr const T& back() const noexcept;

    constexpr iterator       head() noexcept;
    constexpr const_iterator head() const noexcept;
    constexpr iterator       tail() noexcept;
    constexpr const_iterator tail() const noexcept;
    constexpr iterator       end() noexcept;
    constexpr const_iterator end() const noexcept;

    constexpr unsigned size() const noexcept;
    constexpr int      ssize() const noexcept;
    constexpr i32      capacity() const noexcept;
    constexpr i32      available() const noexcept;
    constexpr bool     empty() const noexcept;
    constexpr bool     full() const noexcept;
    constexpr i32      index_from_begin(i32 index) const noexcept;
};

template<typename T>
class hierarchy
{
public:
    using value_type = T;

private:
    hierarchy* m_parent  = nullptr;
    hierarchy* m_sibling = nullptr;
    hierarchy* m_child   = nullptr;
    T*         m_id      = nullptr;

public:
    hierarchy() noexcept = default;
    ~hierarchy() noexcept;

    void set_id(T* object) noexcept;
    T*   id() const noexcept;

    void parent_to(hierarchy& node) noexcept;
    void make_sibling_after(hierarchy& node) noexcept;
    bool parented_by(const hierarchy& node) const noexcept;
    void remove_from_parent() noexcept;
    void remove_from_hierarchy() noexcept;

    T* get_parent() const noexcept;  // parent of this node
    T* get_child() const noexcept;   // first child of this ndoe
    T* get_sibling() const noexcept; // next node with the same parent
    T* get_prior_sibling() const noexcept;
    T* get_next() const noexcept;
    T* get_next_leaf() const noexcept;

private:
    hierarchy<T>* get_prior_sibling_node() const noexcept;
};

/*****************************************************************************
 *
 * Containers implementation
 *
 ****************************************************************************/

template<typename Identifier, typename T>
table<Identifier, T>::value_type::value_type(Identifier id_,
                                             const T&   value_) noexcept
  : id(id_)
  , value(value_)
{}

template<typename Identifier, typename T>
constexpr void table<Identifier, T>::set(Identifier id, const T& value) noexcept
{
    if (auto* value_found = get(id); value_found) {
        *value_found = value;
    } else {
        data.emplace_back(id, value);
        sort();
    }
}

template<typename Identifier, typename T>
constexpr T* table<Identifier, T>::get(Identifier id) noexcept
{
    auto it = std::lower_bound(
      data.begin(), data.end(), id, [](auto& m, Identifier id) {
          return m.id < id;
      });

    return (!(it == data.end()) && (id == it->id)) ? &it->value : nullptr;
}

template<typename Identifier, typename T>
constexpr const T* table<Identifier, T>::get(Identifier id) const noexcept
{
    auto it = std::lower_bound(
      data.begin(), data.end(), id, [](const auto& m, Identifier id) {
          return m.id < id;
      });

    return (!(it == data.end()) && (id == it->id)) ? &it->value : nullptr;
}

template<typename Identifier, typename T>
constexpr void table<Identifier, T>::erase(Identifier id) noexcept
{
    auto it = std::lower_bound(
      data.begin(), data.end(), id, [](const auto& m, Identifier id) {
          return m.id < id;
      });

    if (!(it == data.end()) && (id == it->id)) {
        data.erase(it);
        sort();
    }
}

template<typename Identifier, typename T>
constexpr void table<Identifier, T>::sort() noexcept
{
    if (data.size() > 1)
        std::sort(data.begin(),
                  data.end(),
                  [](const auto& left, const auto& right) noexcept {
                      return left.id < right.id;
                  });
}

template<typename Identifier, typename T>
constexpr unsigned table<Identifier, T>::size() const noexcept
{
    return data.size();
}

template<typename Identifier, typename T>
constexpr int table<Identifier, T>::ssize() const noexcept
{
    return data.ssize();
}

// template<typename T, size_type length>
// class small_vector;

template<typename T, sz length>
constexpr small_vector<T, length>::small_vector() noexcept
{
    m_size = 0;
}

template<typename T, sz length>
constexpr small_vector<T, length>::small_vector(
  const small_vector<T, length>& other) noexcept
  : m_size(other.m_size)
{
    std::uninitialized_copy_n(other.data(), other.m_size, data());
}

template<typename T, sz length>
constexpr small_vector<T, length>::~small_vector() noexcept
{
    std::destroy_n(data(), m_size);
}

template<typename T, sz length>
constexpr small_vector<T, length>& small_vector<T, length>::operator=(
  const small_vector<T, length>& other) noexcept
{
    if (&other != this) {
        m_size = other.m_size;
        std::copy_n(other.data(), other.m_size, data());
    }

    return *this;
}

template<typename T, sz length>
constexpr status small_vector<T, length>::resize(int default_size) noexcept
{
    static_assert(std::is_nothrow_default_constructible_v<T> ||
                    std::is_trivially_default_constructible_v<T>,
                  "T must be nothrow or trivially default constructible to use "
                  "init() function");

    irt_return_if_fail(std::cmp_greater(default_size, 0) &&
                         std::cmp_less(default_size, length),
                       status::vector_init_capacity_error);

    const auto new_default_size = static_cast<size_type>(default_size);

    if (new_default_size > m_size)
        std::uninitialized_default_construct_n(data() + m_size,
                                               new_default_size - m_size);
    else
        std::destroy_n(data() + new_default_size, m_size - new_default_size);

    m_size = new_default_size;

    return status::success;
}

template<typename T, sz length>
constexpr T* small_vector<T, length>::data() noexcept
{
    return reinterpret_cast<T*>(&m_buffer[0]);
}

template<typename T, sz length>
constexpr const T* small_vector<T, length>::data() const noexcept
{
    return reinterpret_cast<const T*>(&m_buffer[0]);
}

template<typename T, sz length>
constexpr typename small_vector<T, length>::reference
small_vector<T, length>::front() noexcept
{
    irt_assert(m_size > 0);
    return m_buffer[0];
}

template<typename T, sz length>
constexpr typename small_vector<T, length>::const_reference
small_vector<T, length>::front() const noexcept
{
    irt_assert(m_size > 0);
    return m_buffer[0];
}

template<typename T, sz length>
constexpr typename small_vector<T, length>::reference
small_vector<T, length>::back() noexcept
{
    irt_assert(m_size > 0);
    return m_buffer[m_size - 1];
}

template<typename T, sz length>
constexpr typename small_vector<T, length>::const_reference
small_vector<T, length>::back() const noexcept
{
    irt_assert(m_size > 0);
    return m_buffer[m_size - 1];
}

template<typename T, sz length>
constexpr typename small_vector<T, length>::reference
small_vector<T, length>::operator[](int index) noexcept
{
    irt_assert(std::cmp_greater_equal(index, 0));
    irt_assert(std::cmp_less(index, m_size));

    return data()[index];
}

template<typename T, sz length>
constexpr typename small_vector<T, length>::const_reference
small_vector<T, length>::operator[](int index) const noexcept
{
    irt_assert(std::cmp_greater_equal(index, 0));
    irt_assert(std::cmp_less(index, m_size));

    return data()[index];
}

template<typename T, sz length>
constexpr typename small_vector<T, length>::iterator
small_vector<T, length>::begin() noexcept
{
    return data();
}

template<typename T, sz length>
constexpr typename small_vector<T, length>::const_iterator
small_vector<T, length>::begin() const noexcept
{
    return data();
}

template<typename T, sz length>
constexpr typename small_vector<T, length>::iterator
small_vector<T, length>::end() noexcept
{
    return data() + m_size;
}

template<typename T, sz length>
constexpr typename small_vector<T, length>::const_iterator
small_vector<T, length>::end() const noexcept
{
    return data() + m_size;
}

template<typename T, sz length>
constexpr unsigned small_vector<T, length>::size() const noexcept
{
    return m_size;
}

template<typename T, sz length>
constexpr int small_vector<T, length>::ssize() const noexcept
{
    return m_size;
}

template<typename T, sz length>
constexpr int small_vector<T, length>::capacity() const noexcept
{
    return length;
}

template<typename T, sz length>
constexpr bool small_vector<T, length>::empty() const noexcept
{
    return m_size == 0;
}

template<typename T, sz length>
constexpr bool small_vector<T, length>::full() const noexcept
{
    return m_size >= length;
}

template<typename T, sz length>
constexpr void small_vector<T, length>::clear() noexcept
{
    std::destroy_n(data(), m_size);
    m_size = 0;
}

template<typename T, sz length>
constexpr bool small_vector<T, length>::can_alloc(int number) noexcept
{
    return static_cast<std::ptrdiff_t>(length) -
             static_cast<std::ptrdiff_t>(m_size) >=
           static_cast<std::ptrdiff_t>(number);
}

template<typename T, sz length>
template<typename... Args>
constexpr typename small_vector<T, length>::reference
small_vector<T, length>::emplace_back(Args&&... args) noexcept
{
    static_assert(
      std::is_trivially_constructible_v<T, Args...> ||
        std::is_nothrow_constructible_v<T, Args...>,
      "T must but trivially or nothrow constructible from this argument(s)");

    assert(can_alloc(1) && "check alloc() with full() before using use.");

    std::construct_at(&(data()[m_size]), std::forward<Args>(args)...);
    ++m_size;

    return data()[m_size - 1];
}

template<typename T, sz length>
constexpr void small_vector<T, length>::pop_back() noexcept
{
    static_assert(std::is_nothrow_destructible_v<T> ||
                    std::is_trivially_destructible_v<T>,
                  "T must be nothrow or trivially destructible to use "
                  "pop_back() function");

    if (m_size) {
        std::destroy_at(data() + m_size - 1);
        --m_size;
    }
}

template<typename T, sz length>
constexpr void small_vector<T, length>::swap_pop_back(int index) noexcept
{
    irt_assert(std::cmp_greater_equal(index, 0) &&
               std::cmp_less(index, m_size));

    const auto new_index = static_cast<size_type>(index);

    if (new_index == m_size - 1) {
        pop_back();
    } else {
        auto to_delete = data() + new_index;
        auto last      = data() + m_size - 1;

        std::destroy_at(to_delete);

        if constexpr (std::is_move_constructible_v<T>) {
            std::uninitialized_move_n(last, 1, to_delete);
        } else {
            std::uninitialized_copy_n(last, 1, to_delete);
            std::destroy_at(last);
        }

        --m_size;
    }
}

template<class T>
constexpr i32 ring_buffer<T>::advance(i32 position) const noexcept
{
    return (position + 1) % m_capacity;
}

template<class T>
constexpr i32 ring_buffer<T>::back(i32 position) const noexcept
{
    return (((position - 1) % m_capacity) + m_capacity) % m_capacity;
}

template<class T>
constexpr ring_buffer<T>::ring_buffer(T* buffer, int capacity) noexcept
  : m_buffer(buffer)
  , m_capacity(static_cast<i32>(capacity))
{
    irt_assert(m_buffer);
    irt_assert(m_capacity > 0);
    irt_assert(capacity < INT32_MAX);
}

template<class T>
constexpr ring_buffer<T>::ring_buffer(ring_buffer&& rhs) noexcept
{
    m_buffer   = rhs.m_buffer;
    m_head     = rhs.m_head;
    m_tail     = rhs.m_tail;
    m_capacity = rhs.m_capacity;

    rhs.m_buffer   = nullptr;
    rhs.m_head     = 0;
    rhs.m_tail     = 0;
    rhs.m_capacity = 0;
}

template<class T>
constexpr ring_buffer<T>& ring_buffer<T>::operator=(ring_buffer&& rhs) noexcept
{
    if (this != &rhs) {
        clear();

        m_buffer   = rhs.m_buffer;
        m_head     = rhs.m_head;
        m_tail     = rhs.m_tail;
        m_capacity = rhs.m_capacity;

        rhs.m_buffer   = nullptr;
        rhs.m_head     = 0;
        rhs.m_tail     = 0;
        rhs.m_capacity = 0;
    }

    return *this;
}

template<class T>
constexpr ring_buffer<T>::~ring_buffer() noexcept
{
    clear();
}

template<class T>
constexpr void ring_buffer<T>::swap(ring_buffer& rhs) noexcept
{
    std::swap(m_buffer, rhs.m_buffer);
    std::swap(m_head, rhs.m_head);
    std::swap(m_tail, rhs.m_tail);
    std::swap(m_capacity, rhs.m_capacity);
}

template<class T>
constexpr void ring_buffer<T>::clear() noexcept
{
    if (!std::is_trivially_destructible_v<T>) {
        while (!empty())
            dequeue();
    }

    m_head = 0;
    m_tail = 0;
}

template<class T>
constexpr void ring_buffer<T>::reset(T* buffer, int capacity) noexcept
{
    irt_assert(buffer);
    irt_assert(capacity > 0);
    irt_assert(capacity < INT32_MAX);

    m_buffer   = buffer;
    m_capacity = static_cast<i32>(capacity);
}

template<class T>
template<typename... Args>
constexpr bool ring_buffer<T>::emplace_front(Args&&... args) noexcept
{
    if (full())
        return false;

    m_head = back(m_head);
    std::construct_at(&m_buffer[m_head], std::forward<Args>(args)...);

    return true;
}

template<class T>
template<typename... Args>
constexpr bool ring_buffer<T>::emplace_back(Args&&... args) noexcept
{
    if (full())
        return false;

    std::construct_at(&m_buffer[m_tail], std::forward<Args>(args)...);
    m_tail = advance(m_tail);

    return true;
}

template<class T>
constexpr bool ring_buffer<T>::push_front(const T& item) noexcept
{
    if (full())
        return false;

    m_head = back(m_head);
    std::construct_at(&m_buffer[m_head], item);

    return true;
}

template<class T>
constexpr void ring_buffer<T>::pop_front() noexcept
{
    if (!empty()) {
        std::destroy_at(&m_buffer[m_head]);
        m_head = advance(m_head);
    }
}

template<class T>
constexpr bool ring_buffer<T>::push_back(const T& item) noexcept
{
    if (full())
        return false;

    std::construct_at(&m_buffer[m_tail], item);
    m_tail = advance(m_tail);

    return true;
}

template<class T>
constexpr void ring_buffer<T>::pop_back() noexcept
{
    if (!empty()) {
        m_tail = back(m_tail);
        std::destroy_at(&m_buffer[m_tail]);
    }
}

template<class T>
constexpr void ring_buffer<T>::erase_after(iterator this_it) noexcept
{
    irt_assert(this_it.ring != nullptr);

    if (this_it == tail())
        return;

    while (this_it != tail())
        pop_back();
}

template<class T>
constexpr void ring_buffer<T>::erase_before(iterator this_it) noexcept
{
    irt_assert(this_it.ring != nullptr);

    if (this_it == head())
        return;

    auto it = head();
    while (it != head())
        pop_front();
}

template<class T>
template<typename... Args>
constexpr bool ring_buffer<T>::emplace_enqueue(Args&&... args) noexcept
{
    if (full())
        return false;

    std::construct_at(&m_buffer[m_tail], std::forward<Args>(args)...);
    m_tail = advance(m_tail);

    return true;
}

template<class T>
template<typename... Args>
constexpr void ring_buffer<T>::force_emplace_enqueue(Args&&... args) noexcept
{
    if (full())
        dequeue();

    std::construct_at(&m_buffer[m_tail], std::forward<Args>(args)...);
    m_tail = advance(m_tail);
}

template<class T>
constexpr bool ring_buffer<T>::enqueue(const T& item) noexcept
{
    if (full())
        return false;

    std::construct_at(&m_buffer[m_tail], item);
    m_tail = advance(m_tail);

    return true;
}

template<class T>
constexpr void ring_buffer<T>::force_enqueue(const T& item) noexcept
{
    if (full())
        dequeue();

    std::construct_at(&m_buffer[m_tail], item);
    m_tail = advance(m_tail);
}

template<class T>
constexpr void ring_buffer<T>::dequeue() noexcept
{
    if (!empty()) {
        std::destroy_at(&m_buffer[m_head]);
        m_head = advance(m_head);
    }
}

template<class T>
constexpr typename ring_buffer<T>::iterator ring_buffer<T>::head() noexcept
{
    return empty() ? iterator{ nullptr, 0 } : iterator{ this, m_head };
}

template<class T>
constexpr typename ring_buffer<T>::const_iterator ring_buffer<T>::head()
  const noexcept
{
    return empty() ? const_iterator{ nullptr, 0 }
                   : const_iterator{ this, m_head };
}

template<class T>
constexpr typename ring_buffer<T>::iterator ring_buffer<T>::tail() noexcept
{
    return empty() ? iterator{ nullptr, 0 } : iterator{ this, back(m_tail) };
}

template<class T>
constexpr typename ring_buffer<T>::const_iterator ring_buffer<T>::tail()
  const noexcept
{
    return empty() ? const_iterator{ nullptr, 0 }
                   : const_iterator{ this, back(m_tail) };
}

template<class T>
constexpr typename ring_buffer<T>::iterator ring_buffer<T>::end() noexcept
{
    return iterator{ nullptr, 0 };
}

template<class T>
constexpr typename ring_buffer<T>::const_iterator ring_buffer<T>::end()
  const noexcept
{
    return const_iterator{ nullptr, 0 };
}

template<class T>
constexpr T& ring_buffer<T>::front() noexcept
{
    irt_assert(!empty());

    return m_buffer[m_head];
}

template<class T>
constexpr const T& ring_buffer<T>::front() const noexcept
{
    irt_assert(!empty());

    return m_buffer[m_head];
}

template<class T>
constexpr T& ring_buffer<T>::back() noexcept
{
    irt_assert(!empty());

    return m_buffer[back(m_tail)];
}

template<class T>
constexpr const T& ring_buffer<T>::back() const noexcept
{
    irt_assert(!empty());

    return m_buffer[back(m_tail)];
}

template<class T>
constexpr bool ring_buffer<T>::empty() const noexcept
{
    return m_head == m_tail;
}

template<class T>
constexpr bool ring_buffer<T>::full() const noexcept
{
    return advance(m_tail) == m_head;
}

template<class T>
constexpr unsigned ring_buffer<T>::size() const noexcept
{
    return static_cast<sz>(ssize());
}

template<class T>
constexpr int ring_buffer<T>::ssize() const noexcept
{
    return (m_tail >= m_head) ? m_tail - m_head
                              : m_capacity - (m_head - m_tail);
}

template<class T>
constexpr int ring_buffer<T>::available() const noexcept
{
    return capacity() - size();
}

template<class T>
constexpr int ring_buffer<T>::capacity() const noexcept
{
    return m_capacity;
}

template<class T>
constexpr int ring_buffer<T>::index_from_begin(int idx) const noexcept
{
    // irt_assert(idx < ssize());

    return (m_head + idx) % m_capacity;
}

template<typename T>
hierarchy<T>::~hierarchy() noexcept
{
    remove_from_hierarchy();
}

template<typename T>
T* hierarchy<T>::id() const noexcept
{
    return m_id;
}

template<typename T>
void hierarchy<T>::set_id(T* id) noexcept
{
    m_id = id;
}

template<typename T>
bool hierarchy<T>::parented_by(const hierarchy& node) const noexcept
{
    if (m_parent == &node)
        return true;

    if (m_parent)
        return m_parent->parented_by(node);

    return false;
}

template<typename T>
void hierarchy<T>::parent_to(hierarchy& node) noexcept
{
    remove_from_parent();

    m_parent     = &node;
    m_sibling    = node.m_child;
    node.m_child = this;
}

template<typename T>
void hierarchy<T>::make_sibling_after(hierarchy& node) noexcept
{
    remove_from_parent();

    m_parent       = node.m_parent;
    m_sibling      = node.m_sibling;
    node.m_sibling = this;
}

template<typename T>
void hierarchy<T>::remove_from_parent() noexcept
{
    if (m_parent) {
        hierarchy<T>* prev = get_prior_sibling_node();

        if (prev) {
            prev->m_sibling = m_sibling;
        } else {
            m_parent->m_child = m_sibling;
        }
    }

    m_parent  = nullptr;
    m_sibling = nullptr;
}

template<typename T>
void hierarchy<T>::remove_from_hierarchy() noexcept
{
    hierarchy<T>* parent_node = m_parent;
    remove_from_parent();

    if (parent_node) {
        while (m_child) {
            hierarchy<T>* node = m_child;
            node->remove_from_parent();
            node->parent_to(*parent_node);
        }
    } else {
        while (m_child)
            m_child->remove_from_parent();
    }
}

template<typename T>
T* hierarchy<T>::get_parent() const noexcept
{
    return m_parent ? m_parent->m_id : nullptr;
}

template<typename T>
T* hierarchy<T>::get_child() const noexcept
{
    return m_child ? m_child->m_id : nullptr;
}

template<typename T>
T* hierarchy<T>::get_sibling() const noexcept
{
    return m_sibling ? m_sibling->m_id : nullptr;
}

template<typename T>
hierarchy<T>* hierarchy<T>::get_prior_sibling_node() const noexcept
{
    if (!m_parent || (m_parent->m_child == this))
        return nullptr;

    hierarchy<T>* prev;
    hierarchy<T>* node;

    node = m_parent->m_child;
    prev = nullptr;
    while ((node != this) && (node != nullptr)) {
        prev = node;
        node = node->m_sibling;
    }

    irt_assert(node == this &&
               "could not find node in parent's list of children");

    return prev;
}

template<typename T>
T* hierarchy<T>::get_prior_sibling() const noexcept
{
    hierarchy<T>* prior = get_prior_sibling_node();

    return prior ? prior->m_id : nullptr;
}

template<typename T>
T* hierarchy<T>::get_next() const noexcept
{
    if (m_child)
        return m_child->m_id;

    const hierarchy<T>* node = this;
    while (node && node->m_sibling == nullptr)
        node = node->m_parent;

    return node ? node->m_sibling->m_id : nullptr;
}

template<typename T>
T* hierarchy<T>::get_next_leaf() const noexcept
{
    if (m_child) {
        const hierarchy<T>* node = m_child;
        while (node->m_child)
            node = node->m_child;

        return node->m_id;
    }

    const hierarchy<T>* node = this;
    while (node && node->m_sibling == nullptr)
        node = node->m_parent;

    if (node) {
        node = node->m_sibling;
        while (node->m_child) {
            node = node->m_child;
        }
        return node->m_id;
    }

    return nullptr;
}

} // namespace irt

#endif
