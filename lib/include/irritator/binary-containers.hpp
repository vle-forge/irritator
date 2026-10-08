// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_BINARY_CONTAINERS_HPP
#define ORG_VLEPROJECT_IRRITATOR_BINARY_CONTAINERS_HPP

#include <irritator/binary.hpp>
#include <irritator/container.hpp>

#include <tuple>
#include <utility>

/// @c binary_serialize of @c id_array, @c data_array and @c id_data_array.
///
/// What a snapshot restores: *exactly* the state of the container, the slot of
/// every element, the key of every identifier, the key counter and the free
/// list. An identifier kept outside of the container before the snapshot is
/// valid after the restore if (and only if) its element was alive when the
/// snapshot was taken, and the next @c alloc() returns the identifier it would
/// have returned in the original run. There is no compaction.
///
/// Format (all the numbers are little endian, @c index_type is @c u16 for the
/// 32 bits identifiers and @c u32 for the 64 bits ones):
///
/// @verbatim
/// id_data_array : the ids container (id_array or data_array), then the
///                 columns
/// id_array      : capacity, max_size, max_used, next_key, free_head (all
///                 index_type), max_used identifiers
/// data_array    : capacity, max_size, max_used, next_key, free_head (all
///                 index_type), then for each slot [0, max_used):
///                 the identifier, then the element if the identifier is valid
/// column        : the elements of the alive slots, in slot order
/// @endverbatim
///
/// The dead slots of a @c data_array (destroyed elements) and the dead slots
/// of the columns of an @c id_data_array (stale values, the column keeps
/// constructed objects for its whole capacity) are not written: the dump only
/// depends on the alive elements, so two equal states give two equal dumps.
/// The consequence is that the stale values of the dead slots of the columns
/// are not restored: a program must not read a slot it just allocated before
/// initializing it.
///
/// Reading is defensive: every field is checked against the others (counts,
/// indices, keys, free list), the container is never accessed outside its
/// capacity and, after a failure, it is left cleared (valid and empty). The
/// capacity is restored (reserve) if the container is smaller, never reduced.
/// Use @c binary_reader::set_allocation_limit for a file that is not trusted.
///
/// The element type of a @c data_array must be default constructible; an
/// element which is not @c binary_raw needs its own @c binary_serialize.
namespace irt {

/// The friend of the containers (see the @c friend declarations in
/// @c container.hpp), it reaches the private members.
struct container_binary_access {
    /// Checks that the slots form a consistent array, in order: alive slots
    /// have their own index in their identifier, dead slots are exactly the
    /// slots of the free list, which is sorted by index (@c free keeps it
    /// so).
    template<typename Identifier, typename IndexType>
    class slot_checker
    {
    public:
        slot_checker(IndexType free_head_, IndexType none_) noexcept
          : none(none_)
          , expected_dead(free_head_)
        {}

        /// @return true if the slot is alive, false if it is dead. Sets
        /// @c good to false if the slot is not valid.
        bool check(const Identifier id, const IndexType index) noexcept
        {
            if (g_get_key(id) != 0) {
                if (static_cast<IndexType>(g_get_index(id)) != index)
                    good = false;

                ++alive;
                return true;
            }

            if (expected_dead != index)
                good = false;
            else
                expected_dead = static_cast<IndexType>(g_get_index(id));

            return false;
        }

        bool finish(const IndexType max_size) const noexcept
        {
            return good and alive == max_size and expected_dead == none;
        }

        bool ok() const noexcept { return good; }

    private:
        IndexType none;
        IndexType expected_dead;
        IndexType alive = 0;
        bool      good  = true;
    };

    /// The header of @c data_array and @c id_array.
    template<typename IndexType>
    struct header {
        IndexType capacity  = 0;
        IndexType max_size  = 0;
        IndexType max_used  = 0;
        IndexType next_key  = 1;
        IndexType free_head = 0;
    };

    template<typename IndexType>
    static bool check_header(const header<IndexType>& h,
                             const IndexType          none) noexcept
    {
        // max_size and free_head are checked with the slots (slot_checker)

        return h.capacity < none and h.max_used <= h.capacity and
               h.next_key != 0;
    }

