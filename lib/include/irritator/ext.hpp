// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_EXTENSION_HPP
#define ORG_VLEPROJECT_IRRITATOR_EXTENSION_HPP

#include <irritator/core.hpp>

#include <forward_list>

#include <iterator>
#include <type_traits>
#include <utility>

namespace irt {

/// An efficient, type-erasing, onwning callable. This is intended for use as
/// attribute in class or struct.
///
/// This class owns the callable, so it is safe to store a small_function.
/// @c tparam Size number of bytes to be used. Requires 1 at minimum.
template<size_t Size, typename Fn>
class small_function;

template<size_t Size, typename Ret, typename... Params>
class small_function<Size, Ret(Params...)>
{
public:
    small_function() noexcept = default;

    small_function(const small_function& other) noexcept
    {
        if (other) {
            other.manager(
              std::data(data), std::data(other.data), Operation::Clone);
            invoker = other.invoker;
            manager = other.manager;
        }
    }

    small_function& operator=(const small_function& other) noexcept
    {
        if (this != &other)
            small_function(other).swap(*this);

        return *this;
    }

    small_function(small_function&& other) noexcept { other.swap(*this); }

    small_function& operator=(small_function&& other) noexcept
    {
        if (this != &other)
            small_function(std::move(other)).swap(*this);

        return *this;
    }

    small_function& operator=(std::nullptr_t) noexcept
    {
        if (manager) {
            manager(&data, nullptr, Operation::Destroy);
            manager = nullptr;
            invoker = nullptr;
        }
        return *this;
    }

    template<typename F>
    small_function(F&& f) noexcept
    {
        using f_type = std::decay_t<F>;

        static_assert(sizeof(f_type) <= Size, "storage too small");

        new (std::data(data)) f_type(std::forward<f_type>(f));
        invoker = &invoke<f_type>;
        manager = &manage<f_type>;
    }

    template<typename F>
    small_function& operator=(F&& f) noexcept
    {
        small_function(std::forward<F>(f)).swap(*this);
        return *this;
    }

    template<typename F>
    small_function& operator=(std::reference_wrapper<F> f)
    {
        small_function(f).swap(*this);
        return *this;
    }

    ~small_function() noexcept
    {
        if (manager)
            manager(&data, nullptr, Operation::Destroy);
    }

    void swap(small_function& other) noexcept
    {
        std::swap(data, other.data);
        std::swap(manager, other.manager);
        std::swap(invoker, other.invoker);
    }

    explicit operator bool() const noexcept { return !!manager; }

    Ret operator()(Params... args) noexcept
    {
        debug::ensure(invoker);
        return invoker(&data, std::forward<Params>(args)...);
    }

private:
    enum class Operation { Clone, Destroy };

    using Invoker = Ret (*)(void*, Params&&...);
    using Manager = void (*)(void*, const void*, Operation);
    using Storage = std::byte[Size];

    template<typename F>
    static Ret invoke(void* data, Params&&... args)
    {
        F& f = *static_cast<F*>(data);
        return f(std::forward<Params>(args)...);
    }

    template<typename F>
    static void manage(void* dest, const void* src, Operation op)
    {
        switch (op) {
        case Operation::Clone:
            new (dest) F(*reinterpret_cast<const F*>(src));
            break;
        case Operation::Destroy:
            static_cast<F*>(dest)->~F();
            break;
        }
    }

    Storage data{};
    Invoker invoker = nullptr;
    Manager manager = nullptr;
};

//! An efficient, type-erasing, non-owning reference to a callable. This is
//! intended for use as the type of a function parameter that is not used
//! after the function in question returns.
//!
//! This class does not own the callable, so it is not in general safe to
//! store a function_ref.
template<typename Fn>
class function_ref;

template<typename Ret, typename... Params>
class function_ref<Ret(Params...) noexcept>
{
    Ret (*callback)(void* callable, Params... params) = nullptr;
    void* callable                                    = nullptr;

    template<typename Callable>
    static Ret callback_fn(void* callable, Params... params) noexcept
    {
        return (*reinterpret_cast<Callable*>(callable))(
          std::forward<Params>(params)...);
    }

public:
    function_ref() = default;
    function_ref(std::nullptr_t) {}

