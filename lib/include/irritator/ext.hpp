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

/// An efficient, type-erasing, owning callable with Small Buffer Optimization.
///
/// This class stores small lambdas and, function objects inline without
/// heap allocation. It is designed specifically for lambdas, not for function
/// pointers, munber function pointer nor virtual functions.
///
/// @tparam Fn Function signature (e.g., void(int, float)).
/// @tparam Size Number of bytes for inline storage (minimum 1).
/// @tparam Align Alignment for inline storage (power of two, minimum 1).
///
/// Requirements:
/// - Callable must fit in Size bytes
/// - Callable must be nothrow movable (for noexcept guarantee)
///
/// Example:
///   lambda_function f([x = 42](int y) {
///       return x + y;
///   });
///
///   int result = f(10);  // returns 52
template<typename Sig,
         std::size_t Size  = 64,
         std::size_t Align = alignof(std::max_align_t)>
class lambda_function;

// Traits to extract R(Args...) signature from &L::operator()
template<typename T>
struct lambda_signature; // default is undefined

// no-noexcept variants
template<typename C, typename R, typename... Args>
struct lambda_signature<R (C::*)(Args...)> {
    using type = R(Args...);
};
template<typename C, typename R, typename... Args>
struct lambda_signature<R (C::*)(Args...) const> {
    using type = R(Args...);
};
template<typename C, typename R, typename... Args>
struct lambda_signature<R (C::*)(Args...) volatile> {
    using type = R(Args...);
};
template<typename C, typename R, typename... Args>
struct lambda_signature<R (C::*)(Args...) const volatile> {
    using type = R(Args...);
};

// ref-qualifiers variants
template<typename C, typename R, typename... Args>
struct lambda_signature<R (C::*)(Args...) &> {
    using type = R(Args...);
};
template<typename C, typename R, typename... Args>
struct lambda_signature<R (C::*)(Args...) const&> {
    using type = R(Args...);
};
template<typename C, typename R, typename... Args>
struct lambda_signature<R (C::*)(Args...) volatile&> {
    using type = R(Args...);
};
template<typename C, typename R, typename... Args>
struct lambda_signature<R (C::*)(Args...) const volatile&> {
    using type = R(Args...);
};
template<typename C, typename R, typename... Args>
struct lambda_signature<R (C::*)(Args...) &&> {
    using type = R(Args...);
};
template<typename C, typename R, typename... Args>
struct lambda_signature<R (C::*)(Args...) const&&> {
    using type = R(Args...);
};
template<typename C, typename R, typename... Args>
struct lambda_signature<R (C::*)(Args...) volatile&&> {
    using type = R(Args...);
};
template<typename C, typename R, typename... Args>
struct lambda_signature<R (C::*)(Args...) const volatile&&> {
    using type = R(Args...);
};

// noexcept variants
template<typename C, typename R, typename... Args>
struct lambda_signature<R (C::*)(Args...) noexcept> {
    using type = R(Args...);
};
template<typename C, typename R, typename... Args>
struct lambda_signature<R (C::*)(Args...) const noexcept> {
    using type = R(Args...);
};
template<typename C, typename R, typename... Args>
struct lambda_signature<R (C::*)(Args...) volatile noexcept> {
    using type = R(Args...);
};
template<typename C, typename R, typename... Args>
struct lambda_signature<R (C::*)(Args...) const volatile noexcept> {
    using type = R(Args...);
};

// no-noexcept and ref-qualifiers variants
template<typename C, typename R, typename... Args>
struct lambda_signature<R (C::*)(Args...) & noexcept> {
    using type = R(Args...);
};
template<typename C, typename R, typename... Args>
struct lambda_signature<R (C::*)(Args...) const & noexcept> {
    using type = R(Args...);
};
template<typename C, typename R, typename... Args>
struct lambda_signature<R (C::*)(Args...) volatile & noexcept> {
    using type = R(Args...);
};
template<typename C, typename R, typename... Args>
struct lambda_signature<R (C::*)(Args...) const volatile & noexcept> {
    using type = R(Args...);
};
template<typename C, typename R, typename... Args>
struct lambda_signature<R (C::*)(Args...) && noexcept> {
    using type = R(Args...);
};
template<typename C, typename R, typename... Args>
struct lambda_signature<R (C::*)(Args...) const && noexcept> {
    using type = R(Args...);
};
template<typename C, typename R, typename... Args>
struct lambda_signature<R (C::*)(Args...) volatile && noexcept> {
    using type = R(Args...);
};
template<typename C, typename R, typename... Args>
struct lambda_signature<R (C::*)(Args...) const volatile && noexcept> {
    using type = R(Args...);
};

template<typename L>
using deduce_signature_t = typename lambda_signature<
  decltype(&std::remove_reference_t<L>::operator())>::type;

