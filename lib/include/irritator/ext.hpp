// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_EXTENSION_HPP
#define ORG_VLEPROJECT_IRRITATOR_EXTENSION_HPP

#include <irritator/core.hpp>

#include <utility>

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
    using index_type      = typename container_type::index_type;
    using iterator        = typename container_type::iterator;
    using const_iterator  = typename container_type::const_iterator;
    using reference       = typename container_type::reference;
    using const_reference = typename container_type::const_reference;
    using pointer         = typename container_type::pointer;
    using const_pointer   = typename container_type::const_pointer;

    container_type data;

    constexpr void     set(Identifier id, T value) noexcept;
    constexpr T*       get(Identifier id) noexcept;
    constexpr const T* get(Identifier id) const noexcept;
    constexpr void     sort() noexcept;
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

    static_assert(std::is_nothrow_move_constructible_v<T> ||
                  std::is_trivially_move_constructible_v<T> ||
                  std::is_nothrow_copy_constructible_v<T> ||
                  std::is_trivially_copy_constructible_v<T>);

    using size_type = small_storage_size_t<length>;

private:
    std::byte m_buffer[length * sizeof(T)];
    size_type m_size;

public:
    using index_type      = i32;
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

    constexpr status init(i32 default_size) noexcept;
    constexpr void   clear() noexcept;

    constexpr reference       front() noexcept;
    constexpr const_reference front() const noexcept;
    constexpr reference       back() noexcept;
    constexpr const_reference back() const noexcept;

    constexpr T*       data() noexcept;
    constexpr const T* data() const noexcept;

    constexpr reference       operator[](const index_type index) noexcept;
    constexpr const_reference operator[](const index_type index) const noexcept;

    constexpr iterator       begin() noexcept;
    constexpr const_iterator begin() const noexcept;
    constexpr iterator       end() noexcept;
    constexpr const_iterator end() const noexcept;

    constexpr bool can_alloc(int number = 1) noexcept;
    constexpr sz   size() const noexcept;
    constexpr i32  ssize() const noexcept;
    constexpr sz   capacity() const noexcept;
    constexpr bool empty() const noexcept;
    constexpr bool full() const noexcept;

    template<typename... Args>
    constexpr reference emplace_back(Args&&... args) noexcept;
    constexpr void      pop_back() noexcept;
    constexpr void      swap_pop_back(index_type index) noexcept;
};

template<class T, i32 Size>
class ring_buffer
{
    static_assert(std::is_nothrow_constructible_v<T> ||
                  std::is_nothrow_move_constructible_v<T>);

private:
    T   buffer[Size];
    i32 head   = 0;
    i32 tail   = 0;
    i32 number = 0;

public:
    constexpr ring_buffer() noexcept = default;
    constexpr ~ring_buffer() noexcept;

    constexpr void clear() noexcept;

    template<typename... Args>
    constexpr bool emplace_enqueue(Args&&... args) noexcept;

    constexpr bool enqueue(const T& item) noexcept;
    constexpr T    dequeue() noexcept;

    constexpr T&       front() noexcept;
    constexpr const T& front() const noexcept;