    template<typename Callable>
    function_ref(
      Callable&& callable,
      // This is not the copy-constructor.
      std::enable_if_t<!std::is_same<std::remove_cvref_t<Callable>,
                                     function_ref>::value>* = nullptr,
      // Functor must be callable and return a suitable type.
      std::enable_if_t<std::is_void<Ret>::value ||
                       std::is_convertible<decltype(std::declval<Callable>()(
                                             std::declval<Params>()...)),
                                           Ret>::value>* = nullptr)
      : callback(callback_fn<std::remove_reference_t<Callable>>)
      , callable(&callable)
    {}

    Ret operator()(Params... params) const
    {
        debug::ensure(callable);
        return callback(callable, std::forward<Params>(params)...);
    }

    explicit operator bool() const { return callback; }
};

//! @brief A helper container to store Identifier -> T relation.
//! @tparam Identifier Any integer or enum type.
//! @tparam T Any type (trivial or not).
template<typename Identifier,
         typename T,
         typename A = allocator<new_delete_memory_resource>>
    requires(std::three_way_comparable<Identifier>)
class table
{
public:
    struct value_type {
        value_type() noexcept = default;
        value_type(Identifier id, const T& value) noexcept;

        template<typename U>
        value_type(std::is_constructible<Identifier, U> id_,
                   const T&                             value_) noexcept
          : id{ id_ }
          , value{ value_ }
        {}

        Identifier id;
        T          value;

        auto operator<=>(const value_type&) const = default;

        auto operator<=>(const Identifier& other) const { return id <=> other; }
    };

    using container_type  = vector<value_type, A>;
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

    template<typename U>
    constexpr T* get(U id) noexcept;

    template<typename U>
    constexpr const T* get(U id) const noexcept;

    constexpr void erase(Identifier id) noexcept;
    constexpr void sort() noexcept;

    constexpr unsigned size() const noexcept;
    constexpr int      ssize() const noexcept;
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

/**
 * @brief Managed to stores large string buffer.
 *
 * A string_buffer stores a forward list of chunks of memory to store strings.
 * Users can allocate new strings, but the deletion of strings is forgotten.
 * Use this class when storing new allocated strings is more important than
 * replacing or deleting.
 */
class string_buffer
{
public:
    constexpr static inline std::size_t string_buffer_node_length = 1024 * 1024;

    string_buffer() noexcept = default;

    string_buffer(const string_buffer&) noexcept            = delete;
    string_buffer& operator=(const string_buffer&) noexcept = delete;

    string_buffer(string_buffer&&) noexcept            = default;
    string_buffer& operator=(string_buffer&&) noexcept = default;

    /**
     * Clear the underlying container using the @a container_type::clear()
     * function. After the call all @a std::string_view are invalid.
     *
     * @attention Any use of @a std::string_view after @a clear() is UB.
     */
    void clear() noexcept;

    /**
     * Appends an @a std::string_view into the underlying buffer and returns a
     * new @a std::string_view to this new chunk of characters. If necessary, a
     * new @a value_type or chunk is allocated to store more strings.
     *
     * @param str A @a std::string_view to copy into the buffer. The @a str
     *  string must be lower than `string_buffer_node_length`.
     */
    std::string_view append(std::string_view str) noexcept;

    /**
     * Computes and returns the number of `value_type` allocated.
     */
    std::size_t size() const noexcept;

private:
    using value_type     = std::array<char, string_buffer_node_length>;
    using container_type = std::forward_list<value_type>;

    // Allocate a new @c value_type buffer in front of the last allocated
    // buffer.
    void do_alloc() noexcept;

