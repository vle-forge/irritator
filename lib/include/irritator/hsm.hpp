#include <array>
#include <irritator/core.hpp>
#include <irritator/ext.hpp>

namespace irritator {

/**
 * Hierarchical state machine.
 * \note This implementation have the standard restriction for HSM:
 * 1. You can not call Transition from HSM::enter_event_id and
 * HSM::exit_event_id! These event are provided to execute your
 * construction/destruction. Use custom events for that.
 * 2. You are not allowed to dispatch an event from within an event dispatch.
 * You should queue events if you want such behavior. This restriction is
 * imposed only to prevent the user from creating extremely complicated to trace
 * state machines (which is what we are trying to avoid).
 */
template<u8 MaxNumberOfState = 254, u8 InvalidStateId = 255>
class HSM
{
public:
    using state_id = u8;

    enum event_id_type
    {
        invalid_event_id  = -4,
        exit_event_id     = -2,
        enter_event_id    = -1,
        external_event_id = 0
    };

    static const u8 max_number_of_state = MaxNumberOfState;
    static const u8 invalid_state_id    = InvalidStateId;

    struct event
    {
        event() noexcept = default;
        event(i32 id_) noexcept;

        i32   id        = invalid_event_id;
        void* user_data = nullptr;
    };

    /// Call from event to perform state update. \return true if the event
    /// change the current state, false otherwise.
    using state_handler = bool (*)(HSM& sm, const event& e);

    HSM() noexcept = default;

    void start() noexcept;

    /// Dispatches an event, return true if the event was processed, otherwise
    /// false.
    bool dispatch(const event& e) noexcept;

    /// Return true if the state machine is currently dispatching an event.
    bool is_dispatching() const noexcept;

    /// Transitions the state machine. This function can not be called from
    /// Enter / Exit events in the state handler.
    void transition(state_id target) noexcept;

    state_id get_current_state() const noexcept;
    state_id get_source_state() const noexcept;

    /// Set a handler for a state ID. This function will overwrite the current
    /// state handler.
    /// \param id state id from 0 to max_number_of_state
    /// \param name state debug name.
    /// \param handler delegate to the state function.
    /// \param superId id of the super state, if invalid_state_id this is a top
    /// state. Only one state can be a top state.
    /// \param sub_id if !=
    /// invalid_state_id this sub state (child state) will be entered after the
    /// state Enter event is executed.
    void set_state(state_id      id,
                   state_handler handler,
                   state_id      super_id = invalid_state_id,
                   state_id      sub_id   = invalid_state_id) noexcept;

    /// Resets the state to invalid mode.
    void clear_state(state_id id);

    bool is_in_state(state_id id) const;

protected:
    int  steps_to_common_root(state_id source, state_id target);
    void on_enter_sub_state();

    state_id m_current_state        = invalid_state_id;
    state_id m_next_state           = invalid_state_id;
    state_id m_source_state         = invalid_state_id;
    state_id m_current_source_state = invalid_state_id;
    state_id m_top_state =         = invalid_state_id;
    bool     m_disallow_transition = false;

    struct state
    {
        state() noexcept = default;

        state_handler handler  = nullptr;
        state_id      super_id = invalid_state_id;
        state_id      subId    = invalid_state_id;
    };