    /// ids of id_array.
    template<typename Ar, typename Identifier, typename A, typename Reserve>
    static void serialize_ids(Ar& ar, id_array<Identifier, A>& d, Reserve&& reserve_fn) noexcept
    {
        using array      = id_array<Identifier, A>;
        using index_type = typename array::index_type;

        constexpr auto none = array::none;

        if constexpr (Ar::is_writer) {
            ar(d.m_capacity, d.m_max_size, d.m_max_used, d.m_next_key,
               d.m_free_head);
            ar.write_span(std::span<const Identifier>(d.m_items, d.m_max_used));
        } else {
            header<index_type> h;
            ar(h.capacity, h.max_size, h.max_used, h.next_key, h.free_head);

            if (not ar.ok() or not check_header(h, none)) {
                ar.fail();
                d.clear();
                return;
            }

            d.clear();

            if (not reserve_fn(h.capacity)) {
                ar.fail();
                return;
            }

            if (h.max_used > 0)
                ar.read_span(std::span<Identifier>(d.m_items, h.max_used));

            slot_checker<Identifier, index_type> checker(h.free_head, none);

            for (index_type i = 0; i != h.max_used and ar.ok(); ++i)
                checker.check(d.m_items[i], i);

            if (not ar.ok() or not checker.finish(h.max_size)) {
                ar.fail(); // d is still empty: the fields are not set
                return;
            }

            d.m_max_size  = h.max_size;
            d.m_max_used  = h.max_used;
            d.m_next_key  = h.next_key;
            d.m_free_head = h.free_head;
        }
    }

    /// ids and elements of data_array.
    template<typename Ar, typename T, typename Identifier, typename A, typename Reserve>
    static void serialize_ids(Ar& ar,
                              data_array<T, Identifier, A>& d,
                              Reserve&&                     reserve_fn) noexcept
    {
        using array      = data_array<T, Identifier, A>;
        using index_type = typename array::index_type;

        static_assert(std::is_default_constructible_v<T>,
                      "binary_serialize of data_array needs a default "
                      "constructible element: the reader builds it, then "
                      "fills it");

        constexpr auto none = array::none;

        if constexpr (Ar::is_writer) {
            ar(d.m_capacity, d.m_max_size, d.m_max_used, d.m_next_key,
               d.m_free_head);

            for (index_type i = 0; i != d.m_max_used; ++i) {
                ar(d.m_items[i].id);

                if (g_get_key(d.m_items[i].id) != 0)
                    ar(d.m_items[i].item);
            }
        } else {
            header<index_type> h;
            ar(h.capacity, h.max_size, h.max_used, h.next_key, h.free_head);

            if (not ar.ok() or not check_header(h, none)) {
                ar.fail();
                d.clear();
                return;
            }

            d.clear();

            if (not reserve_fn(h.capacity)) {
                ar.fail();
                return;
            }

            slot_checker<Identifier, index_type> checker(h.free_head, none);

            // The slots are published one by one (max_used grows with the
            // loop), so after a failure clear() destroys exactly the
            // elements that were built.

            for (index_type i = 0; i != h.max_used; ++i) {
                Identifier id{};
                ar(id);

                if (not ar.ok()) {
                    d.clear();
                    return;
                }

                const bool alive = checker.check(id, i);
                if (not checker.ok()) {
                    ar.fail();
                    d.clear();
                    return;
                }

                if (alive) {
                    std::construct_at(std::addressof(d.m_items[i].item));
                    d.m_items[i].id = id;
                    d.m_max_used    = static_cast<index_type>(i + 1);

                    ar(d.m_items[i].item);

                    if (not ar.ok()) {
                        d.clear();
                        return;
                    }
                } else {
                    d.m_items[i].id = id;
                    d.m_max_used    = static_cast<index_type>(i + 1);
                }
            }

            if (not checker.finish(h.max_size)) {
                ar.fail();
                d.clear();
                return;
            }

            d.m_max_size  = h.max_size;
            d.m_max_used  = h.max_used;
            d.m_next_key  = h.next_key;
            d.m_free_head = h.free_head;
        }
    }

    /// The size of a slot of the ids container.
    template<typename Ids>
    static constexpr std::size_t slot_size() noexcept
    {
        if constexpr (requires { typename Ids::internal_value_type; })
            return sizeof(typename Ids::internal_value_type);
        else
            return sizeof(typename Ids::identifier_type);
    }

    template<typename Ar, typename Identifier, typename A>
    static void serialize(Ar& ar, id_array<Identifier, A>& d) noexcept
    {
        serialize_ids(ar, d, [&d, &ar](const auto capacity) noexcept {
            return reserve_checked(ar, d, capacity, sizeof(Identifier));
        });
    }

    template<typename Ar, typename T, typename Identifier, typename A>
    static void serialize(Ar& ar, data_array<T, Identifier, A>& d) noexcept
    {
        using item = typename data_array<T, Identifier, A>::internal_value_type;

        serialize_ids(ar, d, [&d, &ar](const auto capacity) noexcept {
            return reserve_checked(ar, d, capacity, sizeof(item));
        });
    }