    constexpr size_t size() const noexcept;
    constexpr i32    ssize() const noexcept;
    constexpr bool   empty() const noexcept;
    constexpr bool   full() const noexcept;
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
constexpr void table<Identifier, T>::set(Identifier id, T value) noexcept
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
constexpr void table<Identifier, T>::sort() noexcept
{
    if (data.size() > 1)
        std::sort(data.begin(),
                  data.end(),
                  [](const auto& left, const auto& right) noexcept {
                      return left.id < right.id;
                  });
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
    std::copy_n(other.data(), other.m_size, data());
}

template<typename T, sz length>
constexpr small_vector<T, length>::~small_vector() noexcept
{
    clear();
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
constexpr status small_vector<T, length>::init(i32 default_size) noexcept
{
    static_assert(std::is_nothrow_default_constructible_v<T> ||
                    std::is_trivially_default_constructible_v<T>,
                  "T must be nothrow or trivially default constructible to use "
                  "init() function");

    irt_return_if_fail(default_size > 0 &&
                         default_size < static_cast<i32>(length),
                       status::vector_init_capacity_error);

    for (i32 i = 0; i < default_size; ++i)
        new (&(m_buffer[i])) T{};

    m_size = default_size;
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
small_vector<T, length>::operator[](const index_type index) noexcept
{
    irt_assert(index >= 0);
    irt_assert(index < m_size);

    return data()[index];
}

template<typename T, sz length>
constexpr typename small_vector<T, length>::const_reference
small_vector<T, length>::operator[](const index_type index) const noexcept
{
    irt_assert(index >= 0);
    irt_assert(index < m_size);

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
constexpr sz small_vector<T, length>::size() const noexcept
{
    return m_size;
}

template<typename T, sz length>
constexpr i32 small_vector<T, length>::ssize() const noexcept
{
    return static_cast<i32>(m_size);
}

template<typename T, sz length>
constexpr sz small_vector<T, length>::capacity() const noexcept
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
    static_assert(std::is_nothrow_destructible_v<T> ||
                    std::is_trivially_destructible_v<T>,
                  "T must be nothrow or trivially destructible to use "
                  "clear() function");

    // if constexpr (!std::is_trivially_destructible_v<T>) {
    for (i32 i = 0; i != m_size; ++i)
        data()[i].~T();
    // }

    m_size = 0;
}

template<typename T, sz length>
constexpr bool small_vector<T, length>::can_alloc(int number) noexcept
{
    return static_cast<i64>(length) - static_cast<i64>(m_size) >=
           static_cast<i64>(number);
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

    new (&(data()[m_size])) T(std::forward<Args>(args)...);

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
        if constexpr (!std::is_trivially_destructible_v<T>)
            data()[m_size - 1].~T();

        --m_size;
    }
}

template<typename T, sz length>
constexpr void small_vector<T, length>::swap_pop_back(index_type index) noexcept
{
    irt_assert(index >= 0 && index < m_size);

    if (index == m_size - 1) {
        pop_back();
    } else {
        if constexpr (std::is_trivially_destructible_v<T>) {
            data()[index] = data()[m_size - 1];
            pop_back();
        } else if constexpr (std::is_nothrow_swappable_v<T>) {
            using std::swap;

            swap(data()[index], data()[m_size - 1]);
            pop_back();
        } else {
            data()[index] = data()[m_size - 1];
            pop_back();
        }
    }
}

template<class T, i32 Size>
constexpr ring_buffer<T, Size>::~ring_buffer() noexcept
{
    clear();
}

template<class T, i32 Size>
constexpr void ring_buffer<T, Size>::clear() noexcept
{
    while (!empty())
        dequeue();

    head   = 0;
    tail   = 0;
    number = 0;
}

template<class T, i32 Size>
template<typename... Args>
constexpr bool ring_buffer<T, Size>::emplace_enqueue(Args&&... args) noexcept
{
    if (full())
        return false;

    new (&buffer[tail]) T(std::forward<Args>(args)...);
    tail = (tail + 1) % Size;
    ++number;

    return true;
}

template<class T, i32 Size>
constexpr bool ring_buffer<T, Size>::enqueue(const T& item) noexcept
{
    if (full())
        return false;

    buffer[tail] = item;
    tail         = (tail + 1) % Size;
    ++number;

    return true;
}

template<class T, i32 Size>
constexpr T ring_buffer<T, Size>::dequeue() noexcept
{
    irt_assert(!empty());

    T item = buffer[head];

    if constexpr (!std::is_trivially_destructible_v<T>)
        buffer[head].~T();

    head = (head + 1) % Size;
    --number;

    return item;
}

template<class T, i32 Size>
constexpr T& ring_buffer<T, Size>::front() noexcept
{
    return buffer[head];
}
template<class T, i32 Size>
constexpr const T& ring_buffer<T, Size>::front() const noexcept
{
    return buffer[head];
}

template<class T, i32 Size>
constexpr bool ring_buffer<T, Size>::empty() const noexcept
{
    return number == 0;
}

template<class T, i32 Size>
constexpr bool ring_buffer<T, Size>::full() const noexcept
{
    return number == Size;
}

template<class T, i32 Size>
constexpr size_t ring_buffer<T, Size>::size() const noexcept
{
    return static_cast<sz>(ssize());
}

template<class T, i32 Size>
constexpr i32 ring_buffer<T, Size>::ssize() const noexcept
{
    return (tail >= head) ? tail - head : Size - head - tail;
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

namespace detail {

// inline status
// flat_merge_create_models(none& none_mdl,
//                          const data_array<component, component_id>&
//                          components, const data_array<model, model_id>&
//                          component_models, simulation& sim)
// {
//     auto* compo = components.try_to_get(none_mdl.id);
//     if (!compo)
//         return status::success;

//     auto& c = *compo;

//     // For each none models in the component, we call recursively the
//     // flat_merge_create_model function to flattend the hierarchy of models
//     and
//     // remove the none models. The new simulation models are built from leaf.

//     model* from_c = nullptr;
//     while (c.models.next(from_c)) {
//         if (from_c->type != dynamics_type::none)
//             continue;

//         irt_return_if_bad(flat_merge_create_models(
//           get_dyn<none>(*from_c), components, component_models, sim));
//     }

//     // For each models (none exclude) in the component, we duplicate the
//     model
//     // into the simulation and we fill the dictionary component model ->
//     // simulation model to prepare connections between models.

//     from_c = nullptr;
//     while (c.models.next(from_c)) {
//         if (from_c->type == dynamics_type::none)
//             continue;

//         irt_return_if_fail(sim.can_alloc(),
//                            status::simulation_not_enough_model);

//         auto& mdl = sim.models.alloc();
//         auto id = sim.models.get_id(mdl);
//         mdl.type = from_c->type;
//         mdl.handle = nullptr;

//         // @TODO Need a new status::model_none_not_enough_memory
//         irt_return_if_fail(none_mdl.dict.try_emplace_back(
//                              c.models.get_id(from_c), id) != nullptr,
//                            status::simulation_not_enough_model);

//         dispatch(mdl,
//                  [&sim, &from_c]<typename Dynamics>(Dynamics& dyn) -> void {
//                      new (&dyn) Dynamics(get_dyn<Dynamics>(*from_c));
//                  });
//     }

//     return status::success;
// }

// inline status
// copy_input_connection(const shared_flat_list<node>& connections,
//                       const map<model_id, model_id>& dict,
//                       const data_array<model, model_id>& component_models,
//                       const data_array<component, component_id>& components,
//                       simulation& sim,
//                       model& out_mdl,
//                       int out_port)
// {
//     std::vector<std::tuple<const model*, int>> stack;
//     std::vector<std::tuple<model*, int>> to_connect;

//     for (const auto& elem : connections) {
//         const auto* mdl = component_models.try_to_get(elem.model);
//         if (!mdl)
//             continue;

//         if (mdl->type == dynamics_type::none) {
//             stack.emplace_back(std::make_tuple(mdl, elem.port_index));
//         } else {
//             auto* v = dict.find(elem.model);
//             if (!v)
//                 continue;

//             auto* sim_mdl = sim.models.try_to_get(*v);
//             irt_return_if_fail(sim_mdl, status::unknown_dynamics);

//             to_connect.emplace_back(std::make_tuple(sim_mdl,
//             elem.port_index));
//         }
//     }

//     while (!stack.empty()) {
//         const auto src_mdl = stack.back();
//         stack.pop_back();

//         const auto& src_dyn = get_dyn<none>(*std::get<0>(src_mdl));
//         const auto* compo = components.try_to_get(src_dyn.id);
//         if (!compo)
//             continue;

//         auto it = compo->internal_y.begin();
//         std::advance(it, std::get<1>(src_mdl));

//         for (auto& elem : it->connections) {
//             const auto* mdl = component_models.try_to_get(elem.model);
//             if (!mdl)
//                 continue;

//             if (mdl->type == dynamics_type::none) {
//                 stack.emplace_back(mdl, elem.port_index);
//             } else {
//                 auto* v = src_dyn.dict.find(elem.model);
//                 if (!v)
//                     continue;

//                 auto* sim_mdl = sim.models.try_to_get(*v);
//                 irt_return_if_fail(sim_mdl, status::unknown_dynamics);

//                 to_connect.emplace_back(sim_mdl, elem.port_index);
//             }
//         }
//     }

//     for (auto& elem : to_connect)
//         sim.connect(*std::get<0>(elem), std::get<1>(elem), out_mdl,
//         out_port);
// }

// inline status
// copy_output_connection(const shared_flat_list<node>& connections,
//                        const map<model_id, model_id>& dict,
//                        const data_array<model, model_id>& component_models,
//                        const data_array<component, component_id>& components,
//                        simulation& sim,
//                        model& out_mdl,
//                        int out_port)
// {
//     std::vector<std::tuple<const model*, int>> stack;
//     std::vector<std::tuple<model*, int>> to_connect;

//     for (const auto& elem : connections) {
//         const auto* mdl = component_models.try_to_get(elem.model);
//         if (!mdl)
//             continue;

//         if (mdl->type == dynamics_type::none) {
//             stack.emplace_back(std::make_tuple(mdl, elem.port_index));
//         } else {
//             auto* v = dict.find(elem.model);
//             if (!v)
//                 continue;

//             auto* sim_mdl = sim.models.try_to_get(*v);
//             irt_return_if_fail(sim_mdl, status::unknown_dynamics);

//             to_connect.emplace_back(std::make_tuple(sim_mdl,
//             elem.port_index));
//         }
//     }

//     while (!stack.empty()) {
//         const auto src_mdl = stack.back();
//         stack.pop_back();

//         const auto& src_dyn = get_dyn<none>(*std::get<0>(src_mdl));
//         const auto* compo = components.try_to_get(src_dyn.id);
//         if (!compo)
//             continue;

//         auto it = compo->internal_y.begin();
//         std::advance(it, std::get<1>(src_mdl));

//         for (auto& elem : it->connections) {
//             const auto* mdl = component_models.try_to_get(elem.model);
//             if (!mdl)
//                 continue;

//             if (mdl->type == dynamics_type::none) {
//                 stack.emplace_back(mdl, elem.port_index);
//             } else {
//                 auto* v = src_dyn.dict.find(elem.model);
//                 if (!v)
//                     continue;

//                 auto* sim_mdl = sim.models.try_to_get(*v);
//                 irt_return_if_fail(sim_mdl, status::unknown_dynamics);

//                 to_connect.emplace_back(sim_mdl, elem.port_index);
//             }
//         }
//     }

//     for (auto& elem : to_connect)
//         sim.connect(*std::get<0>(elem), std::get<1>(elem), out_mdl,
//         out_port);
// }

// inline status
// flat_merge_create_connections(
//   none& none_mdl,
//   const data_array<component, component_id>& components,
//   const data_array<model, model_id>& component_models,
//   simulation& sim)
// {
//     none_mdl.dict.sort();

//     // For each models (none exclude) in the component, we duplicate the
//     model
//     // connection using the dictionary.

//     for (const auto& elem : none_mdl.dict.elements) {
//         const auto& from_c_mdl = component_models.get(elem.u);
//         if (from_c_mdl.type == dynamics_type::none)
//             continue;

//         auto& sim_mdl = sim.models.get(elem.v);

//         auto ret = dispatch(
//           sim_mdl,
//           [&components,
//            &component_models,
//            &none_mdl,
//            &sim,
//            &from_c_mdl]<typename Dynamics>(Dynamics& dyn) -> status {
//               auto& from_sim = get_model(dyn);
//               auto& from_c_dyn = get_dyn<Dynamics>(from_c_mdl);

//               if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
//                   int port_index = 0;
//                   for (auto& y : from_c_dyn.y) {
//                       auto ret = copy_output_connection(y.connections,
//                                                         none_mdl.dict,
//                                                         component_models,
//                                                         components,
//                                                         sim,
//                                                         from_sim,
//                                                         port_index);
//                       irt_return_if_bad(ret);
//                       ++port_index;
//                   }
//               }

//               return status::success;
//           });

//         irt_return_if_bad(ret);
//     }

//     return status::success;
// }

} // namespace detail

// inline status
// flat_merge(
//  [[maybe_unused]] none&                                      none_mdl,
//  [[maybe_unused]] const data_array<component, component_id>& components,
//  [[maybe_unused]] const data_array<model, model_id>& component_models,
//  [[maybe_unused]] simulation&                                sim)
//{
//    // irt_return_if_bad(detail::flat_merge_create_models(
//    //   none_mdl, components, component_models, sim));
//
//    // irt_return_if_bad(detail::flat_merge_create_connections(
//    //   none_mdl, components, component_models, sim));
//
//    return status::success;
//}

} // namespace irt

#endif