    std::array<state, max_number_of_state> m_states;
};

inline bool HSM::is_dispatching() const noexcept
{
    return m_current_source_state != invalid_state_id;
}

inline state_id HSM::get_current_state() const noexcept
{
    return m_current_state;
}

inline state_id HSM::get_source_state() const noexcept
{
    return m_source_state;
}

inline void HSM::start() noexcept
{
    irt_assert(m_top_state != invalid_state_id);
    m_current_state = m_top_state;
    m_next_state    = invalid_state_id;

    if (m_states[m_current_state].handler)
        m_states[m_current_state].handler(*this, event(enter_event_id));

    while ((m_next_state = m_states[m_current_state].sub_id) !=
           invalid_state_id) {
        on_enter_sub_state();
    }
}

inline bool HSM::dispatch(const event& e) noexcept
{
    irt_assert(e.id >= 0);
    irt_assert(!is_dispatching());

    bool is_processed = false;

    for (state_id sid = m_current_state; sid != invalid_state_id;) {
        auto& state            = m_states[sid];
        m_current_source_state = sid;

        if (state.handler && state.handler(*this, e)) {
            if (m_next_state != invalid_state_id) {
                do {
                    on_enter_sub_state();
                } while ((m_next_state = m_states[m_current_state].sub_id) !=
                         invalid_state_id);
            }
            is_processed = true;
            break;
        }

        sid = state.super_id;
    }
    m_current_source_state = invalid_state_id;

    return is_processed;
}

inline inline void HSM::on_enter_sub_state() noexcept
{
    irt_assert(m_next_state != invalid_state_id);

    small_vector<state*, max_number_of_state / 10> entry_path;
    for (state_id sid = m_next_state; sid != m_current_state;) {
        auto& state = m_states[sid];

        entry_path.push_back(&state);
        sid = state.super_id;

        irt_assert(sid != invalid_state_id);
    }

    while (!entry_path.empty()) {
        m_disallow_transition = true;

        if (entry_path.back()->handler)
            entry_path.back()->handler(*this, event(enter_event_id));

        m_disallow_transition = false;
        entry_path.pop_back();
    }

    m_current_state = m_next_state;
    m_next_state    = invalid_state_id;
}

inline void HSM::transition(state_id target) noexcept
{
    irt_assert(target < max_number_of_state);
    irt_assert(m_disallow_transition == false);
    irt_assert(is_dispatching());

    if (m_disallow_transition)
        return;

    if (m_current_source_state != invalid_state_id)
        m_source_state = m_current_source_state;

    m_disallow_transition = true;
    state_id sid;
    for (sid = m_current_state; sid != m_source_state;) {
        auto& state = m_states[sid];
        state.handler(*this, event(exit_event_id));
        sid = state.super_id;
    }

    int stepsToCommonRoot = steps_to_common_root(m_source_state, target);
    irt_assert(stepsToCommonRoot >= 0);

    while (stepsToCommonRoot--) {
        State& state = m_states[sid];

        if (state.handler)
            state.handler(*this, event(exit_event_id));

        sid = state.super_id;
    }

    m_disallow_transition = false;

    m_current_state = sid;
    m_next_state    = target;
}

inline void HSM::set_state(state_id      id,
                           state_handler handler,
                           state_id      super_id,
                           state_id      sub_id) noexcept
{
    if (super_id == invalid_state_id) {
        irt_Assert(m_top_state == invalid_state_id);
        m_top_state = id;
    }

    m_states[id].super_id = super_id;
    m_states[id].sub_id   = subId;
    m_states[id].handler  = handler;

    irt_assert(super_id == invalid_state_id ||
               m_states[super_id].sub_id != invalid_state_id);
}

inline void HSM::clear_state(state_id id) noexcept
{
    m_states[id].handler.clear();
    m_states[id].name     = nullptr;
    m_states[id].super_id = invalid_state_id;
}

inline bool HSM::is_in_state(state_id id) const noexcept
{
    state_id sid = m_current_state;

    while (sid != invalid_state_id) {
        if (sid == id)
            return true;

        sid = m_states[sid].super_id;
    }

    return false;
}

inline int HSM::steps_to_common_root(state_id source, state_id target) noexcept
{
    if (source == target)
        return 1;

    int toLca = 0;
    for (state_id s = source; s != invalid_state_id; ++toLca) {
        for (state_id t = target; t != invalid_state_id;) {
            if (s == t)
                return toLca;

            t = m_states[t].super_id;
        }

        s = m_states[s].super_id;
    }

    return -1;
}

inline HSM::event::event(i32 id_) noexcept
  : id(id_)
  , user_data(nullptr)
{}

} // namespace irritator