template<typename R, typename... Args, std::size_t Size, std::size_t Align>
class lambda_function<R(Args...), Size, Align>
{
    static_assert(Align != 0, "Align must be non-zero");
    static_assert((Align & (Align - 1)) == 0, "Align must be power of two");

    alignas(Align) std::byte m_storage[Size];

    struct vtable_t {
        R (*invoke)(const void*, Args&&...) noexcept;
        void (*destroy)(void*) noexcept;
        void (*move_construct)(void* dst, void* src) noexcept;
    } m_vtbl{};

    bool m_engaged = false;

    template<typename L>
    static R invoke_fn(const void* p, Args&&... args) noexcept
    {
        auto const* lp = static_cast<L const*>(p);

        if constexpr (std::is_void_v<R>) {
            (*lp)(std::forward<Args>(args)...);
        } else {
            return (*lp)(std::forward<Args>(args)...);
        }
    }

    template<typename L>
    static void destroy_fn(void* p) noexcept
    {
        static_cast<L*>(p)->~L();
    }

    template<typename L>
    static void move_construct_fn(void* dst, void* src) noexcept
    {
        auto* s = static_cast<L*>(src);

        ::new (dst) L(std::move(*s));
        destroy_fn<L>(src);
    }

    template<typename T>
    static constexpr bool is_lambda_like_v =
      std::is_class_v<T> && requires { &T::operator(); };

public:
    using result_type = R;

    constexpr lambda_function() noexcept = default;

    lambda_function(const lambda_function&)            = delete;
    lambda_function& operator=(const lambda_function&) = delete;

    constexpr lambda_function(lambda_function&& other) noexcept
    {
        if (other.m_engaged) {
            other.m_vtbl.move_construct(m_storage, other.m_storage);
            m_vtbl          = other.m_vtbl;
            m_engaged       = true;
            other.m_engaged = false;
        }
    }

    constexpr lambda_function& operator=(lambda_function&& other) noexcept
    {
        if (this != &other) {
            reset();
            if (other.m_engaged) {
                other.m_vtbl.move_construct(m_storage, other.m_storage);
                m_vtbl          = other.m_vtbl;
                m_engaged       = true;
                other.m_engaged = false;
            }
        }
        return *this;
    }

    ~lambda_function() { reset(); }

    template<typename L,
             typename T                                 = std::decay_t<L>,
             std::enable_if_t<is_lambda_like_v<T>, int> = 0>
    constexpr lambda_function(L&& l) noexcept
    {
        emplace(std::forward<L>(l));
    }

    template<typename L,
             typename T                                 = std::decay_t<L>,
             std::enable_if_t<is_lambda_like_v<T>, int> = 0>
    constexpr void emplace(L&& l) noexcept
    {
        using U = std::decay_t<L>;

        static_assert(sizeof(U) <= Size, "Too small buffer for lambda");
        static_assert(alignof(U) <= Align, "Too large alignement for lambda");

        reset();
        ::new (m_storage) U(std::forward<L>(l));

        m_vtbl =
          vtable_t{ &invoke_fn<U>, &destroy_fn<U>, &move_construct_fn<U> };
        m_engaged = true;
    }

    constexpr R operator()(Args... args) const noexcept
    {
        debug::ensure(m_engaged);

        return m_vtbl.invoke(m_storage, std::forward<Args>(args)...);
    }

    constexpr explicit operator bool() const noexcept { return m_engaged; }

    constexpr void reset() noexcept
    {
        if (m_engaged) {
            m_vtbl.destroy(m_storage);
            m_engaged = false;
        }
    }
};

/// Deduction guide for lambda_function
///
/// The buffer size is fixed a 64 bytes with maximum alignment. Use the @a
/// make_lambda function to be able to fix the size and alignment.
template<typename L>
lambda_function(L)
  -> lambda_function<deduce_signature_t<L>, 64, alignof(std::max_align_t)>;

/// Helper function to create a lambda_function with custom size and alignment.
template<std::size_t Size,
         std::size_t Align = alignof(std::max_align_t),
         typename L>
auto make_lambda(L&& l) noexcept
  -> lambda_function<deduce_signature_t<L>, Size, Align>
{
    lambda_function<deduce_signature_t<L>, Size, Align> f(std::forward<L>(l));
    return f;
}

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

//! Function object for performing comparisons. The main template
//! invokes operator< on type T. If T is an enumeration, a cast
//! to the @c std::underlying_type_t<T> is used before the
//! comparison.
template<class T>
struct table_less {
    constexpr auto operator()(const T& lhs, const T& rhs) const noexcept -> bool
    {
        if constexpr (std::is_enum_v<T>) {
            using type = std::underlying_type_t<T>;

            return std::less<type>{}(static_cast<type>(lhs),
                                     static_cast<type>(rhs));
        } else {
            return std::less{}(lhs, rhs);
        }
    }
};