    container_type m_container;
    std::size_t    m_position = { 0 };
};

inline std::string_view string_buffer::append(std::string_view str) noexcept
{
    debug::ensure(str.size() < string_buffer_node_length);

    if (str.empty() or str.size() >= string_buffer_node_length)
        return std::string_view();

    if (m_container.empty() ||
        str.size() + m_position > string_buffer_node_length)
        do_alloc();

    std::size_t position = m_position;
    m_position += str.size();

    char* buffer = m_container.front().data() + position;

    std::copy_n(str.data(), str.size(), buffer);

    return std::string_view(buffer, str.size());
}

inline void string_buffer::clear() noexcept
{
    m_container.clear();
    m_position = 0;
}

inline std::size_t string_buffer::size() const noexcept
{
    return static_cast<std::size_t>(
      std::distance(m_container.cbegin(), m_container.cend()));
}

inline void string_buffer::do_alloc() noexcept
{
    m_container.emplace_front();
    m_position = 0;
}

/*****************************************************************************
 *
 * Containers implementation
 *
 ****************************************************************************/

template<typename Identifier, typename T, typename A>
    requires(std::three_way_comparable<Identifier>)
table<Identifier, T, A>::value_type::value_type(Identifier id_,
                                                const T&   value_) noexcept
  : id(id_)
  , value(value_)
{}

template<typename Identifier, typename T, typename A>
    requires(std::three_way_comparable<Identifier>)
constexpr void table<Identifier, T, A>::set(const Identifier id,
                                            const T&         value) noexcept
{
    if (auto* value_found = get(id); value_found) {
        *value_found = value;
    } else {
        data.emplace_back(id, value);
        sort();
    }
}

template<typename Identifier, typename T, typename A>
    requires(std::three_way_comparable<Identifier>)
constexpr T* table<Identifier, T, A>::get(Identifier id) noexcept
{
    auto it = binary_find(
      data.begin(), data.end(), id, [](auto left, auto right) noexcept -> bool {
          if constexpr (std::is_same_v<decltype(left), Identifier>)
              return left < right.id;
          else
              return left.id < right;
      });

    return it == data.end() ? nullptr : &it->value;
}

template<typename Identifier, typename T, typename A>
    requires(std::three_way_comparable<Identifier>)
constexpr const T* table<Identifier, T, A>::get(Identifier id) const noexcept
{
    auto it = binary_find(
      data.begin(), data.end(), id, [](auto left, auto right) noexcept -> bool {
          if constexpr (std::is_same_v<decltype(left), Identifier>)
              return left < right.id;
          else
              return left.id < right;
      });

    return it == data.end() ? nullptr : &it->value;
}

template<typename Identifier, typename T, typename A>
    requires(std::three_way_comparable<Identifier>)
template<typename U>
constexpr T* table<Identifier, T, A>::get(U id) noexcept
{
    auto it = binary_find(
      data.begin(), data.end(), id, [](auto left, auto right) noexcept -> bool {
          if constexpr (std::is_same_v<decltype(left), value_type>)
              return left.id < right;
          else
              return left < right.id.sv();
      });

    return it == data.end() ? nullptr : &it->value;
}

template<typename Identifier, typename T, typename A>
    requires(std::three_way_comparable<Identifier>)
template<typename U>
constexpr const T* table<Identifier, T, A>::get(U id) const noexcept
{
    auto it = binary_find(
      data.begin(), data.end(), id, [](auto left, auto right) noexcept -> bool {
          if constexpr (std::is_same_v<decltype(left), value_type>)
              return left.id < right;
          else
              return left < right.id.sv();
      });

    return it == data.end() ? nullptr : &it->value;
}

template<typename Identifier, typename T, typename A>
    requires(std::three_way_comparable<Identifier>)
constexpr void table<Identifier, T, A>::erase(Identifier id) noexcept
{
    auto it = binary_find(
      data.begin(), data.end(), id, [](auto left, auto right) noexcept -> bool {
          if constexpr (std::is_same_v<decltype(left), Identifier>)
              return left < right.id.sv();
          else
              return left.id < right;
      });

    if (it != data.end()) {
        data.erase(it);
        sort();
    }
}

template<typename Identifier, typename T, typename A>
    requires(std::three_way_comparable<Identifier>)
constexpr void table<Identifier, T, A>::sort() noexcept
{
    if (data.size() > 1)
        std::sort(data.begin(),
                  data.end(),
                  [](const auto& left, const auto& right) noexcept {
                      return left.id < right.id;
                  });
}

template<typename Identifier, typename T, typename A>
    requires(std::three_way_comparable<Identifier>)
constexpr unsigned table<Identifier, T, A>::size() const noexcept
{
    return data.size();
}

template<typename Identifier, typename T, typename A>
    requires(std::three_way_comparable<Identifier>)
constexpr int table<Identifier, T, A>::ssize() const noexcept
{
    return data.ssize();
}

// class hierarchy
//

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

    debug::ensure(node == this &&
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