    /// Reserve for the containers alone: checks the allocation limit.
    template<typename Ar, typename Container, typename Capacity>
    static bool reserve_checked(Ar&                ar,
                        Container&         d,
                        const Capacity     capacity,
                        const std::size_t  bytes_per_slot) noexcept
    {
        if (std::cmp_less_equal(capacity, d.capacity()))
            return true;

        if (not ar.can_allocate(static_cast<std::size_t>(capacity) *
                                bytes_per_slot))
            return false;

        return d.reserve(capacity);
    }

    /// The alive slots of the ids container, as runs of consecutive indices.
    template<typename Ids, typename Fn>
    static void for_each_run(const Ids& ids, Fn&& fn) noexcept
    {
        const auto max_used = ids.m_max_used;

        std::size_t i = 0;
        while (i != max_used) {
            if (not is_alive(ids, i)) {
                ++i;
                continue;
            }

            std::size_t last = i + 1;
            while (last != max_used and is_alive(ids, last))
                ++last;

            fn(i, last);
            i = last;
        }
    }

    template<typename Identifier, typename A>
    static bool is_alive(const id_array<Identifier, A>& ids,
                         const std::size_t              i) noexcept
    {
        return g_get_key(ids.m_items[i]) != 0;
    }

    template<typename T, typename Identifier, typename A>
    static bool is_alive(const data_array<T, Identifier, A>& ids,
                         const std::size_t                   i) noexcept
    {
        return g_get_key(ids.m_items[i].id) != 0;
    }

    /// One column: the elements of the alive slots.
    template<typename Ar, typename Ids, typename Column>
    static void serialize_column(Ar& ar, const Ids& ids, Column& column) noexcept
    {
        using value_type = std::remove_cvref_t<decltype(*column.get())>;

        for_each_run(ids, [&](const std::size_t first,
                              const std::size_t last) noexcept {
            if constexpr (binary_raw<value_type>) {
                // same bytes as one by one, with one operation
                if constexpr (Ar::is_writer)
                    ar.write_bytes(column.get() + first,
                                   (last - first) * sizeof(value_type));
                else
                    ar.read_bytes(column.get() + first,
                                  (last - first) * sizeof(value_type));
            } else {
                for (auto i = first; i != last and ar.ok(); ++i)
                    ar(column.get()[i]);
            }
        });
    }

    template<typename Ar, typename Ids, typename Tuple, std::size_t... Is>
    static void serialize_columns(Ar&         ar,
                                  const Ids&  ids,
                                  Tuple&      columns,
                                  std::index_sequence<Is...>) noexcept
    {
        (serialize_column(ar, ids, std::get<Is>(columns)), ...);
    }

    template<typename Ar, typename T, typename Identifier, typename A, class... Ts>
    static void serialize(Ar& ar, id_data_array<T, Identifier, A, Ts...>& d) noexcept
    {
        using array    = id_data_array<T, Identifier, A, Ts...>;
        using ids_type = typename array::identifier_container_type;

        // The reserve of the whole container (ids and columns): the ids
        // container never grows alone.

        [[maybe_unused]] constexpr std::size_t bytes_per_slot =
          slot_size<ids_type>() +
                                               (sizeof(Ts) + ... +
                                                std::size_t{ 0 });

        auto reserve_all = [&d, &ar](const auto capacity) noexcept {
            return reserve_checked(ar, d, capacity, bytes_per_slot);
        };

        if constexpr (Ar::is_writer) {
            serialize_ids(ar, d.m_ids, reserve_all);
            serialize_columns(ar, d.m_ids, d.m_col,
                              std::index_sequence_for<Ts...>());
        } else {
            serialize_ids(ar, d.m_ids, reserve_all);

            // after a failure the ids are cleared, so there is no column to
            // read

            serialize_columns(ar, d.m_ids, d.m_col,
                              std::index_sequence_for<Ts...>());

            if (not ar.ok())
                d.m_ids.clear();
        }
    }
};

template<typename Ar, typename Identifier, typename A>
void binary_serialize(Ar& ar, id_array<Identifier, A>& d) noexcept
{
    container_binary_access::serialize(ar, d);
}

template<typename Ar, typename T, typename Identifier, typename A>
void binary_serialize(Ar& ar, data_array<T, Identifier, A>& d) noexcept
{
    container_binary_access::serialize(ar, d);
}

template<typename Ar, typename T, typename Identifier, typename A, class... Ts>
void binary_serialize(Ar& ar, id_data_array<T, Identifier, A, Ts...>& d) noexcept
{
    container_binary_access::serialize(ar, d);
}

} // namespace irt

#endif