//! @brief A helper container to store Identifier -> T relation.
//! @tparam Identifier Any integer or enum type.
//! @tparam T Any type (trivial or not).
template<typename Identifier,
         typename T,
         class Compare = irt::table_less<Identifier>,
         typename A    = allocator<new_delete_memory_resource>>
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
    };

    struct value_type_compare {
        constexpr auto operator()(const auto& left, const auto& right) noexcept
          -> bool
        {
            if constexpr (std::is_same_v<std::decay_t<decltype(left)>,
                                         value_type>) {
                if constexpr (std::is_same_v<std::decay_t<decltype(right)>,
                                             value_type>)
                    return identifier_compare{}(left.id, right.id);
                else
                    return identifier_compare{}(left.id, right);
            } else {
                if constexpr (std::is_same_v<std::decay_t<decltype(right)>,
                                             value_type>)
                    return identifier_compare{}(left, right.id);
                else
                    return identifier_compare{}(left, right);
            }
        }
    };

    using container_type     = vector<value_type, A>;
    using size_type          = typename container_type::size_type;
    using reference          = typename container_type::reference;
    using const_reference    = typename container_type::const_reference;
    using pointer            = typename container_type::pointer;
    using const_pointer      = typename container_type::const_pointer;
    using identifier_compare = Compare;

    container_type data;

    constexpr table() noexcept = default;
    constexpr table(std::integral auto size, const reserve_tag_t) noexcept;
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
 * A string_buffer stores a forward list of chunks of memory to store
 * strings. Users can allocate new strings, but the deletion of strings is
 * forgotten. Use this class when storing new allocated strings is more
 * important than replacing or deleting.
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
     * Appends an @a std::string_view into the underlying buffer and returns
     * a new @a std::string_view to this new chunk of characters. If
     * necessary, a new @a value_type or chunk is allocated to store more
     * strings.
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

template<typename Identifier, typename T, class Compare, typename A>
table<Identifier, T, Compare, A>::value_type::value_type(
  Identifier id_,
  const T&   value_) noexcept
  : id(id_)
  , value(value_)
{}

template<typename Identifier, typename T, class Compare, typename A>
constexpr table<Identifier, T, Compare, A>::table(
  std::integral auto  size,
  const reserve_tag_t tag) noexcept
  : data{ size, tag }
{}

template<typename Identifier, typename T, class Compare, typename A>
constexpr void table<Identifier, T, Compare, A>::set(const Identifier id,
                                                     const T& value) noexcept
{
    if (auto* value_found = get(id); value_found) {
        *value_found = value;
    } else {
        data.emplace_back(id, value);
        sort();
    }
}

template<typename Identifier, typename T, class Compare, typename A>
constexpr T* table<Identifier, T, Compare, A>::get(Identifier id) noexcept
{
    const auto view = data.view();
    const auto it =
      binary_find(view.begin(), view.end(), id, value_type_compare{});

    return it == view.end() ? nullptr : &it->value;
}

template<typename Identifier, typename T, class Compare, typename A>
constexpr const T* table<Identifier, T, Compare, A>::get(
  Identifier id) const noexcept
{
    const auto view = data.view();
    const auto it =
      binary_find(view.begin(), view.end(), id, value_type_compare{});

    return it == view.end() ? nullptr : &it->value;
}

template<typename Identifier, typename T, class Compare, typename A>
template<typename U>
constexpr T* table<Identifier, T, Compare, A>::get(U id) noexcept
{
    const auto view = data.view();
    const auto it =
      binary_find(view.begin(), view.end(), id, value_type_compare{});

    return it == view.end() ? nullptr : &it->value;
}

template<typename Identifier, typename T, class Compare, typename A>
template<typename U>
constexpr const T* table<Identifier, T, Compare, A>::get(U id) const noexcept
{
    const auto view = data.view();
    auto it = binary_find(view.begin(), view.end(), id, value_type_compare{});

    return it == view.end() ? nullptr : &it->value;
}

template<typename Identifier, typename T, class Compare, typename A>
constexpr void table<Identifier, T, Compare, A>::erase(Identifier id) noexcept
{
    const auto view = data.view();
    auto it = binary_find(view.begin(), view.end(), id, value_type_compare{});

    if (it != view.end())
        data.erase(std::distance(view.begin(), it));
}

template<typename Identifier, typename T, class Compare, typename A>
constexpr void table<Identifier, T, Compare, A>::sort() noexcept
{
    if (data.size() > 1) {
        const auto view = data.view();
        std::sort(view.begin(), view.end(), value_type_compare{});
    }
}

template<typename Identifier, typename T, class Compare, typename A>
constexpr unsigned table<Identifier, T, Compare, A>::size() const noexcept
{
    return data.size();
}

template<typename Identifier, typename T, class Compare, typename A>
constexpr int table<Identifier, T, Compare, A>::ssize() const noexcept
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
