// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_EXTENSION_HPP
#define ORG_VLEPROJECT_IRRITATOR_EXTENSION_HPP

#include <irritator/core.hpp>

#include <charconv>
#include <cstdint>
#include <forward_list>
#include <iterator>
#include <numeric>
#include <string_view>
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

/// Small exact fraction class (signed 32-bit integers), designed for
/// capturing a time step (dt) in a graphical interface without relying on
/// approximate floating-point representation. Always kept in reduced form
/// (GCD applied), denominator always strictly positive after construction.
class fraction
{
public:
    constexpr fraction() noexcept = default;

    constexpr fraction(std::int32_t num, std::int32_t den) noexcept
      : m_num(num)
      , m_den(den)
    {
        normalize();
    }

    constexpr fraction(std::int32_t whole) noexcept
      : m_num(whole)
      , m_den(1)
    {}

    constexpr std::int32_t numerator() const noexcept { return m_num; }
    constexpr std::int32_t denominator() const noexcept { return m_den; }

    constexpr double to_double() const noexcept
    {
        return static_cast<double>(m_num) / static_cast<double>(m_den);
    }

    constexpr explicit operator double() const noexcept { return to_double(); }

    constexpr fraction operator+(const fraction& o) const noexcept
    {
        return fraction(m_num * o.m_den + o.m_num * m_den, m_den * o.m_den);
    }

    constexpr fraction operator-(const fraction& o) const noexcept
    {
        return fraction(m_num * o.m_den - o.m_num * m_den, m_den * o.m_den);
    }

    constexpr fraction operator*(const fraction& o) const noexcept
    {
        return fraction(m_num * o.m_num, m_den * o.m_den);
    }

    constexpr fraction operator/(const fraction& o) const noexcept
    {
        return fraction(m_num * o.m_den, m_den * o.m_num);
    }

    constexpr fraction operator*(std::int32_t k) const noexcept
    {
        return fraction(m_num * k, m_den);
    }

    constexpr bool operator==(const fraction& o) const noexcept
    {
        return m_num == o.m_num && m_den == o.m_den;
    }

    constexpr auto operator<=>(const fraction& o) const noexcept
    {
        return (m_num * o.m_den) <=> (o.m_num * m_den);
    }

private:
    constexpr void normalize() noexcept
    {
        debug::ensure(m_den != 0);

        if (m_den < 0) {
            m_num = -m_num;
            m_den = -m_den;
        }

        const auto g = std::gcd(m_num < 0 ? -m_num : m_num, m_den);
        if (g > 1) {
            m_num /= g;
            m_den /= g;
        }
    }

    std::int32_t m_num = 0;
    std::int32_t m_den = 1;
};

/// Parse a string "num/den" and build a fraction if possible
inline expected<fraction> parse_fraction(std::string_view text) noexcept
{
    const auto slash = text.find('/');

    if (slash == std::string_view::npos) {
        std::int32_t num = 0;
        auto [ptr, ec] =
          std::from_chars(text.data(), text.data() + text.size(), num);
        if (ec != std::errc{} || ptr != text.data() + text.size())
            return make_error(static_cast<i16>(std::errc::invalid_argument),
                              category::generic);

        return fraction(num);
    }

    const auto num_part = text.substr(0, slash);
    const auto den_part = text.substr(slash + 1);

    std::int32_t num = 0;
    std::int32_t den = 0;

    auto [p1, ec1] =
      std::from_chars(num_part.data(), num_part.data() + num_part.size(), num);
    auto [p2, ec2] =
      std::from_chars(den_part.data(), den_part.data() + den_part.size(), den);

    if (ec1 != std::errc{} || ec2 != std::errc{} || den == 0 ||
        p1 != num_part.data() + num_part.size() ||
        p2 != den_part.data() + den_part.size())
        return make_error(static_cast<i16>(std::errc::invalid_argument),
                          category::generic);

    return fraction(num, den);
}

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
