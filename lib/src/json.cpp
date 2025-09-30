// Copyright (c) 2022 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/archiver.hpp>
#include <irritator/core.hpp>
#include <irritator/dot-parser.hpp>
#include <irritator/file.hpp>
#include <irritator/format.hpp>
#include <irritator/helpers.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/reader.h>
#include <rapidjson/writer.h>

#include <limits>
#include <optional>
#include <string_view>
#include <utility>

#include <cerrno>
#include <cstdint>

using namespace std::literals;

namespace irt {

enum class connection_type { internal, input, output, input_pack, output_pack };

struct json_dearchiver::impl {
    json_dearchiver& self;

    modeling*        m_mod = nullptr;
    simulation*      m_sim = nullptr;
    project*         m_pj  = nullptr;
    std::string_view m_path;

    i64         temp_i64    = 0;
    u64         temp_u64    = 0;
    double      temp_double = 0.0;
    bool        temp_bool   = false;
    std::string temp_string;

    small_vector<std::string_view, 16> stack;

    bool has_error = false;

    bool have_error() const noexcept { return has_error; }

    template<typename... T>
    void warning(fmt::format_string<T...> fmt, T&&... args) noexcept
    {
        if (not m_mod)
            return;

        m_mod->journal.push(
          log_level::warning,
          [](auto& title,
             auto& msg,
             auto  path,
             auto& stack,
             auto& fmt,
             auto  args) {
              format(title, "json warning {}\n", path);

              auto data      = msg.data();
              sz   remaining = msg.capacity() - 1u;
              sz   write     = 0;

              for (auto i = 0u; i < stack.size() and remaining > 0; ++i) {
                  auto ret = fmt::format_to_n(
                    data, remaining, "{:{}}{}\n", "", i, stack[i]);
                  write += ret.size;
                  msg.resize(write);
                  data = ret.out;
                  remaining -= ret.size;
              }

              auto ret = fmt::format_to_n(
                data, remaining, "{:{}}", "", stack.size() + 1);
              write += ret.size;
              msg.resize(write);
              data = ret.out;
              remaining -= ret.size;

              ret = fmt::vformat_to_n(data, remaining, fmt, args);
              msg.resize(write + ret.size);
          },
          m_path,
          std::as_const(stack),
          fmt,
          fmt::make_format_args(args...));
    }

    template<typename... T>
    bool error(fmt::format_string<T...> fmt, T&&... args) noexcept
    {
        if (on_error_callback)
            on_error_callback();

        if (m_mod) {
            m_mod->journal.push(
              log_level::error,
              [](auto& title,
                 auto& msg,
                 auto  path,
                 auto& stack,
                 auto& fmt,
                 auto  args) {
                  format(title, "json error {}\n", path);

                  auto data      = msg.data();
                  sz   remaining = static_cast<int>(msg.capacity()) - 1;
                  sz   write     = 0;

                  for (auto i = 0u; i < stack.size() and remaining > 0; ++i) {
                      auto ret = fmt::format_to_n(data,
                                                  remaining,
                                                  "  {}: {}\n",
                                                  static_cast<int>(i),
                                                  stack[i]);
                      write += ret.size;
                      msg.resize(write);
                      data = ret.out;
                      remaining -= ret.size;
                  }

                  auto ret = fmt::format_to_n(
                    data, remaining, "{:{}}", "", stack.size() + 1);
                  write += ret.size;
                  msg.resize(write);
                  data = ret.out;
                  remaining -= ret.size;

                  ret = fmt::vformat_to_n(data, remaining, fmt, args);
                  msg.resize(write + ret.size);
              },
              m_path,
              std::as_const(stack),
              fmt,
              fmt::make_format_args(args...));
        }

        has_error = true;

        return false;
    }

    void clear() noexcept
    {
        temp_i64    = 0;
        temp_u64    = 0;
        temp_double = 0.0;
        temp_bool   = false;
        temp_string.clear();
        stack.clear();
        has_error = false;
    }

    modeling& mod() const noexcept
    {
        debug::ensure(m_mod);
        return *m_mod;
    }

    simulation& sim() const noexcept
    {
        debug::ensure(m_sim);
        return *m_sim;
    }

    project& pj() const noexcept
    {
        debug::ensure(m_pj);
        return *m_pj;
    }

    impl(json_dearchiver& self_,
         modeling&        mod_,
         std::string_view path = std::string_view{}) noexcept
      : self(self_)
      , m_mod(&mod_)
      , m_path(path)
    {}

    impl(json_dearchiver& self_,
         simulation&      sim_,
         std::string_view path = std::string_view{}) noexcept
      : self(self_)
      , m_sim(&sim_)
      , m_path(path)
    {}

    impl(json_dearchiver& self_,
         modeling&        mod_,
         simulation&      sim_,
         project&         pj_,
         std::string_view path = std::string_view{}) noexcept
      : self(self_)
      , m_mod(&mod_)
      , m_sim(&sim_)
      , m_pj(&pj_)
      , m_path(path)
    {}

    struct auto_stack {
        auto_stack(json_dearchiver::impl* r_,
                   const std::string_view id) noexcept
          : r(r_)
        {
            fatal::ensure(r->stack.can_alloc(1));
            r->stack.emplace_back(id);
        }

        ~auto_stack() noexcept { r->stack.pop_back(); }

        json_dearchiver::impl* r = nullptr;
    };

    template<typename Function, typename... Args>
    bool for_each_array(const rapidjson::Value& array,
                        Function&&              f,
                        Args&&... args) noexcept
    {
        if (!array.IsArray())
            return error("json value is not an array");

        rapidjson::SizeType i = 0, e = array.GetArray().Size();
        for (; i != e; ++i) {
            // debug_logi(stack.ssize(), "for-array: {}/{}\n", i, e);

            if (!std::invoke(std::forward<Function>(f),
                             i,
                             array.GetArray()[i],
                             std::forward<Args>(args)...))
                return false;
        }

        return true;
    }

    template<size_t N, typename Function>
    bool for_members(const rapidjson::Value& val,
                     const std::string_view (&names)[N],
                     Function&& fn) noexcept
    {
        if (!val.IsObject())
            return error("json value is not an object");

        auto       it = val.MemberBegin();
        const auto et = val.MemberEnd();

        while (it != et) {
            const auto x =
              binary_find(std::begin(names),
                          std::end(names),
                          std::string_view{ it->name.GetString(),
                                            it->name.GetStringLength() });

            if (x == std::end(names)) {
                // debug_logi(stack.ssize(),
                //            "for-member: unknown element {}\n",
                //            std::string_view{ it->name.GetString(),
                //                              it->name.GetStringLength()
                //                              });

                return error("unknown element {}", it->name.GetString());
            }

            if (!std::invoke(std::forward<Function>(fn),
                             std::distance(std::begin(names), x),
                             it->value)) {
                // debug_logi(stack.ssize(),
                //            "for-member: element {} return false\n",
                //            std::string_view{ it->name.GetString(),
                //                              it->name.GetStringLength()
                //                              });
                return false;
            }

            ++it;
        }

        return true;
    }

    template<typename Function, typename... Args>
    bool for_each_member(const rapidjson::Value& val,
                         Function&&              f,
                         Args&&... args) noexcept
    {
        if (!val.IsObject())
            return error("json value is not an object");

        for (auto it = val.MemberBegin(), et = val.MemberEnd(); it != et;
             ++it) {
            // debug_logi(stack.ssize(), "for-member: {}\n",
            // it->name.GetString());
            if (!std::invoke(std::forward<Function>(f),
                             it->name.GetString(),
                             it->value,
                             std::forward<Args>(args)...))
                return false;
        }

        return true;
    }

    template<typename Function, typename... Args>
    bool for_first_member(const rapidjson::Value& val,
                          std::string_view        name,
                          Function&&              f,
                          Args&&... args) noexcept
    {
        if (!val.IsObject())
            return error("json value is not an object");

        for (auto it = val.MemberBegin(), et = val.MemberEnd(); it != et; ++it)
            if (name == it->name.GetString())
                return std::invoke(std::forward<Function>(f),
                                   it->value,
                                   std::forward<Args>(args)...);

        return error("json object member {} not found", name);
    }

    bool read_temp_i64(const rapidjson::Value& val) noexcept
    {
        if (!val.IsInt64())
            return error("missing integer");

        temp_i64 = val.GetInt64();

        return true;
    }

    bool read_temp_bool(const rapidjson::Value& val) noexcept
    {
        if (!val.IsBool())
            return error("missing bool");

        temp_bool = val.GetBool();

        return true;
    }

    bool read_bool(const rapidjson::Value& val, bool& b) noexcept
    {
        if (not read_temp_bool(val))
            return false;

        b = temp_bool;

        return true;
    }

    bool read_bool(const rapidjson::Value& val, i64& i) noexcept
    {
        if (not read_temp_bool(val))
            return false;

        i = temp_bool;

        return true;
    }

    bool read_temp_u64(const rapidjson::Value& val) noexcept
    {
        if (!val.IsUint64())
            return error("missing u64");

        temp_u64 = val.GetUint64();

        return true;
    }

    bool read_u64(const rapidjson::Value& val, u64& integer) noexcept
    {
        if (val.IsUint64()) {
            integer = val.GetUint64();
            return true;
        }

        return error("missing u64");
    }

    bool read_real(const rapidjson::Value& val, double& r) noexcept
    {
        if (val.IsDouble()) {
            r = val.GetDouble();
            return true;
        }

        return error("missing double");
    }

    bool read_temp_real(const rapidjson::Value& val) noexcept
    {
        if (!val.IsDouble())
            return error("missing double");

        temp_double = val.GetDouble();

        return true;
    }

    bool read_temp_string(const rapidjson::Value& val) noexcept
    {
        if (!val.IsString())
            return error("missing string");

        temp_string = val.GetString();

        return true;
    }

    bool copy_string_to(binary_file_source& src) noexcept
    {
        try {
            src.file_path = temp_string;
            return true;
        } catch (...) {
        }

        return error("binary file missing {}", temp_string);
    }

    bool copy_string_to(text_file_source& src) noexcept
    {
        try {
            src.file_path = temp_string;
            return true;
        } catch (...) {
        }

        return error("text file missing {}", temp_string);
    }

    bool copy_string_to(std::optional<constant::init_type>& type) noexcept
    {
        if (temp_string == "constant"sv)
            type = constant::init_type::constant;
        else if (temp_string == "incoming_component_all"sv)
            type = constant::init_type::incoming_component_all;
        else if (temp_string == "outcoming_component_all"sv)
            type = constant::init_type::outcoming_component_all;
        else if (temp_string == "incoming_component_n"sv)
            type = constant::init_type::incoming_component_n;
        else if (temp_string == "outcoming_component_n"sv)
            type = constant::init_type::outcoming_component_n;
        else
            return error("bad constant init type {}", temp_string);

        return true;
    }

    bool copy_string_to(connection_type& type) noexcept
    {
        if (temp_string == "internal"sv)
            type = connection_type::internal;
        else if (temp_string == "output"sv)
            type = connection_type::output;
        else if (temp_string == "input"sv)
            type = connection_type::input;
        else if (temp_string == "input-pack"sv)
            type = connection_type::input_pack;
        else if (temp_string == "output-pack"sv)
            type = connection_type::output_pack;
        else
            return error("bad connection type {}", temp_string);

        return true;
    }

    bool copy_string_to(distribution_type& dst) noexcept
    {
        if (auto dist_opt = get_distribution_type(temp_string);
            dist_opt.has_value()) {
            dst = dist_opt.value();
            return true;
        }

        return error("bad distribution type {}", temp_string);
    }

    bool copy_string_to(dynamics_type& dst) noexcept
    {
        if (auto opt = get_dynamics_type(temp_string); opt.has_value()) {
            dst = opt.value();
            return true;
        }

        return error("bad dynamics type {}", temp_string);
    }

    bool copy_string_to(child_type& dst_1, dynamics_type& dst_2) noexcept
    {
        if (temp_string == "component"sv) {
            dst_1 = child_type::component;
            return true;
        } else {
            dst_1 = child_type::model;

            if (auto opt = get_dynamics_type(temp_string); opt.has_value()) {
                dst_2 = opt.value();
                return true;
            } else {
                return error("bad dynamics type {}", temp_string);
            }
        }
    }

    bool copy_i64_to(grid_component::type& dst) noexcept
    {
        if (0 <= temp_i64 && temp_i64 < grid_component::type_count) {
            dst = enum_cast<grid_component::type>(temp_i64);
            return true;
        }

        return error("bad grid component type {}", temp_i64);
    }

    bool copy_string_to(component_type& dst) noexcept
    {
        if (auto opt = get_component_type(temp_string); opt.has_value()) {
            dst = opt.value();
            return true;
        }

        return error("bad component type {}", temp_string);
    }

    bool copy_string_to(port_option& type) noexcept
    {
        for (auto i = 0, e = length(port_option_names); i != e; ++i) {
            if (temp_string == port_option_names[i]) {
                type = enum_cast<port_option>(i);
                return true;
            }
        }

        return error("bad input port type {}", temp_string);
    }

    template<std::size_t Length>
    bool copy_string_to(small_string<Length>& dst) noexcept
    {
        dst.assign(temp_string);

        return true;
    }

    bool copy_bool_to(bool& dst) const noexcept
    {
        dst = temp_bool;

        return true;
    }

    bool copy_real_to(double& dst) const noexcept
    {
        dst = temp_double;

        return true;
    }

    bool copy_i64_to(i64& dst) const noexcept
    {
        dst = temp_i64;

        return true;
    }

    bool copy_i64_to(i32& dst) noexcept
    {
        if (!(INT32_MIN <= temp_i64 && temp_i64 <= INT32_MAX))
            return error("not a 32 bits integer: {}", temp_i64);

        dst = static_cast<i32>(temp_i64);

        return true;
    }

    bool copy_u64_to(u32& dst) noexcept
    {
        if (temp_u64 >= UINT32_MAX)
            return error("not a 32 bits unsigned integer: {}", temp_u64);

        dst = static_cast<u8>(temp_u64);

        return true;
    }

    bool copy_i64_to(i8& dst) noexcept
    {
        if (!(0 <= temp_i64 && temp_i64 <= INT8_MAX))
            return error("not a 8 bits integer: {}", temp_i64);

        dst = static_cast<i8>(temp_i64);

        return true;
    }

    bool copy_u64_to(u8& dst) noexcept
    {
        if (!(temp_u64 <= UINT8_MAX))
            return error("not a 8 bits unsigned integer: {}", temp_u64);

        dst = static_cast<u8>(temp_u64);

        return true;
    }

    bool copy_i64_to(hierarchical_state_machine::action_type& dst) noexcept
    {
        if (!(0 <= temp_i64 &&
              temp_i64 < hierarchical_state_machine::action_type_count))
            return error("bad HSM action type: {}", temp_i64);

        dst = enum_cast<hierarchical_state_machine::action_type>(temp_i64);

        return true;
    }

    bool copy_i64_to(hierarchical_state_machine::condition_type& dst) noexcept
    {
        if (!(0 <= temp_i64 &&
              temp_i64 < hierarchical_state_machine::condition_type_count))
            return error("bad HSM condition type: {}", temp_i64);

        dst = enum_cast<hierarchical_state_machine::condition_type>(temp_i64);

        return true;
    }

    bool copy_i64_to(hierarchical_state_machine::variable& dst) noexcept
    {
        if (!(0 <= temp_i64 &&
              temp_i64 < hierarchical_state_machine::variable_count))
            return error("bad HSM variable type: {}", temp_i64);

        dst = enum_cast<hierarchical_state_machine::variable>(temp_i64);

        return true;
    }

    bool copy_string_to(real (*&dst)(real)) noexcept
    {
        if (temp_string == "time"sv)
            dst = &time_function;
        else if (temp_string == "square"sv)
            dst = &square_time_function;
        else if (temp_string == "sin"sv)
            dst = &sin_time_function;
        else
            return error("bad function type: {}", temp_string);

        return true;
    }

    template<typename T>
    bool optional_has_value(const std::optional<T>& v) noexcept
    {
        if (v.has_value())
            return true;

        return false;
    }

    bool copy_u64_to(u64& dst) const noexcept
    {
        dst = temp_u64;
        return true;
    }

    bool copy_u64_to(i64& dst) const noexcept
    {
        if (dst > INT64_MAX)
            return false;

        dst = static_cast<i64>(temp_u64);
        return true;
    }

    bool copy_i64_to(u64& dst) const noexcept
    {
        if (temp_i64 < 0)
            return false;

        dst = static_cast<u64>(temp_i64);
        return true;
    }

    bool copy_bool_to(i64& dst) const noexcept
    {
        dst = temp_bool;
        return true;
    }

    bool copy_real_to(std::optional<double>& dst) noexcept
    {
        dst = temp_double;
        return true;
    }

    bool copy_u64_to(std::optional<u8>& dst) noexcept
    {
        if (!(temp_u64 <= UINT8_MAX))
            return error("bad 8 bits unsigned integer: {}", temp_u64);

        dst = static_cast<u8>(temp_u64);
        return true;
    }

    bool copy_u64_to(std::optional<u32>& dst) noexcept
    {
        if (not is_numeric_castable<u32>(temp_u64))
            return error("bad 32 bits unsigned integer: {}", temp_u64);

        dst = static_cast<u32>(temp_u64);
        return true;
    }

    bool copy_u64_to(std::optional<u64>& dst) noexcept
    {
        dst = temp_u64;
        return true;
    }

    bool copy_i64_to(std::optional<i8>& dst) noexcept
    {
        if (!(INT8_MIN <= temp_i64 && temp_i64 <= INT8_MAX))
            return error("bad 8 bits integer: {}", temp_i64);

        dst = static_cast<i8>(temp_i64);
        return true;
    }

    bool copy_i64_to(std::optional<i32>& dst) noexcept
    {
        if (!(INT32_MIN <= temp_i64 && temp_i64 <= INT32_MAX))
            return error("bad 32 bits integer: {}", temp_i64);

        dst = static_cast<int>(temp_i64);
        return true;
    }

    bool copy_string_to(std::optional<std::string>& dst) noexcept
    {
        dst = temp_string;
        return true;
    }

    bool copy_real_to(float& dst) const noexcept
    {
        dst = static_cast<float>(temp_double);
        return true;
    }

    bool copy_i64_to(std::optional<source::source_type>& dst) noexcept
    {
        if (0 <= temp_i64 and temp_i64 < 5) {
            dst = enum_cast<source::source_type>(temp_i64);
            return true;
        }

        return error("fail to convert integer {} to source::source_type",
                     temp_i64);
    }

    bool project_global_parameters_can_alloc(std::integral auto i) noexcept
    {
        if (!pj().parameters.can_alloc(i))
            return error("can not allocate more global parameters (max: {})",
                         pj().parameters.capacity());

        return true;
    }

    bool project_variable_observers_can_alloc(std::integral auto i) noexcept
    {
        if (!pj().variable_observers.can_alloc(i))
            return error("can not allocate more variable observers (max: {})",
                         pj().variable_observers.capacity());

        return true;
    }

    bool project_grid_observers_can_alloc(std::integral auto i) noexcept
    {
        if (!pj().grid_observers.can_alloc(i))
            return error("can not allocate more grid observers (max: {})",
                         pj().grid_observers.capacity());

        return true;
    }

    bool generic_can_alloc(const generic_component& compo,
                           std::integral auto       i) noexcept
    {
        if (not compo.children.can_alloc(i))
            return error(
              "can not allocate more child in generic component (max: {})",
              compo.children.capacity());

        return true;
    }

    bool is_double_greater_than(double excluded_min) noexcept
    {
        if (temp_double <= excluded_min)
            return error(
              "bad real greater than {} > {}", temp_double, excluded_min);

        return true;
    }

    bool is_double_greater_equal_than(double included_min) noexcept
    {
        if (temp_double < included_min)
            return error(
              "bad real greater or equal {} >= {}", temp_double, included_min);

        return true;
    }

    bool is_i64_less_than(int excluded_max) noexcept
    {
        if (temp_i64 >= excluded_max)
            return error(
              "bad integer less than {} < {}", temp_i64, excluded_max);

        return true;
    }

    bool is_i64_greater_equal_than(int included_min) noexcept
    {
        if (temp_i64 < included_min)
            return error(
              "bad integer greater or equal {} >= {}", temp_i64, included_min);

        return true;
    }

    bool is_value_array_size_equal(const rapidjson::Value& val, int to) noexcept
    {
        debug::ensure(val.IsArray());

        if (std::cmp_equal(val.GetArray().Size(), to))
            return true;

        return error("to many elements in array ({})", to);
    }

    bool is_value_array(const rapidjson::Value& val) noexcept
    {
        if (!val.IsArray())
            return error("value is not an array");

        return true;
    }

    bool copy_array_size(const rapidjson::Value& val, i64& dst) noexcept
    {
        debug::ensure(val.IsArray());

        dst = static_cast<i64>(val.GetArray().Size());

        return true;
    }

    bool is_value_array_size_less(const rapidjson::Value& val,
                                  std::integral auto      i) noexcept
    {
        debug::ensure(val.IsArray());

        if (std::cmp_less(val.GetArray().Size(), i))
            return true;

        return error(
          "too many elements in array ({} vs {})", i, val.GetArray().Size());
    }

    bool is_value_object(const rapidjson::Value& val) noexcept
    {
        if (!val.IsObject())
            return error("json value is not an array");

        return true;
    }

    bool affect_configurable_to(bitflags<child_flags>& flag) const noexcept
    {
        flag.set(child_flags::configurable, temp_bool);

        return true;
    }

    bool affect_observable_to(bitflags<child_flags>& flag) const noexcept
    {
        flag.set(child_flags::observable, temp_bool);

        return true;
    }

    bool read_dynamics(const rapidjson::Value& val,
                       qss_integrator_tag,
                       parameter& p) noexcept
    {
        auto_stack a(this, "dynamics qss integrator");

        static constexpr std::string_view n[] = { "X", "dQ" };

        return for_members(val, n, [&](auto idx, const auto& value) noexcept {
            switch (idx) {
            case 0:
                return read_real(value, p.reals[0]);
            case 1:
                return read_real(value, p.reals[1]);
            default:
                return error("unknown element");
            }
        });
    }

    bool read_dynamics(const rapidjson::Value& val,
                       qss_multiplier_tag,
                       parameter& p) noexcept
    {
        auto_stack a(this, "dynamics qss multiplier");

        static constexpr std::string_view n[] = { "value-0", "value-1" };

        return for_members(
          val, n, [&](const auto idx, const auto& value) noexcept -> bool {
              switch (idx) {
              case 0:
                  return read_real(value, p.reals[0]);
              case 1:
                  return read_real(value, p.reals[1]);
              default:
                  return error("unknown element");
              }
          });
    }

    bool read_dynamics(const rapidjson::Value& val,
                       qss_sum_2_tag,
                       parameter& p) noexcept
    {
        auto_stack a(this, "dynamics_qss_sum");

        static constexpr std::string_view n[] = { "value-0", "value-1" };

        return for_members(
          val, n, [&](const auto idx, const auto& value) noexcept -> bool {
              switch (idx) {
              case 0:
                  return read_real(value, p.reals[0]);
              case 1:
                  return read_real(value, p.reals[1]);
              default:
                  return error("unknown element");
              }
          });
    }

    bool read_dynamics(const rapidjson::Value& val,
                       qss_sum_3_tag,
                       parameter& p) noexcept
    {
        auto_stack a(this, "dynamics qss sum");

        static constexpr std::string_view n[] = { "value-0",
                                                  "value-1",
                                                  "value-2" };

        return for_members(
          val, n, [&](const auto idx, const auto& value) noexcept -> bool {
              switch (idx) {
              case 0:
                  return read_real(value, p.reals[0]);
              case 1:
                  return read_real(value, p.reals[1]);
              case 2:
                  return read_real(value, p.reals[2]);
              default:
                  return error("unknown element");
              }
          });
    }

    bool read_dynamics(const rapidjson::Value& val,
                       qss_sum_4_tag,
                       parameter& p) noexcept
    {
        auto_stack a(this, "dynamics qss sum");

        static constexpr std::string_view n[] = {
            "value-0", "value-1", "value-2", "value-3"
        };

        return for_members(
          val, n, [&](const auto idx, const auto& value) noexcept -> bool {
              switch (idx) {
              case 0:
                  return read_real(value, p.reals[0]);
              case 1:
                  return read_real(value, p.reals[1]);
              case 2:
                  return read_real(value, p.reals[2]);
              case 3:
                  return read_real(value, p.reals[3]);
              default:
                  return error("unknown element");
              }
          });
    }

    bool read_dynamics(const rapidjson::Value& val,
                       qss_wsum_2_tag,
                       parameter& p) noexcept
    {
        auto_stack a(this, "dynamics qss wsum 2");

        static constexpr std::string_view n[] = {
            "coeff-0", "coeff-1", "value-0", "value-1"
        };

        return for_members(
          val, n, [&](const auto idx, const auto& value) noexcept -> bool {
              switch (idx) {
              case 0:
                  return read_real(value, p.reals[2]);
              case 1:
                  return read_real(value, p.reals[3]);
              case 2:
                  return read_real(value, p.reals[0]);
              case 3:
                  return read_real(value, p.reals[1]);
              default:
                  return error("unknown element");
              }
          });
    }

    bool read_dynamics(const rapidjson::Value& val,
                       qss_wsum_3_tag,
                       parameter& p) noexcept
    {
        auto_stack a(this, "dynamics qss wsum 3");

        static constexpr std::string_view n[] = { "coeff-0", "coeff-1",
                                                  "coeff-2", "value-0",
                                                  "value-1", "value-2" };

        return for_members(
          val, n, [&](const auto idx, const auto& value) noexcept -> bool {
              switch (idx) {
              case 0:
                  return read_real(value, p.reals[3]);
              case 1:
                  return read_real(value, p.reals[4]);
              case 2:
                  return read_real(value, p.reals[5]);
              case 3:
                  return read_real(value, p.reals[0]);
              case 4:
                  return read_real(value, p.reals[1]);
              case 5:
                  return read_real(value, p.reals[2]);
              default:
                  return error("unknown element");
              }
          });
    }

    bool read_dynamics(const rapidjson::Value& val,
                       qss_wsum_4_tag,
                       parameter& p) noexcept
    {
        auto_stack a(this, "dynamics qss wsum 4");

        static constexpr std::string_view n[] = { "coeff-0", "coeff-1",
                                                  "coeff-2", "coeff-3",
                                                  "value-0", "value-1",
                                                  "value-2", "value-3" };

        return for_members(
          val, n, [&](const auto idx, const auto& value) noexcept -> bool {
              switch (idx) {
              case 0:
                  return read_real(value, p.reals[4]);
              case 1:
                  return read_real(value, p.reals[5]);
              case 2:
                  return read_real(value, p.reals[6]);
              case 3:
                  return read_real(value, p.reals[7]);
              case 4:
                  return read_real(value, p.reals[0]);
              case 5:
                  return read_real(value, p.reals[1]);
              case 6:
                  return read_real(value, p.reals[2]);
              case 7:
                  return read_real(value, p.reals[3]);
              default:
                  return error("unknown element");
              }
          });
    }

    bool read_dynamics(const rapidjson::Value& /*val*/,
                       qss_invert_tag,
                       parameter& /*p*/) noexcept
    {
        auto_stack a(this, "dynamics invert function");

        return true;
    }

    bool read_dynamics(const rapidjson::Value& /*val*/,
                       qss_integer_tag,
                       parameter& /*p*/) noexcept
    {
        auto_stack a(this, "dynamics integer function");

        return true;
    }

    bool read_dynamics(const rapidjson::Value& val,
                       qss_compare_tag,
                       parameter& p) noexcept
    {
        auto_stack a(this, "dynamics compare");

        static constexpr std::string_view n[] = {
            "a", "a-less-b", "b", "not-a-less-b"
        };

        return for_members(val, n, [&](auto idx, const auto& value) noexcept {
            switch (idx) {
            case 0:
                return read_real(value, p.reals[0]);
            case 1:
                return read_real(value, p.reals[2]);
            case 2:
                return read_real(value, p.reals[1]);
            case 3:
                return read_real(value, p.reals[3]);

            default:
                return error("unknown element");
            }
        });

        return true;
    }

    bool read_dynamics(const rapidjson::Value& /*val*/,
                       counter_tag,
                       parameter& /*p*/) noexcept
    {
        auto_stack a(this, "dynamics counter");

        return true;
    }

    bool read_dynamics(const rapidjson::Value& val,
                       queue_tag,
                       parameter& p) noexcept
    {
        auto_stack a(this, "dynamics queue");

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("ta"sv == name)
                  return read_temp_real(value) && is_double_greater_than(0.0) &&
                         copy_real_to(p.reals[0]);

              return error("unknown element");
          });
    }

    bool copy_to_source(const std::optional<source::source_type> type,
                        const std::optional<u64>                 id,
                        source&                                  src)
    {
        if (not type.has_value() or not id.has_value())
            return true;

        switch (*type) {
        case source::source_type::binary_file:
            if (const auto* ptr = self.binary_file_mapping.get(*id))
                src = source(*ptr);
            break;

        case source::source_type::constant:
            if (const auto* ptr = self.constant_mapping.get(*id))
                src = source(*ptr);
            break;

        case source::source_type::random:
            if (const auto* ptr = self.random_mapping.get(*id))
                src = source(*ptr);
            break;

        case source::source_type::text_file:
            if (const auto* ptr = self.text_file_mapping.get(*id))
                src = source(*ptr);
            break;
        }

        return true;
    }

    bool copy_to_source(const std::optional<source::source_type> type,
                        const std::optional<u64>                 id,
                        i64&                                     t,
                        i64&                                     i) noexcept
    {
        if (not type.has_value() or not id.has_value())
            return true;

        switch (*type) {
        case source::source_type::binary_file:
            if (const auto* ptr = self.binary_file_mapping.get(*id)) {
                t = ordinal(*type);
                i = ordinal(*ptr);
            }
            break;

        case source::source_type::constant:
            if (const auto* ptr = self.constant_mapping.get(*id)) {
                t = ordinal(*type);
                i = ordinal(*ptr);
            }
            break;

        case source::source_type::random:
            if (const auto* ptr = self.random_mapping.get(*id)) {
                t = ordinal(*type);
                i = ordinal(*ptr);
            }
            break;

        case source::source_type::text_file:
            if (const auto* ptr = self.text_file_mapping.get(*id)) {
                t = ordinal(*type);
                i = ordinal(*ptr);
            }
            break;
        }

        return true;
    }

    bool read_dynamics(const rapidjson::Value& val,
                       dynamic_queue_tag,
                       parameter& p) noexcept
    {
        auto_stack a(this, "dynamics_dynamic_queue");

        std::optional<source::source_type> type;
        std::optional<u64>                 id;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("source-ta-type"sv == name)
                         return read_temp_i64(value) && copy_i64_to(type);

                     if ("source-ta-id"sv == name)
                         return read_temp_u64(value) && copy_u64_to(id);

                     return error("unknown element");
                 }) and
               copy_to_source(type, id, p.integers[0], p.integers[1]);
    }

    bool read_dynamics(const rapidjson::Value& val,
                       priority_queue_tag,
                       parameter& p) noexcept
    {
        auto_stack a(this, "dynamics priority queue");

        std::optional<source::source_type> type;
        std::optional<u64>                 id;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("source-ta-type"sv == name)
                         return read_temp_i64(value) && copy_i64_to(type);

                     if ("source-ta-id"sv == name)
                         return read_temp_u64(value) && copy_u64_to(id);

                     return error("unknown element");
                 }) and
               copy_to_source(type, id, p.integers[0], p.integers[1]);
    }

    bool copy_to_generator_options(bool ta_use_source,
                                   bool value_use_source,
                                   i64& options)
    {
        bitflags<generator::option> flags;

        if (ta_use_source)
            flags.set(generator::option::ta_use_source);
        if (value_use_source)
            flags.set(generator::option::value_use_source);

        options = static_cast<i64>(flags.to_unsigned());

        return true;
    }

    bool read_dynamics(const rapidjson::Value& val,
                       generator_tag,
                       parameter& p) noexcept
    {
        auto_stack a(this, "dynamics generator");

        std::optional<source::source_type> type_ta;
        std::optional<source::source_type> type_value;
        std::optional<u64>                 id_ta;
        std::optional<u64>                 id_value;

        auto ta_use_source    = false;
        auto value_use_source = false;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("offset"sv == name)
                         return read_temp_real(value) &&
                                is_double_greater_equal_than(0.0) &&
                                copy_real_to(p.reals[0]);

                     if ("ta-is-external"sv == name)
                         return read_bool(value, ta_use_source);

                     if ("value-is-external"sv == name)
                         return read_bool(value, value_use_source);

                     if ("source-ta-type"sv == name)
                         return read_temp_i64(value) && copy_i64_to(type_ta);

                     if ("source-ta-id"sv == name)
                         return read_temp_u64(value) && copy_u64_to(id_ta);

                     if ("source-value-type"sv == name)
                         return read_temp_i64(value) && copy_i64_to(type_value);

                     if ("source-value-id"sv == name)
                         return read_temp_u64(value) && copy_u64_to(id_value);

                     return error("unknown element");
                 }) and
               copy_to_source(type_ta, id_ta, p.integers[1], p.integers[2]) and
               copy_to_source(
                 type_value, id_value, p.integers[3], p.integers[4]) and
               copy_to_generator_options(
                 ta_use_source, value_use_source, p.integers[0]);
    }

    bool copy_string_to_constant_port(component&                compo,
                                      const constant::init_type type,
                                      parameter&                p) noexcept

    {
        if (type != constant::init_type::incoming_component_n and
            type != constant::init_type::outcoming_component_n)
            return error("constant port not necessary");

        const auto port = type == constant::init_type::incoming_component_n
                            ? compo.get_or_add_x(temp_string)
                            : compo.get_or_add_y(temp_string);

        p.integers[1] = ordinal(port);

        return true;
    }

    bool read_modeling_dynamics(const rapidjson::Value& val,
                                component&              compo,
                                constant_tag,
                                parameter& p) noexcept
    {
        auto_stack a(this, "dynamics constant");

        std::optional<constant::init_type> type;

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("value"sv == name)
                  return read_temp_real(value) && copy_real_to(p.reals[0]);

              if ("offset"sv == name)
                  return read_temp_real(value) &&
                         is_double_greater_equal_than(0.0) &&
                         copy_real_to(p.reals[1]);

              if ("type"sv == name)
                  return read_temp_string(value) && copy_string_to(type) &&
                         copy(ordinal(*type), p.integers[0]);

              if ("port"sv == name and type.has_value())
                  return read_temp_string(value) &&
                         copy_string_to_constant_port(compo, *type, p);

              return true;
          });
    }

    bool read_dynamics(const rapidjson::Value& val,
                       qss_cross_tag,
                       parameter& p) noexcept
    {
        auto_stack a(this, "dynamics qss cross");

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("threshold"sv == name)
                  return read_temp_real(value) && copy_real_to(p.reals[0]);

              if ("detect-up"sv == name)
                  return read_bool(value, p.integers[0]);

              return error("unknown element");
          });
    }

    bool read_dynamics(const rapidjson::Value& val,
                       qss_filter_tag,
                       parameter& p) noexcept
    {
        auto_stack a(this, "dynamics_qss_filter");

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("lower-threshold"sv == name)
                  return read_temp_real(value) && copy_real_to(p.reals[0]);
              if ("upper-threshold"sv == name)
                  return read_temp_real(value) && copy_real_to(p.reals[1]);

              return error("unknown element");
          });
    }

    bool read_dynamics(const rapidjson::Value& val,
                       qss_power_tag,
                       parameter& p) noexcept
    {
        auto_stack a(this, "dynamics qss power");

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("n"sv == name)
                  return read_temp_real(value) && copy_real_to(p.reals[0]);

              return error("unknown element");
          });
    }

    bool read_dynamics(const rapidjson::Value& /*val*/,
                       qss_square_tag,
                       parameter& /*p*/) noexcept
    {
        auto_stack a(this, "dynamics qss square");

        return true;
    }

    bool read_dynamics(const rapidjson::Value& /*val*/,
                       accumulator_2_tag,
                       parameter& /*p*/) noexcept
    {
        auto_stack a(this, "dynamics accumulator 2");

        return true;
    }

    bool read_dynamics(const rapidjson::Value& val,
                       time_func_tag,
                       parameter& p) noexcept
    {
        auto_stack a(this, "dynamics time func");
        temp_string.clear();

        auto ret = for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("function"sv == name)
                  return read_temp_string(value);

              if ("offset"sv == name)
                  return read_real(value, p.reals[0]);

              if ("timestep"sv == name)
                  return read_real(value, p.reals[1]);

              return error("unknown element");
          });

        if (ret) {
            if (temp_string == "time"sv)
                p.integers[0] = 0;
            else if (temp_string == "square"sv)
                p.integers[0] = 1;
            else
                p.integers[0] = 2;
        }
        return ret;
    }

    bool read_dynamics(const rapidjson::Value& val,
                       logical_and_2_tag,
                       parameter& p) noexcept
    {
        auto_stack a(this, "dynamics logical and 2");

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("value-0"sv == name)
                  if ("value-0"sv == name)
                      return read_bool(value, p.integers[0]);

              if ("value-1"sv == name)
                  return read_bool(value, p.integers[1]);

              return error("unknown element");
          });
    }

    bool read_dynamics(const rapidjson::Value& val,
                       logical_or_2_tag,
                       parameter& p) noexcept
    {
        auto_stack a(this, "dynamics logical or 2");

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("value-0"sv == name)
                  return read_bool(value, p.integers[0]);

              if ("value-1"sv == name)
                  return read_bool(value, p.integers[1]);

              return error("unknown element");
          });
    }

    bool read_dynamics(const rapidjson::Value& val,
                       logical_and_3_tag,
                       parameter& p) noexcept
    {
        auto_stack a(this, "dynamics logical and 3");

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("value-0"sv == name)
                  return read_bool(value, p.integers[0]);

              if ("value-1"sv == name)
                  return read_bool(value, p.integers[1]);

              if ("value-2"sv == name)
                  return read_bool(value, p.integers[2]);

              return error("unknown element");
          });
    }

    bool read_dynamics(const rapidjson::Value& val,
                       logical_or_3_tag,
                       parameter& p) noexcept
    {
        auto_stack a(this, "dynamics logical or 3");

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("value-0"sv == name)
                  return read_bool(value, p.integers[0]);

              if ("value-1"sv == name)
                  return read_bool(value, p.integers[1]);

              if ("value-2"sv == name)
                  return read_bool(value, p.integers[2]);

              return error("unknown element");
          });
    }

    bool read_dynamics(const rapidjson::Value& /*val*/,
                       logical_invert_tag,
                       parameter& /*p*/) noexcept
    {
        auto_stack a(this, "dynamics logical invert");

        return true;
    }

    bool read_hsm_condition_action(
      const rapidjson::Value&                       val,
      hierarchical_state_machine::condition_action& s) noexcept
    {
        auto_stack          a(this, "dynamics hsm condition action");
        std::optional<u8>   port;
        std::optional<u8>   mask;
        std::optional<i32>  i;
        std::optional<real> f;

        auto ret = for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("var-1"sv == name)
                  return read_temp_i64(value) && is_i64_greater_equal_than(0) &&
                         is_i64_less_than(
                           hierarchical_state_machine::variable_count) &&
                         copy_i64_to(s.var1);

              if ("var-2"sv == name)
                  return read_temp_i64(value) && is_i64_greater_equal_than(0) &&
                         is_i64_less_than(
                           hierarchical_state_machine::variable_count) &&
                         copy_i64_to(s.var2);

              if ("type"sv == name)
                  return read_temp_i64(value) && is_i64_greater_equal_than(0) &&
                         is_i64_less_than(
                           hierarchical_state_machine::condition_type_count) &&
                         copy_i64_to(s.type);

              if ("port"sv == name)
                  return read_temp_u64(value) && copy_u64_to(port);

              if ("mask"sv == name)
                  return read_temp_u64(value) && copy_u64_to(mask);

              if ("constant-i"sv == name)
                  return read_temp_i64(value) && copy_i64_to(i);

              if ("constant-r"sv == name)
                  return read_temp_real(value) && copy_real_to(f);

              return error("unknown element");
          });

        if (not ret)
            return ret;

        if (port.has_value() and mask.has_value())
            s.set(*port, *mask);
        else if (i.has_value())
            s.constant.i = *i;
        else if (f.has_value())
            s.constant.f = static_cast<float>(*f);

        return true;
    }

    bool read_hsm_state_action(
      const rapidjson::Value&                   val,
      hierarchical_state_machine::state_action& s) noexcept
    {
        auto_stack          a(this, "dynamics hsm state action");
        std::optional<i32>  i;
        std::optional<real> f;

        auto ret = for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("var-1"sv == name)
                  return read_temp_i64(value) && is_i64_greater_equal_than(0) &&
                         is_i64_less_than(
                           hierarchical_state_machine::variable_count) &&
                         copy_i64_to(s.var1);

              if ("var-2"sv == name)
                  return read_temp_i64(value) && is_i64_greater_equal_than(0) &&
                         is_i64_less_than(
                           hierarchical_state_machine::variable_count) &&
                         copy_i64_to(s.var2);

              if ("type"sv == name)
                  return read_temp_i64(value) && is_i64_greater_equal_than(0) &&
                         is_i64_less_than(
                           hierarchical_state_machine::action_type_count) &&
                         copy_i64_to(s.type);

              if ("constant-i"sv == name)
                  return read_temp_i64(value) && copy_i64_to(i);

              if ("constant-r"sv == name)
                  return read_temp_real(value) && copy_real_to(f);

              return error("unknown element");
          });

        if (not ret)
            return ret;

        if (i.has_value())
            s.constant.i = *i;
        else if (f.has_value())
            s.constant.f = static_cast<float>(*f);

        return true;
    }

    bool read_hsm_state(
      const rapidjson::Value&                                      val,
      std::array<hierarchical_state_machine::state,
                 hierarchical_state_machine::max_number_of_state>& states,
      std::array<name_str, hierarchical_state_machine::max_number_of_state>&
        names,
      std::array<position, hierarchical_state_machine::max_number_of_state>&
        positions) noexcept
    {
        auto_stack a(this, "dynamics hsm state");

        std::optional<int>                id;
        hierarchical_state_machine::state s;
        name_str                          state_name;
        position                          pos;

        if (not for_each_member(
              val,
              [&](const auto name, const auto& value) noexcept -> bool {
                  if ("id"sv == name)
                      return read_temp_i64(value) and copy_i64_to(id);

                  if ("name"sv == name)
                      return read_temp_string(value) and
                             copy_string_to(state_name);

                  if ("enter"sv == name)
                      return read_hsm_state_action(value, s.enter_action);

                  if ("exit"sv == name)
                      return read_hsm_state_action(value, s.exit_action);

                  if ("if"sv == name)
                      return read_hsm_state_action(value, s.if_action);

                  if ("else"sv == name)
                      return read_hsm_state_action(value, s.else_action);

                  if ("condition"sv == name)
                      return read_hsm_condition_action(value, s.condition);

                  if ("if-transition"sv == name)
                      return read_temp_u64(value) &&
                             copy_u64_to(s.if_transition);

                  if ("else-transition"sv == name)
                      return read_temp_u64(value) &&
                             copy_u64_to(s.else_transition);

                  if ("super-id"sv == name)
                      return read_temp_u64(value) && copy_u64_to(s.super_id);

                  if ("sub-id"sv == name)
                      return read_temp_u64(value) && copy_u64_to(s.sub_id);

                  if ("x"sv == name)
                      return read_temp_real(value) && copy_real_to(pos.x);

                  if ("y"sv == name)
                      return read_temp_real(value) && copy_real_to(pos.y);

                  return error("unknown element");
              }) or
            not id.has_value())
            return false;

        states[*id]    = s;
        names[*id]     = state_name;
        positions[*id] = pos;

        return true;
    }

    bool read_hsm_state(
      const rapidjson::Value& val,
      std::array<hierarchical_state_machine::state,
                 hierarchical_state_machine::max_number_of_state>&
        states) noexcept
    {
        auto_stack a(this, "dynamics hsm state");

        std::optional<int>                id;
        hierarchical_state_machine::state s;

        if (not for_each_member(
              val,
              [&](const auto name, const auto& value) noexcept -> bool {
                  if ("id"sv == name)
                      return read_temp_i64(value) and copy_i64_to(id);

                  if ("enter"sv == name)
                      return read_hsm_state_action(value, s.enter_action);

                  if ("exit"sv == name)
                      return read_hsm_state_action(value, s.exit_action);

                  if ("if"sv == name)
                      return read_hsm_state_action(value, s.if_action);

                  if ("else"sv == name)
                      return read_hsm_state_action(value, s.else_action);

                  if ("condition"sv == name)
                      return read_hsm_condition_action(value, s.condition);

                  if ("if-transition"sv == name)
                      return read_temp_u64(value) &&
                             copy_u64_to(s.if_transition);

                  if ("else-transition"sv == name)
                      return read_temp_u64(value) &&
                             copy_u64_to(s.else_transition);

                  if ("super-id"sv == name)
                      return read_temp_u64(value) && copy_u64_to(s.super_id);

                  if ("sub-id"sv == name)
                      return read_temp_u64(value) && copy_u64_to(s.sub_id);

                  return error("unknown element");
              }) or
            not id.has_value())
            return false;

        states[*id] = s;

        return true;
    }

    bool read_hsm_states(
      const rapidjson::Value&                                      val,
      std::array<hierarchical_state_machine::state,
                 hierarchical_state_machine::max_number_of_state>& states,
      std::array<name_str, hierarchical_state_machine::max_number_of_state>&
        names,
      std::array<position, hierarchical_state_machine::max_number_of_state>&
        positions) noexcept
    {
        auto_stack a(this, "dynamics hsm states");

        return for_each_array(
          val, [&](const auto /*i*/, const auto& value) noexcept -> bool {
              return read_hsm_state(value, states, names, positions);
          });
    }

    bool read_hsm_states(
      const rapidjson::Value& val,
      std::array<hierarchical_state_machine::state,
                 hierarchical_state_machine::max_number_of_state>&
        states) noexcept
    {
        auto_stack a(this, "dynamics hsm states");

        return for_each_array(
          val, [&](const auto /*i*/, const auto& value) noexcept -> bool {
              return read_hsm_state(value, states);
          });
    }

    bool try_modeling_copy_component_id(const small_string<31>&   reg,
                                        const directory_path_str& dir,
                                        const file_path_str&      file,
                                        component_id&             c_id)
    {
        registred_path* reg_ptr  = search_reg(reg.sv());
        dir_path*       dir_ptr  = nullptr;
        file_path*      file_ptr = nullptr;

        if (reg_ptr)
            dir_ptr = search_dir_in_reg(*reg_ptr, dir.sv());

        if (!dir_ptr)
            dir_ptr = search_dir(dir.sv());

        if (dir_ptr)
            file_ptr = search_file(*dir_ptr, file.sv());

        if (file_ptr) {
            c_id = file_ptr->component;
            debug::ensure(mod().components.exists(c_id));

            if (auto* c = mod().components.try_to_get<component>(c_id); c) {
                if (c->state == component_status::unmodified)
                    return true;

                if (auto ret = mod().load_component(*c); ret)
                    return true;
            }
        }

        return false;
    }

    bool try_read_child_hsm_component(const rapidjson::Value& val,
                                      component_id&           c_id) noexcept
    {
        auto_stack a(this, "child simple or grid component");

        name_str           reg_name;
        directory_path_str dir_path;
        file_path_str      file_path;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("path"sv == name)
                         return read_temp_string(value) &&
                                copy_string_to(reg_name);

                     if ("directory"sv == name)
                         return read_temp_string(value) &&
                                copy_string_to(dir_path);

                     if ("file"sv == name)
                         return read_temp_string(value) &&
                                copy_string_to(file_path);

                     return true;
                 }) &&
               try_modeling_copy_component_id(
                 reg_name, dir_path, file_path, c_id);
    }

    bool read_modeling_dynamics(const rapidjson::Value& val,
                                hsm_wrapper_tag,
                                parameter& p) noexcept
    {
        auto_stack a(this, "dynamics hsm");

        static constexpr std::string_view n[] = {
            "hsm", "i1", "i2", "r1", "r2", "source-id", "source-type", "timeout"
        };

        std::optional<source::source_type> type;
        std::optional<u64>                 id;

        return for_members(
                 val,
                 n,
                 [&](auto idx, const auto& value) noexcept {
                     switch (idx) {
                     case 0: {
                         component_id c;
                         if (try_read_child_hsm_component(value, c)) {
                             p.integers[0] = static_cast<i64>(c);
                             return true;
                         } else {
                             warning("HSM component not found");
                             return true;
                         }
                     }
                     case 1:
                         return read_temp_i64(value) &&
                                copy_i64_to(p.integers[1]);
                     case 2:
                         return read_temp_i64(value) &&
                                copy_i64_to(p.integers[2]);
                     case 3:
                         return read_temp_real(value) &&
                                copy_real_to(p.reals[0]);
                     case 4:
                         return read_temp_real(value) &&
                                copy_real_to(p.reals[1]);
                     case 5:
                         return read_temp_i64(value) && copy_i64_to(type);
                     case 6:
                         return read_temp_u64(value) && copy_u64_to(id);
                     case 7:
                         return read_temp_real(value) &&
                                copy_real_to(p.reals[2]);
                     default:
                         return error("unknown element");
                     }
                 }) and
               ((optional_has_value(type) and optional_has_value(id) and
                 copy_to_source(type, id, p.integers[3], p.integers[4])) or
                true);
    }

    ////

    template<std::integral T, std::integral R>
    bool copy(T from, R& to) noexcept
    {
        if (std::cmp_greater_equal(from, std::numeric_limits<R>::min()) &&
            std::cmp_less_equal(from, std::numeric_limits<R>::max())) {
            to = from;
            return true;
        }

        return error("integer convertion error");
    }

    template<typename T>
    bool copy(const u64 from, T& id) noexcept
    {
        if constexpr (std::is_enum_v<T>) {
            id = enum_cast<T>(from);
            return true;
        }

        if constexpr (std::is_integral_v<T>) {
            if (is_numeric_castable<T>(id)) {
                id = numeric_cast<T>(id);
                return true;
            }

            return error("integer convertion error from {}", from);
        }

        if constexpr (std::is_same_v<T, std::optional<u64>>) {
            id = from;
            return true;
        }

        return error("integer convertion error {}", from);
    }

    template<typename T>
    bool copy(const T from, u64& id) noexcept
    {
        if constexpr (std::is_enum_v<T>) {
            id = ordinal(from);
            return true;
        }

        if constexpr (std::is_integral_v<T>) {
            if (is_numeric_castable<u64>(id)) {
                id = numeric_cast<u64>(id);
                return true;
            }

            return error("integer convertion error from {}", from);
        }

        if constexpr (std::is_same_v<T, std::optional<u64>>) {
            if (from.has_value()) {
                id = *from;
                return true;
            }

            return error("integer convertion error from {}", from);
        }

        return error("integer convertion error from {}", from);
    }

    bool fix_child_name(generic_component&        generic,
                        generic_component::child& c) noexcept
    {
        if (c.flags[child_flags::configurable] or
            c.flags[child_flags::observable]) {
            const auto id  = generic.children.get_id(c);
            const auto idx = get_index(id);

            if (generic.children_names[idx].empty())
                generic.children_names[idx] = generic.make_unique_name_id(id);
        }

        return true;
    }

    bool read_child(const rapidjson::Value&   val,
                    generic_component&        generic,
                    generic_component::child& c,
                    child_id                  c_id) noexcept
    {
        auto_stack a(this, "child");

        std::optional<u64> id;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("id"sv == name)
                         return read_temp_u64(value) && copy_u64_to(id);
                     if ("x"sv == name)
                         return read_temp_real(value) and
                                copy_real_to(
                                  generic.children_positions[c_id].x);
                     if ("y"sv == name)
                         return read_temp_real(value) and
                                copy_real_to(
                                  generic.children_positions[c_id].y);
                     if ("name"sv == name)
                         return read_temp_string(value) and
                                copy_string_to(generic.children_names[c_id]);
                     if ("configurable"sv == name)
                         return read_temp_bool(value) and
                                affect_configurable_to(c.flags);
                     if ("observable"sv == name)
                         return read_temp_bool(value) and
                                affect_observable_to(c.flags);

                     return true;
                 }) &&
               optional_has_value(id) &&
               cache_model_mapping_add(*id, ordinal(c_id)) &&
               fix_child_name(generic, c);
    }

    bool read_child_model_dynamics(const rapidjson::Value&   val,
                                   component&                compo,
                                   generic_component&        gen,
                                   generic_component::child& c) noexcept
    {
        auto_stack a(this, "child model dynamics");

        const auto c_id  = gen.children.get_id(c);
        const auto c_idx = get_index(c_id);

        auto& param = gen.children_parameters[c_idx];
        param.clear();

        return for_first_member(
          val, "dynamics"sv, [&](const auto& value) noexcept -> bool {
              return dispatch(
                c.id.mdl_type,
                [&]<typename Tag>(const Tag tag) noexcept -> bool {
                    if constexpr (std::is_same_v<Tag, hsm_wrapper_tag>) {
                        return read_modeling_dynamics(value, tag, param);
                    } else if constexpr (std::is_same_v<Tag, constant_tag>) {
                        return read_modeling_dynamics(value, compo, tag, param);
                    } else {
                        return read_dynamics(value, tag, param);
                    }
                });
          });
    }

    bool read_child_model(const rapidjson::Value&   val,
                          component&                compo,
                          generic_component&        gen,
                          const dynamics_type       type,
                          generic_component::child& c) noexcept
    {
        auto_stack a(this, "child model");

        c.type        = child_type::model;
        c.id.mdl_type = type;

        return read_child_model_dynamics(val, compo, gen, c);
    }

    auto search_reg(std::string_view name) const noexcept -> registred_path*
    {
        registred_path* reg = nullptr;
        while (mod().registred_paths.next(reg))
            if (name == reg->name.sv())
                return reg;

        return nullptr;
    }

    auto search_dir_in_reg(registred_path& reg, std::string_view name) noexcept
      -> dir_path*
    {
        for (auto dir_id : reg.children) {
            if (auto* dir = mod().dir_paths.try_to_get(dir_id); dir) {
                if (name == dir->path.sv())
                    return dir;
            }
        }

        return nullptr;
    }

    bool search_dir(std::string_view name, dir_path_id& out) noexcept
    {
        auto_stack s(this, "search directory");

        for (auto reg_id : mod().component_repertories) {
            if (auto* reg = mod().registred_paths.try_to_get(reg_id); reg) {
                for (auto dir_id : reg->children) {
                    if (auto* dir = mod().dir_paths.try_to_get(dir_id); dir) {
                        if (dir->path.sv() == name) {
                            out = dir_id;
                            return true;
                        }
                    }
                }
            }
        }

        return error("directory not found");
    }

    bool search_file(dir_path_id      id,
                     std::string_view file_name,
                     file_path_id&    out) noexcept
    {
        auto_stack s(this, "search file in directory");

        if (auto* dir = mod().dir_paths.try_to_get(id); dir) {
            for (int i = 0, e = dir->children.ssize(); i != e; ++i) {
                if (auto* f = mod().file_paths.try_to_get(dir->children[i]);
                    f) {
                    if (f->path.sv() == file_name) {
                        out = dir->children[i];
                        return true;
                    }
                }
            }
        }

        return error("file not found");
    }

    /**
     * Search a directory named @a dir_name from the recorded path @a reg.
     * @param reg The recorded path used to search the @a dir_name.
     * @param dir_name The directory name to search.
     */
    auto search_dir(const registred_path&  reg,
                    const std::string_view dir_name) const noexcept -> dir_path*
    {
        for (auto dir_id : reg.children) {
            if (auto* dir = mod().dir_paths.try_to_get(dir_id); dir) {
                if (dir->path.sv() == dir_name)
                    return dir;
            }
        }

        return nullptr;
    }

    /**
     * Search a directory named @a dir_name into the recorded path @a
     * reg_path if it exists.
     * @param reg_path The recorded path to search.
     * @param dir_name The directory name to search.
     */
    auto search_dir(const std::string_view reg_path,
                    const std::string_view dir_name) const noexcept -> dir_path*
    {
        for (auto reg_id : mod().component_repertories) {
            if (auto* reg = mod().registred_paths.try_to_get(reg_id);
                reg and reg->name.sv() == reg_path) {
                return search_dir(*reg, dir_name);
            }
        }

        return nullptr;
    }

    /**
     * Search a directory named @a dir_name into recorded path using the @a
     * recorded paths in @a modeling.
     * @param dir_name The directory name to search.
     */
    auto search_dir(const std::string_view dir_name) const noexcept -> dir_path*
    {
        for (auto reg_id : mod().component_repertories) {
            if (auto* reg = mod().registred_paths.try_to_get(reg_id); reg) {
                for (auto dir_id : reg->children) {
                    if (auto* dir = mod().dir_paths.try_to_get(dir_id); dir) {
                        if (dir->path.sv() == dir_name)
                            return dir;
                    }
                }
            }
        }

        return nullptr;
    }

    /**
     * Search a directory named @a dir_name into the recorded path @a
     * reg_path if it exists.
     * @param reg_path The recorded path to search.
     * @param dir_name The directory name to search.
     * @param [out] out Output parameter to store directory identifier
     * found.
     * @return true if a directory is found, false otherwise.
     */
    auto search_dir(const std::string_view reg_path,
                    const std::string_view dir_name,
                    dir_path_id&           out) noexcept -> bool
    {
        if (reg_path.empty())
            return search_dir(dir_name, out);

        if (auto* dir = search_dir(reg_path, dir_name)) {
            out = mod().dir_paths.get_id(*dir);
            return true;
        }

        return false;
    }

    auto search_file(dir_path& dir, std::string_view name) noexcept
      -> file_path*
    {
        for (auto file_id : dir.children)
            if (auto* file = mod().file_paths.try_to_get(file_id); file)
                if (file->path.sv() == name)
                    return file;

        return nullptr;
    }

    auto search_component(std::string_view name) const noexcept
      -> const component*
    {
        const auto& compo_vec = mod().components.get<component>();
        for (const auto id : mod().components)
            if (compo_vec[id].name.sv() == name)
                return std::addressof(compo_vec[id]);

        return nullptr;
    }

    bool modeling_copy_component_id(const small_string<31>&   reg,
                                    const directory_path_str& dir,
                                    const file_path_str&      file,
                                    component_id&             c_id)
    {
        registred_path* reg_ptr  = search_reg(reg.sv());
        dir_path*       dir_ptr  = nullptr;
        file_path*      file_ptr = nullptr;

        if (reg_ptr)
            dir_ptr = search_dir_in_reg(*reg_ptr, dir.sv());

        if (!dir_ptr)
            dir_ptr = search_dir(dir.sv());

        if (dir_ptr)
            file_ptr = search_file(*dir_ptr, file.sv());

        if (file_ptr) {
            c_id = file_ptr->component;
            debug::ensure(mod().components.exists(c_id));

            if (auto* c = mod().components.try_to_get<component>(c_id); c) {
                if (c->state == component_status::unmodified)
                    return true;

                if (auto ret = mod().load_component(*c); ret)
                    return true;
            }
        }

        return error("fail to found component");
    }

    bool read_child_simple_or_grid_component(const rapidjson::Value& val,
                                             component_id& c_id) noexcept
    {
        auto_stack a(this, "child simple or grid component");

        name_str           reg_name;
        directory_path_str dir_path;
        file_path_str      file_path;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("path"sv == name)
                         return read_temp_string(value) &&
                                copy_string_to(reg_name);

                     if ("directory"sv == name)
                         return read_temp_string(value) &&
                                copy_string_to(dir_path);

                     if ("file"sv == name)
                         return read_temp_string(value) &&
                                copy_string_to(file_path);

                     return true;
                 }) &&
               modeling_copy_component_id(reg_name, dir_path, file_path, c_id);
    }

    bool dispatch_child_component_type(const rapidjson::Value& val,
                                       component_type          type,
                                       component_id&           c_id) noexcept
    {
        auto_stack a(this, "dispatch child component type");

        switch (type) {
        case component_type::none:
            return true;

        case component_type::generic:
            return read_child_simple_or_grid_component(val, c_id);

        case component_type::grid:
            return read_child_simple_or_grid_component(val, c_id);

        case component_type::graph:
            return read_child_simple_or_grid_component(val, c_id);

        case component_type::hsm:
            return read_child_simple_or_grid_component(val, c_id);
        }

        return error("unknown element");
    }

    bool read_child_component(const rapidjson::Value& val,
                              component_id&           c_id) noexcept
    {
        auto_stack a(this, "read child component");

        component_type type = component_type::none;

        return for_first_member(
          val, "component-type", [&](const auto& value) noexcept -> bool {
              return read_temp_string(value) && copy_string_to(type) &&
                     dispatch_child_component_type(val, type, c_id);
          });
    }

    bool dispatch_child_component_or_model(const rapidjson::Value&   val,
                                           component&                compo,
                                           generic_component&        generic,
                                           dynamics_type             d_type,
                                           generic_component::child& c) noexcept
    {
        auto_stack a(this, "dispatch child component or model");

        return c.type == child_type::component
                 ? read_child_component(val, c.id.compo_id)
                 : read_child_model(val, compo, generic, d_type, c);
    }

    bool read_child_component_or_model(const rapidjson::Value&   val,
                                       component&                compo,
                                       generic_component&        generic,
                                       generic_component::child& c) noexcept
    {
        auto_stack a(this, "child component or model");

        dynamics_type type = dynamics_type::constant;

        return for_first_member(val,
                                "type"sv,
                                [&](const auto& value) noexcept -> bool {
                                    return read_temp_string(value) &&
                                           copy_string_to(c.type, type);
                                }) &&
               dispatch_child_component_or_model(val, compo, generic, type, c);
    }

    bool read_children_array(const rapidjson::Value& val,
                             component&              compo,
                             generic_component&      generic) noexcept
    {
        auto_stack a(this, "children array");

        i64 size = 0;

        return is_value_array(val) && copy_array_size(val, size) &&
               generic_can_alloc(generic, size) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     auto& new_child =
                       generic.children.alloc(undefined<component_id>());
                     auto new_child_id = generic.children.get_id(new_child);
                     return read_child(
                              value, generic, new_child, new_child_id) &&
                            read_child_component_or_model(
                              value, compo, generic, new_child);
                 });
    }

    bool cache_model_mapping_sort() noexcept
    {
        self.model_mapping.sort();

        return true;
    }

    bool read_children(const rapidjson::Value& val,
                       component&              compo,
                       generic_component&      generic) noexcept
    {
        auto_stack a(this, "children");

        return read_children_array(val, compo, generic) &&
               cache_model_mapping_sort();
    }

    bool constant_sources_can_alloc(external_source&   srcs,
                                    std::integral auto i) noexcept
    {
        if (srcs.constant_sources.can_alloc(i))
            return true;

        return error("can not allocate more constant source ({})",
                     srcs.constant_sources.capacity());
    }

    bool text_file_sources_can_alloc(external_source&   srcs,
                                     std::integral auto i) noexcept
    {
        if (srcs.text_file_sources.can_alloc(i))
            return true;

        return error("can not allocate more text file source ({})",
                     srcs.text_file_sources.capacity());
    }

    bool binary_file_sources_can_alloc(external_source&   srcs,
                                       std::integral auto i) noexcept
    {
        if (srcs.binary_file_sources.can_alloc(i))
            return true;

        return error("can not allocate more binary file source ({})",
                     srcs.binary_file_sources.capacity());
    }

    bool random_sources_can_alloc(external_source&   srcs,
                                  std::integral auto i) noexcept
    {
        if (srcs.random_sources.can_alloc(i))
            return true;

        return error("can not allocate more random source ({})",
                     srcs.random_sources.capacity());
    }

    bool constant_buffer_size_can_alloc(std::integral auto i) noexcept
    {
        if (std::cmp_greater_equal(i, 0) &&
            std::cmp_less_equal(i, external_source_chunk_size))
            return true;

        return error(
          "can not allocate more data in constant source 0 <= {} < {}",
          i,
          external_source_chunk_size);
    }

    bool read_constant_source(const rapidjson::Value& val,
                              constant_source&        src) noexcept
    {
        auto_stack s(this, "srcs constant source");

        i64 len = 0;

        return is_value_array(val) && copy_array_size(val, len) &&
               constant_buffer_size_can_alloc(len) &&
               for_each_array(
                 val, [&](const auto i, const auto& value) noexcept -> bool {
                     src.length = static_cast<u32>(i);
                     return read_temp_real(value) &&
                            copy_real_to(src.buffer[i]);
                 });
    }

    bool read_constant_sources(const rapidjson::Value& val,
                               external_source&        srcs) noexcept
    {
        auto_stack s(this, "srcs constant source");

        i64 len = 0;

        return is_value_array(val) && copy_array_size(val, len),
               constant_sources_can_alloc(srcs, len) &&
                 for_each_array(
                   val,
                   [&](const auto /*i*/, const auto& value) noexcept -> bool {
                       auto& cst = srcs.constant_sources.alloc();
                       auto  id  = srcs.constant_sources.get_id(cst);
                       std::optional<u32> id_in_file;
                       std::string        name;

                       return for_each_member(
                                value,
                                [&](const auto  name,
                                    const auto& value) noexcept -> bool {
                                    if ("id"sv == name)
                                        return read_temp_u64(value) &&
                                               copy_u64_to(id_in_file);

                                    if ("name"sv == name)
                                        return read_temp_string(value) &&
                                               copy_string_to(cst.name);

                                    if ("parameters"sv == name)
                                        return read_constant_source(value, cst);

                                    return true;
                                }) &&
                              optional_has_value(id_in_file) &&
                              cache_constant_mapping_add(*id_in_file, id);
                   });
    }

    bool search_file_from_dir_component(const component& compo,
                                        file_path_id&    out) const noexcept
    {
        out = get_file_from_component(mod(), compo, temp_string);

        return is_defined(out);
    }

    bool read_text_file_sources(const rapidjson::Value& val,
                                component&              compo) noexcept
    {
        auto_stack s(this, "srcs text file sources");

        i64 len = 0;

        return is_value_array(val) && copy_array_size(val, len) &&
               text_file_sources_can_alloc(compo.srcs, len) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     auto& text = compo.srcs.text_file_sources.alloc();
                     auto  id   = compo.srcs.text_file_sources.get_id(text);
                     std::optional<u64> id_in_file;

                     auto_stack s(this, "srcs text file source");

                     return for_each_member(
                              value,
                              [&](const auto  name,
                                  const auto& value) noexcept -> bool {
                                  if ("id"sv == name)
                                      return read_temp_u64(value) &&
                                             copy_u64_to(id_in_file);

                                  if ("name"sv == name)
                                      return read_temp_string(value) &&
                                             copy_string_to(text.name);

                                  if ("path"sv == name)
                                      return read_temp_string(value) &&
                                             search_file_from_dir_component(
                                               compo, text.file_id);

                                  return true;
                              }) &&
                            optional_has_value(id_in_file) &&
                            cache_text_file_mapping_add(*id_in_file, id);
                 });
    }

    bool read_text_file_sources(const rapidjson::Value& val,
                                external_source&        srcs) noexcept
    {
        auto_stack s(this, "srcs text file sources");

        i64 len = 0;

        return is_value_array(val) && copy_array_size(val, len) &&
               text_file_sources_can_alloc(srcs, len) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     auto& text = srcs.text_file_sources.alloc();
                     auto  id   = srcs.text_file_sources.get_id(text);
                     std::optional<u64> id_in_file;

                     auto_stack s(this, "srcs text file source");

                     return for_each_member(
                              value,
                              [&](const auto  name,
                                  const auto& value) noexcept -> bool {
                                  if ("id"sv == name)
                                      return read_temp_u64(value) &&
                                             copy_u64_to(id_in_file);

                                  if ("name"sv == name)
                                      return read_temp_string(value) &&
                                             copy_string_to(text.name);

                                  if ("path"sv == name)
                                      return read_temp_string(value) &&
                                             copy_string_to(text);

                                  return true;
                              }) &&
                            optional_has_value(id_in_file) &&
                            cache_text_file_mapping_add(*id_in_file, id);
                 });
    }

    bool cache_constant_mapping_add(u64                id_in_file,
                                    constant_source_id id) const noexcept
    {
        self.constant_mapping.data.emplace_back(id_in_file, id);
        return true;
    }

    bool cache_text_file_mapping_add(u64                 id_in_file,
                                     text_file_source_id id) noexcept
    {
        self.text_file_mapping.data.emplace_back(id_in_file, id);
        return true;
    }

    bool cache_binary_file_mapping_add(u64                   id_in_file,
                                       binary_file_source_id id) const noexcept
    {
        self.binary_file_mapping.data.emplace_back(id_in_file, id);
        return true;
    }

    bool cache_random_mapping_add(u64 id_in_file, random_source_id id) noexcept
    {
        self.random_mapping.data.emplace_back(id_in_file, id);
        return true;
    }

    bool cache_constant_mapping_sort() noexcept
    {
        self.constant_mapping.sort();
        return true;
    }

    bool cache_text_file_mapping_sort() noexcept
    {
        self.text_file_mapping.sort();
        return true;
    }

    bool cache_binary_file_mapping_sort() const noexcept
    {
        self.binary_file_mapping.sort();
        return true;
    }

    bool cache_random_mapping_sort() noexcept
    {
        self.random_mapping.sort();
        return true;
    }

    bool cache_constant_mapping_get(u64                 id_in_file,
                                    constant_source_id& id) noexcept
    {
        if (const auto* ptr = self.constant_mapping.get(id_in_file)) {
            id = *ptr;
            return true;
        }

        return error("unknown constant source (id: {})", id_in_file);
    }

    bool cache_text_file_mapping_get(u64                  id_in_file,
                                     text_file_source_id& id) noexcept
    {
        if (const auto* ptr = self.text_file_mapping.get(id_in_file)) {
            id = *ptr;
            return true;
        }

        return error("unknown text file source (id: {})", id_in_file);
    }

    bool cache_random_mapping_get(u64 id_in_file, random_source_id& id) noexcept
    {
        if (const auto* ptr = self.random_mapping.get(id_in_file)) {
            id = *ptr;
            return true;
        }

        return error("unknown random source (id: {})", id_in_file);
    }

    bool cache_binary_file_mapping_get(u64                    id_in_file,
                                       binary_file_source_id& id) noexcept
    {
        if (const auto* ptr = self.binary_file_mapping.get(id_in_file)) {
            id = *ptr;
            return true;
        }

        return error("unknown binary file source (id: {})", id_in_file);
    }

    bool read_binary_file_sources(const rapidjson::Value& val,
                                  component&              compo) noexcept
    {
        auto_stack s(this, "srcs binary file sources");

        i64 len = 0;

        return is_value_array(val) && copy_array_size(val, len) &&
               binary_file_sources_can_alloc(compo.srcs, len) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     auto& bin = compo.srcs.binary_file_sources.alloc();
                     auto  id  = compo.srcs.binary_file_sources.get_id(bin);
                     std::optional<u64> id_in_file;

                     auto_stack s(this, "srcs binary file source");

                     return for_each_member(
                              value,
                              [&](const auto  name,
                                  const auto& value) noexcept -> bool {
                                  if ("id"sv == name)
                                      return read_temp_u64(value) &&
                                             copy_u64_to(id_in_file);

                                  if ("name"sv == name)
                                      return read_temp_string(value) &&
                                             copy_string_to(bin.name);

                                  if ("path"sv == name)
                                      return read_temp_string(value) &&
                                             search_file_from_dir_component(
                                               compo, bin.file_id);

                                  return true;
                              }) &&
                            optional_has_value(id_in_file) &&
                            cache_binary_file_mapping_add(*id_in_file, id);
                 });
    }

    bool read_binary_file_sources(const rapidjson::Value& val,
                                  external_source&        srcs) noexcept
    {
        auto_stack s(this, "srcs binary file sources");

        i64 len = 0;

        return is_value_array(val) && copy_array_size(val, len) &&
               binary_file_sources_can_alloc(srcs, len) &&
               for_each_array(
                 val, [&](const auto /*i*/, const auto& elem) noexcept -> bool {
                     auto& text = srcs.binary_file_sources.alloc();
                     auto  id   = srcs.binary_file_sources.get_id(text);
                     std::optional<u64> id_in_file;

                     auto_stack s(this, "srcs binary file source");

                     return for_each_member(
                              elem,
                              [&](const auto  name,
                                  const auto& value) noexcept -> bool {
                                  if ("id"sv == name)
                                      return read_temp_u64(value) &&
                                             copy_u64_to(id_in_file);

                                  if ("name"sv == name)
                                      return read_temp_string(value) &&
                                             copy_string_to(text.name);

                                  if ("path"sv == name)
                                      return read_temp_string(value) &&
                                             copy_string_to(text);

                                  return true;
                              }) &&
                            optional_has_value(id_in_file) &&
                            cache_binary_file_mapping_add(*id_in_file, id);
                 });
    }

    bool read_distribution_type(const rapidjson::Value& val,
                                random_source&          r) noexcept
    {
        auto_stack s(this, "srcs random source distribution");

        switch (r.distribution) {
        case distribution_type::uniform_int:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("a"sv == name)
                      return read_temp_i64(value) && copy_i64_to(r.a32);
                  if ("b"sv == name)
                      return read_temp_i64(value) && copy_i64_to(r.b32);
                  return true;
              });

        case distribution_type::uniform_real:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("a"sv == name)
                      return read_temp_real(value) && copy_real_to(r.a);
                  if ("b"sv == name)
                      return read_temp_real(value) && copy_real_to(r.b);
                  return true;
              });

        case distribution_type::bernouilli:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("p"sv == name)
                      return read_temp_real(value) && copy_real_to(r.p);
                  return true;
              });

        case distribution_type::binomial:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("t"sv == name)
                      return read_temp_i64(value) && copy_i64_to(r.t32);
                  if ("p"sv == name)
                      return read_temp_real(value) && copy_real_to(r.p);
                  return true;
              });

        case distribution_type::negative_binomial:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("t"sv == name)
                      return read_temp_i64(value) && copy_i64_to(r.t32);
                  if ("p"sv == name)
                      return read_temp_real(value) && copy_real_to(r.p);
                  return true;
              });

        case distribution_type::geometric:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("p"sv == name)
                      return read_temp_real(value) && copy_real_to(r.p);
                  return true;
              });

        case distribution_type::poisson:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("mean"sv == name)
                      return read_temp_real(value) && copy_real_to(r.mean);
                  return true;
              });

        case distribution_type::exponential:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("lambda"sv == name)
                      return read_temp_real(value) && copy_real_to(r.lambda);
                  return true;
              });

        case distribution_type::gamma:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("alpha"sv == name)
                      return read_temp_real(value) && copy_real_to(r.alpha);
                  if ("beta"sv == name)
                      return read_temp_real(value) && copy_real_to(r.beta);
                  return true;
              });

        case distribution_type::weibull:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("a"sv == name)
                      return read_temp_real(value) && copy_real_to(r.a);
                  if ("b"sv == name)
                      return read_temp_real(value) && copy_real_to(r.b);
                  return true;
              });

        case distribution_type::exterme_value:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("a"sv == name)
                      return read_temp_real(value) && copy_real_to(r.a);
                  if ("b"sv == name)
                      return read_temp_real(value) && copy_real_to(r.b);
                  return true;
              });

        case distribution_type::normal:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("mean"sv == name)
                      return read_temp_real(value) && copy_real_to(r.mean);
                  if ("stddev"sv == name)
                      return read_temp_real(value) && copy_real_to(r.stddev);
                  return true;
              });

        case distribution_type::lognormal:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("m"sv == name)
                      return read_temp_real(value) && copy_real_to(r.m);
                  if ("s"sv == name)
                      return read_temp_real(value) && copy_real_to(r.s);
                  return true;
              });

        case distribution_type::chi_squared:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("n"sv == name)
                      return read_temp_real(value) && copy_real_to(r.n);
                  return true;
              });

        case distribution_type::cauchy:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("a"sv == name)
                      return read_temp_real(value) && copy_real_to(r.a);
                  if ("b"sv == name)
                      return read_temp_real(value) && copy_real_to(r.b);
                  return true;
              });

        case distribution_type::fisher_f:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("m"sv == name)
                      return read_temp_real(value) && copy_real_to(r.m);
                  if ("n"sv == name)
                      return read_temp_real(value) && copy_real_to(r.n);
                  return true;
              });

        case distribution_type::student_t:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("n"sv == name)
                      return read_temp_real(value) && copy_real_to(r.n);
                  return true;
              });
        }

        unreachable();
    }

    bool read_random_sources(const rapidjson::Value& val,
                             external_source&        srcs) noexcept
    {
        auto_stack s(this, "srcs random sources");

        i64 len = 0;

        return is_value_array(val) && copy_array_size(val, len) &&
               random_sources_can_alloc(srcs, len) &&
               for_each_array(
                 val, [&](const auto /*i*/, const auto& elem) noexcept -> bool {
                     auto& r  = srcs.random_sources.alloc();
                     auto  id = srcs.random_sources.get_id(r);

                     std::optional<u64> id_in_file;

                     auto_stack s(this, "srcs random source");

                     return for_each_member(
                              elem,
                              [&](const auto  name,
                                  const auto& value) noexcept -> bool {
                                  if ("id"sv == name)
                                      return read_temp_u64(value) &&
                                             copy_u64_to(id_in_file);

                                  if ("name"sv == name)
                                      return read_temp_string(value) &&
                                             copy_string_to(r.name);

                                  if ("type"sv == name) {
                                      return read_temp_string(value) &&
                                             copy_string_to(r.distribution) &&
                                             read_distribution_type(elem, r);
                                  }

                                  return true;
                              }) &&
                            optional_has_value(id_in_file) &&
                            cache_random_mapping_add(*id_in_file, id);
                 });
    }

    bool modeling_connect(generic_component&     compo,
                          const child_id         src,
                          const connection::port p_src,
                          const child_id         dst,
                          const connection::port p_dst) noexcept
    {
        auto_stack a(this, "component generic connect");

        if (auto* c_src = compo.children.try_to_get(src); c_src)
            if (auto* c_dst = compo.children.try_to_get(dst); c_dst)
                if (compo.connect(mod(), *c_src, p_src, *c_dst, p_dst))
                    return true;

        return error("fail to connect generic component children");
    }

    bool modeling_connect_input(generic_component& compo,
                                port_id            src_port,
                                child_id           dst,
                                connection::port   p_dst) noexcept
    {
        auto_stack a(this, "component generic connect input");

        if (auto* c_dst = compo.children.try_to_get(dst); c_dst)
            if (compo.connect_input(src_port, *c_dst, p_dst))
                return true;

        return error("fail to connect generic component children");
    }

    bool modeling_connect_output(generic_component& compo,
                                 child_id           src,
                                 connection::port   p_src,
                                 port_id            dst_port) noexcept
    {
        auto_stack a(this, "component generic connect output");

        if (auto* c_src = compo.children.try_to_get(src); c_src)
            if (compo.connect_output(dst_port, *c_src, p_src))
                return true;

        return error("fail to connect generic component children");
    }

    bool cache_model_mapping_to(child_id& dst) noexcept
    {
        if (auto* elem = self.model_mapping.get(temp_u64); elem) {
            dst = enum_cast<child_id>(*elem);
            return true;
        }

        return error("unknown generic component child");
    }

    bool cache_model_mapping_to(std::optional<child_id>& dst) noexcept
    {
        if (auto* elem = self.model_mapping.get(temp_u64); elem) {
            dst = enum_cast<child_id>(*elem);
            return true;
        }

        return error("unknown generic component child");
    }

    bool get_x_port(generic_component&                generic,
                    const child_id                    dst_id,
                    const std::optional<std::string>& dst_str_port,
                    const std::optional<int>&         dst_int_port,
                    std::optional<connection::port>&  out) noexcept
    {
        auto_stack a(this, "component generic x port");

        if (auto* child = generic.children.try_to_get(dst_id); child) {
            if (dst_int_port.has_value()) {
                if (child->type != child_type::model)
                    return error("unknwon generic component port");

                out = std::make_optional(
                  connection::port{ .model = *dst_int_port });
                return true;
            } else if (dst_str_port.has_value()) {
                if (child->type != child_type::component)
                    return error("unknwon generic component port");

                if (auto* compo = mod().components.try_to_get<component>(
                      child->id.compo_id)) {
                    auto p_id = compo->get_x(*dst_str_port);
                    if (is_undefined(p_id))
                        return error("unknown input component");
                    out = std::make_optional(connection::port{ .compo = p_id });
                    return true;
                } else {
                    return error("unknown component");
                }
            } else {
                unreachable();
            }
        }

        return error("unknown element");
    }

    bool get_y_port(generic_component&                generic,
                    const child_id                    src_id,
                    const std::optional<std::string>& src_str_port,
                    const std::optional<int>&         src_int_port,
                    std::optional<connection::port>&  out) noexcept
    {
        auto_stack a(this, "component generic y port");

        if (auto* child = generic.children.try_to_get(src_id); child) {
            if (src_int_port.has_value()) {
                if (child->type != child_type::model)
                    return error("unknwon generic component port");

                out = std::make_optional(
                  connection::port{ .model = *src_int_port });
                return true;
            } else if (src_str_port.has_value()) {
                if (child->type != child_type::component)
                    return error("unknwon generic component port");

                if (auto* compo = mod().components.try_to_get<component>(
                      child->id.compo_id);
                    compo) {
                    auto p_id = compo->get_y(*src_str_port);
                    if (is_undefined(p_id))
                        return error("unknown output component");

                    out = std::make_optional(connection::port{ .compo = p_id });
                    return true;
                } else {
                    return error("unknown component");
                }
            } else {
                unreachable();
            }
        }

        return error("unknown element");
    }

    bool get_x_port(component&                        compo,
                    const std::optional<std::string>& str_port,
                    std::optional<port_id>&           out) noexcept
    {
        if (!str_port.has_value())
            return error("unknown input port");

        auto port_id = compo.get_x(*str_port);
        if (is_undefined(port_id))
            return error("missing input port in component");

        out = port_id;
        return true;
    }

    bool get_y_port(component&                        compo,
                    const std::optional<std::string>& str_port,
                    std::optional<port_id>&           out) noexcept
    {
        if (!str_port.has_value())
            return error("unknown output port");

        auto port_id = compo.get_y(*str_port);
        if (is_undefined(port_id))
            return error("missing output port in component");

        out = port_id;
        return true;
    }

    bool read_internal_connection(const rapidjson::Value& val,
                                  generic_component&      gen) noexcept
    {
        auto_stack s(this, "component generic internal connection");

        std::optional<child_id>         src_id;
        std::optional<child_id>         dst_id;
        std::optional<std::string>      src_str_port;
        std::optional<std::string>      dst_str_port;
        std::optional<int>              src_int_port;
        std::optional<int>              dst_int_port;
        std::optional<connection::port> src_port;
        std::optional<connection::port> dst_port;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("source"sv == name)
                         return read_temp_u64(value) &&
                                cache_model_mapping_to(src_id);

                     if ("destination"sv == name)
                         return read_temp_u64(value) &&
                                cache_model_mapping_to(dst_id);

                     if ("port-source"sv == name)
                         return value.IsString()
                                  ? read_temp_string(value) &&
                                      copy_string_to(src_str_port)
                                  : read_temp_i64(value) &&
                                      copy_i64_to(src_int_port);

                     if ("port-destination"sv == name)
                         return value.IsString()
                                  ? read_temp_string(value) &&
                                      copy_string_to(dst_str_port)
                                  : read_temp_i64(value) &&
                                      copy_i64_to(dst_int_port);

                     return true;
                 }) &&
               optional_has_value(src_id) &&
               get_y_port(gen, *src_id, src_str_port, src_int_port, src_port) &&
               optional_has_value(dst_id) &&
               get_x_port(gen, *dst_id, dst_str_port, dst_int_port, dst_port) &&
               optional_has_value(src_port) && optional_has_value(dst_port) &&
               gen.connections.can_alloc() &&
               modeling_connect(gen, *src_id, *src_port, *dst_id, *dst_port);
    }

    bool read_output_connection(const rapidjson::Value& val,
                                component&              compo,
                                generic_component&      gen) noexcept
    {
        auto_stack s(this, "component generic output connection");

        child_id src_id = undefined<child_id>();

        std::optional<connection::port> src_port;
        std::optional<std::string>      src_str_port;
        std::optional<int>              src_int_port;

        std::optional<port_id>     port;
        std::optional<std::string> str_port;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("source"sv == name)
                         return read_temp_u64(value) &&
                                cache_model_mapping_to(src_id);
                     if ("port-source"sv == name)
                         return value.IsString()
                                  ? read_temp_string(value) &&
                                      copy_string_to(src_str_port)
                                  : read_temp_i64(value) &&
                                      copy_i64_to(src_int_port);
                     if ("port"sv == name)
                         return read_temp_string(value) &&
                                copy_string_to(str_port);

                     return true;
                 }) &&
               get_y_port(compo, str_port, port) &&
               get_y_port(gen, src_id, src_str_port, src_int_port, src_port) &&
               gen.connections.can_alloc() && optional_has_value(src_port) &&
               optional_has_value(port) &&
               modeling_connect_output(gen, src_id, *src_port, *port);
    }

    bool read_input_connection(const rapidjson::Value& val,
                               component&              compo,
                               generic_component&      gen) noexcept
    {
        auto_stack s(this, "component generic input connection");

        child_id dst_id = undefined<child_id>();

        std::optional<connection::port> dst_port;
        std::optional<std::string>      dst_str_port;
        std::optional<int>              dst_int_port;

        std::optional<port_id>     port;
        std::optional<std::string> str_port;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("destination"sv == name)
                         return read_temp_u64(value) &&
                                cache_model_mapping_to(dst_id);
                     if ("port-destination"sv == name)
                         return value.IsString()
                                  ? read_temp_string(value) &&
                                      copy_string_to(dst_str_port)
                                  : read_temp_i64(value) &&
                                      copy_i64_to(dst_int_port);

                     if ("port"sv == name)
                         return read_temp_string(value) &&
                                copy_string_to(str_port);

                     return true;
                 }) &&
               get_x_port(compo, str_port, port) &&
               get_x_port(gen, dst_id, dst_str_port, dst_int_port, dst_port) &&
               gen.connections.can_alloc() && optional_has_value(dst_port) &&
               optional_has_value(port) &&
               modeling_connect_input(gen, *port, dst_id, *dst_port);
    }

    bool read_input_pack_connection(const rapidjson::Value& val,
                                    component&              compo) noexcept
    {
        auto_stack s(this, "component input pack connection");

        auto child      = undefined<component_id>();
        auto port       = undefined<port_id>();
        auto child_port = undefined<port_id>();

        if (not read_child_simple_or_grid_component(val, child))
            return false;

        if (auto it = val.FindMember("port"); it != val.MemberEnd()) {
            if (read_temp_string(it->value))
                port = compo.get_x(temp_string);
            else
                return error("bad input pack connection port");
        }

        if (auto it = val.FindMember("child-port"); it != val.MemberEnd()) {
            if (read_temp_string(it->value))
                child_port =
                  mod().components.get<component>(child).get_x(temp_string);
            else
                return error("bad input pack connection child-port");
        }

        compo.input_connection_pack.push_back(
          connection_pack{ .parent_port     = port,
                           .child_port      = child_port,
                           .child_component = child });

        return true;
    }

    bool read_output_pack_connection(const rapidjson::Value& val,
                                     component&              compo) noexcept
    {
        auto_stack s(this, "component output pack connection");

        auto child      = undefined<component_id>();
        auto port       = undefined<port_id>();
        auto child_port = undefined<port_id>();

        if (not read_child_simple_or_grid_component(val, child))
            return false;

        if (auto it = val.FindMember("port"); it != val.MemberEnd()) {
            if (read_temp_string(it->value))
                port = compo.get_y(temp_string);
            else
                return error("bad output pack connection port");
        }

        if (auto it = val.FindMember("child-port"); it != val.MemberEnd()) {
            if (read_temp_string(it->value))
                child_port =
                  mod().components.get<component>(child).get_y(temp_string);
            else
                return error("bad output pack connection child-port");
        }

        compo.output_connection_pack.push_back(
          connection_pack{ .parent_port     = port,
                           .child_port      = child_port,
                           .child_component = child });

        return true;
    }

    bool dispatch_connection_type(const rapidjson::Value& val,
                                  connection_type         type,
                                  component&              compo,
                                  generic_component&      gen) noexcept
    {
        auto_stack s(this, "component generic dispatch connection");

        switch (type) {
        case connection_type::internal:
            return read_internal_connection(val, gen);
        case connection_type::output:
            return read_output_connection(val, compo, gen);
        case connection_type::input:
            return read_input_connection(val, compo, gen);
        case connection_type::input_pack:
            return read_input_pack_connection(val, compo);
        case connection_type::output_pack:
            return read_output_pack_connection(val, compo);
        }

        unreachable();
    }

    bool read_connections(const rapidjson::Value& val,
                          component&              compo,
                          generic_component&      gen) noexcept
    {
        auto_stack s(this, "component generic connections");

        return is_value_array(val) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& val_con) noexcept -> bool {
                     return for_each_member(
                       val_con,
                       [&](const auto  name,
                           const auto& value) noexcept -> bool {
                           if ("type"sv == name) {
                               connection_type type = connection_type::internal;
                               return read_temp_string(value) &&
                                      copy_string_to(type) &&
                                      dispatch_connection_type(
                                        val_con, type, compo, gen);
                           }

                           return true;
                       });
                 });
    }

    bool read_generic_component(const rapidjson::Value& val,
                                component&              compo) noexcept
    {
        auto_stack s(this, "component generic");

        auto& generic       = mod().generic_components.alloc();
        compo.type          = component_type::generic;
        compo.id.generic_id = mod().generic_components.get_id(generic);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("children"sv == name)
                  return read_children(value, compo, generic);

              if ("connections"sv == name)
                  return read_connections(value, compo, generic);

              return true;
          });
    }

    bool grid_children_set(std::span<component_id> out,
                           std::integral auto      idx,
                           component_id            c_id) noexcept
    {
        debug::ensure(std::cmp_less_equal(idx, out.size()));
        out[idx] = c_id;
        return true;
    }

    bool graph_children_add(graph_component& graph, component_id c_id) noexcept
    {
        const auto id  = graph.g.nodes.alloc();
        const auto idx = get_index(id);

        graph.g.node_components[idx] = c_id;

        return true;
    }

    bool read_grid_children(const rapidjson::Value& val,
                            grid_component&         compo) noexcept
    {
        auto_stack s(this, "component grid children");

        return is_value_array(val) &&
               is_value_array_size_equal(val, compo.cells_number()) &&
               for_each_array(
                 val, [&](const auto i, const auto& value) noexcept -> bool {
                     component_id c_id = undefined<component_id>();

                     return read_child_component(value, c_id) &&
                            grid_children_set(compo.children(), i, c_id);
                 });
    }

    bool dispatch_graph_type(const rapidjson::Value& val,
                             const rapidjson::Value& name,
                             graph_component&        graph) noexcept
    {
        auto_stack s(this, "component graph type");

        debug::ensure(name.IsString());

        if ("dot-file"sv == name.GetString()) {
            graph.g_type    = graph_component::graph_type::dot_file;
            graph.param.dot = graph_component::dot_file_param{};
            return read_dot_graph_param(val, graph);
        }

        if ("scale-free"sv == name.GetString()) {
            graph.g_type      = graph_component::graph_type::scale_free;
            graph.param.scale = graph_component::scale_free_param{};
            return read_scale_free_graph_param(val, graph);
        }

        if ("small-world"sv == name.GetString()) {
            graph.g_type      = graph_component::graph_type::small_world;
            graph.param.small = graph_component::small_world_param{};
            return read_small_world_graph_param(val, graph);
        }

        return error("bad graph component type");
    }

    bool read_dot_graph_param(const rapidjson::Value& val,
                              graph_component&        graph) noexcept
    {
        auto_stack s(this, "read dot graph paramters");

        name_str           reg_path;
        directory_path_str dir_path;
        file_path_str      file_path;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("path"sv == name)
                         return read_temp_string(value) &&
                                copy_string_to(reg_path);

                     if ("dir"sv == name)
                         return read_temp_string(value) &&
                                copy_string_to(dir_path);

                     if ("file"sv == name)
                         return read_temp_string(value) &&
                                copy_string_to(file_path);

                     return true;
                 }) &&
               search_dir(reg_path.sv(), dir_path.sv(), graph.param.dot.dir) &&
               search_file(
                 graph.param.dot.dir, file_path.sv(), graph.param.dot.file);
    }

    bool read_scale_free_graph_param(const rapidjson::Value& val,
                                     graph_component&        graph) noexcept
    {
        auto_stack s(this, "read scale free parameters");

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("alpha"sv == name)
                  return read_temp_real(value) && is_double_greater_than(0) &&
                         copy_real_to(graph.param.scale.alpha);

              if ("beta"sv == name)
                  return read_temp_real(value) && is_double_greater_than(0) &&
                         copy_real_to(graph.param.scale.beta);

              if ("nodes"sv == name)
                  return read_temp_i64(value) && is_i64_greater_equal_than(0) &&
                         copy_i64_to(graph.param.scale.nodes);

              return true;
          });
    }

    bool read_small_world_graph_param(const rapidjson::Value& val,
                                      graph_component&        graph) noexcept
    {
        auto_stack s(this, "read small world parameters");

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("probability"sv == name)
                  return read_temp_real(value) && is_double_greater_than(0) &&
                         copy_real_to(graph.param.small.probability);

              if ("k"sv == name)
                  return read_temp_i64(value) && is_i64_greater_equal_than(1) &&
                         copy_i64_to(graph.param.small.k);

              if ("nodes"sv == name)
                  return read_temp_i64(value) && is_i64_greater_equal_than(0) &&
                         copy_i64_to(graph.param.small.nodes);

              return true;
          });
    }

    bool reserve_graph_node(graph_component& compo, i64 len)
    {
        compo.g.nodes.reserve(len);
        compo.g.node_areas.resize(len);
        compo.g.node_components.resize(len);
        compo.g.node_ids.resize(len);
        compo.g.node_names.resize(len);
        compo.g.node_positions.resize(len);

        return true;
    }

    bool read_graph_children(const rapidjson::Value& val,
                             graph_component&        compo) noexcept
    {
        auto_stack s(this, "component graph children");

        auto it = val.FindMember("children");
        auto et = val.MemberEnd();

        if (it != et) {
            if (!it->value.IsArray())
                return error("json value is not an array");

            rapidjson::SizeType i = 0, e = it->value.GetArray().Size();
            if (e == 0)
                return true;

            for (const auto id : compo.g.nodes) {
                if (not(read_child_component(it->value.GetArray()[i],
                                             compo.g.node_components[id])))
                    return false;

                i++;

                if (i >= e)
                    break;
            }
        }

        return true;
    }

    bool is_grid_valid(const grid_component& grid) noexcept
    {
        return std::cmp_equal(grid.cells_number(), grid.children().size());
    }

    bool read_grid_input_connection(const rapidjson::Value& val,
                                    const component&        compo,
                                    grid_component&         grid) noexcept
    {
        auto_stack s(this, "component grid");

        std::optional<int>         row, col;
        std::optional<std::string> id;
        std::optional<std::string> x;

        if (not for_each_member(
              val,
              [&](const auto name, const auto& value) noexcept -> bool {
                  if ("row"sv == name)
                      return read_temp_i64(value) and copy_i64_to(row);

                  if ("col"sv == name)
                      return read_temp_i64(value) and copy_i64_to(col);

                  if ("id"sv == name)
                      return read_temp_string(value) and copy_string_to(id);

                  if ("x"sv == name)
                      return read_temp_string(value) and copy_string_to(x);

                  return true;
              }) and
            optional_has_value(row) and optional_has_value(col) and
            optional_has_value(id) and optional_has_value(x))
            return error("bad grid component size");

        if (not grid.is_coord_valid(*row, *col)) {
            return error("bad child coordinates {}x{}", *row, *col);
        } else {
            const auto pos        = grid.pos(*row, *col);
            const auto c_compo_id = grid.children()[pos];

            if (const auto* c =
                  mod().components.try_to_get<component>(c_compo_id);
                c) {
                const auto con_id = c->get_x(*id);
                const auto con_x  = compo.get_x(*x);

                if (is_defined(con_id) and is_defined(con_x)) {
                    if (auto ret =
                          grid.connect_input(con_x, *row, *col, con_id);
                        !!ret)
                        return true;
                }
            }
        }

        return false;
    }

    bool read_grid_output_connection(const rapidjson::Value& val,
                                     const component&        compo,
                                     grid_component&         grid) noexcept
    {
        auto_stack s(this, "component grid");

        std::optional<int>         row, col;
        std::optional<std::string> id;
        std::optional<std::string> y;

        if (not for_each_member(
              val,
              [&](const auto name, const auto& value) noexcept -> bool {
                  if ("row"sv == name)
                      return read_temp_i64(value) and copy_i64_to(row);

                  if ("col"sv == name)
                      return read_temp_i64(value) and copy_i64_to(col);

                  if ("id"sv == name)
                      return read_temp_string(value) and copy_string_to(id);

                  if ("y"sv == name)
                      return read_temp_string(value) and copy_string_to(y);

                  return true;
              }) and
            optional_has_value(row) and optional_has_value(col) and
            optional_has_value(id) and optional_has_value(y))
            return error("bad grid component size");

        if (not grid.is_coord_valid(*row, *col)) {
            return error("bad child coordinates: {}x{}", *row, *col);
        } else {
            const auto pos        = grid.pos(*row, *col);
            const auto c_compo_id = grid.children()[pos];

            if (const auto* c =
                  mod().components.try_to_get<component>(c_compo_id);
                c) {
                const auto con_id = c->get_x(*id);
                const auto con_y  = compo.get_y(*y);

                if (is_defined(con_id) and is_defined(con_y)) {
                    if (auto ret =
                          grid.connect_output(con_y, *row, *col, con_id);
                        !!ret)
                        return true;
                }
            }
        }

        return false;
    }

    bool read_grid_connection(const rapidjson::Value& val,
                              component&              compo,
                              grid_component&         grid) noexcept
    {
        auto_stack s(this, "component grid");

        return for_first_member(
          val, "type", [&](const auto& value) noexcept -> bool {
              return read_temp_string(value) and
                     ((temp_string == "input" and
                       read_grid_input_connection(val, compo, grid)) or
                      (temp_string == "output" and
                       read_grid_output_connection(val, compo, grid)) or
                      (temp_string == "input-pack" and
                       read_input_pack_connection(val, compo)) or
                      (temp_string == "output-pack" and
                       read_output_pack_connection(val, compo)));
          });
    }

    bool read_grid_connections(const rapidjson::Value& val,
                               component&              compo,
                               grid_component&         grid) noexcept
    {
        auto_stack s(this, "component grid");

        return is_value_array(val) and
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     return is_value_object(value) and
                            read_grid_connection(value, compo, grid);
                 });
    }

    static bool try_resize_grid_component(grid_component&    grid,
                                          std::optional<i32> rows,
                                          std::optional<i32> columns) noexcept
    {
        if (rows.has_value() and columns.has_value())
            grid.resize(*rows, *columns, undefined<component_id>());

        return true;
    }

    bool read_grid_component(const rapidjson::Value& val,
                             component&              compo) noexcept
    {
        auto_stack s(this, "component grid");

        auto& grid       = mod().grid_components.alloc();
        compo.type       = component_type::grid;
        compo.id.grid_id = mod().grid_components.get_id(grid);

        std::optional<i32> rows, columns;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("rows"sv == name)
                         return read_temp_i64(value) &&
                                is_i64_greater_equal_than(
                                  grid_component::slimit::lower_bound()) &&
                                is_i64_less_than(
                                  grid_component::slimit::upper_bound()) &&
                                copy_i64_to(rows) &&
                                try_resize_grid_component(grid, rows, columns);

                     if ("columns"sv == name)
                         return read_temp_i64(value) &&
                                is_i64_greater_equal_than(
                                  grid_component::slimit::lower_bound()) &&
                                is_i64_less_than(
                                  grid_component::slimit::upper_bound()) &&
                                copy_i64_to(columns) &&
                                try_resize_grid_component(grid, rows, columns);

                     if ("in-connection-type"sv == name)
                         return read_temp_i64(value) &&
                                copy_i64_to(grid.in_connection_type);

                     if ("out-connection-type"sv == name)
                         return read_temp_i64(value) &&
                                copy_i64_to(grid.out_connection_type);

                     if ("children"sv == name)
                         return read_grid_children(value, grid);

                     if ("connections"sv == name)
                         return read_grid_connections(value, compo, grid);

                     return true;
                 }) &&
               is_grid_valid(grid);
    }

    bool graph_component_build_graph(graph_component& graph) noexcept
    {
        auto_stack s(this, "component graph build random graph");

        switch (graph.g_type) {
        case graph_component::graph_type::dot_file:
            if (auto* g = mod().graphs.try_to_get(mod().search_graph_id(
                  graph.param.dot.dir, graph.param.dot.file))) {
                graph.g = *g;
                graph.update_position();
                return true;
            }
            return false;

        case graph_component::graph_type::scale_free: {
            auto ret = graph.g.init_scale_free_graph(graph.param.scale.alpha,
                                                     graph.param.scale.beta,
                                                     graph.param.scale.id,
                                                     graph.param.scale.nodes,
                                                     graph.seed,
                                                     graph.key);
            if (ret.has_value())
                graph.assign_grid_position();

            return ret.has_value();
        }

        case graph_component::graph_type::small_world:
            auto ret =
              graph.g.init_small_world_graph(graph.param.small.probability,
                                             graph.param.small.k,
                                             graph.param.small.id,
                                             graph.param.small.nodes,
                                             graph.seed,
                                             graph.key);
            if (ret.has_value())
                graph.assign_grid_position();

            return ret.has_value();
        }

        unreachable();
    }

    bool read_graph_input_connection(const rapidjson::Value& val,
                                     const component&        compo,
                                     graph_component&        graph) noexcept
    {
        auto_stack s(this, "component input graph connection");

        std::optional<i32>         v_opt;
        std::optional<std::string> id_opt;
        std::optional<std::string> x_opt;

        if (not for_each_member(
              val,
              [&](const auto name, const auto& value) noexcept -> bool {
                  if ("v"sv == name)
                      return read_temp_i64(value) and copy_i64_to(v_opt);

                  if ("id"sv == name)
                      return read_temp_string(value) and copy_string_to(id_opt);

                  if ("x"sv == name)
                      return read_temp_string(value) and copy_string_to(x_opt);

                  return true;
              }) and
            optional_has_value(v_opt) and optional_has_value(id_opt) and
            optional_has_value(x_opt))
            return error("bad grid component size");

        const auto id = graph.g.nodes.get_from_index(*v_opt);
        if (is_undefined(id)) {
            return error("bad child vertex {}", *v_opt);
        } else {
            const auto c_compo_id = graph.g.node_components[id];
            if (mod().components.exists(c_compo_id)) {
                const auto& c = mod().components.get<component>(c_compo_id);
                const auto  con_id = c.get_x(*id_opt);
                const auto  con_x  = compo.get_x(*x_opt);

                if (is_defined(con_id) and is_defined(con_x)) {
                    if (auto ret = graph.connect_input(con_x, id, con_id);
                        !!ret)
                        return true;
                }
            }
        }

        return true;
    }

    bool read_graph_output_connection(const rapidjson::Value& val,
                                      const component&        compo,
                                      graph_component&        graph) noexcept
    {
        auto_stack s(this, "component output graph connection");

        std::optional<i32>         v_opt;
        std::optional<std::string> id_opt;
        std::optional<std::string> y_opt;

        if (not for_each_member(
              val,
              [&](const auto name, const auto& value) noexcept -> bool {
                  if ("v"sv == name)
                      return read_temp_i64(value) and copy_i64_to(v_opt);

                  if ("id"sv == name)
                      return read_temp_string(value) and copy_string_to(id_opt);

                  if ("y"sv == name)
                      return read_temp_string(value) and copy_string_to(y_opt);

                  return true;
              }) and
            optional_has_value(v_opt) and optional_has_value(id_opt) and
            optional_has_value(y_opt))
            return error("bad grid component size");

        const auto id = graph.g.nodes.get_from_index(*v_opt);
        if (is_undefined(id)) {
            return error("bad child vertex {}", *v_opt);
        } else {
            const auto c_compo_id = graph.g.node_components[id];
            if (mod().components.exists(c_compo_id)) {
                const auto& c = mod().components.get<component>(c_compo_id);
                const auto  con_id = c.get_x(*id_opt);
                const auto  con_y  = compo.get_y(*y_opt);

                if (is_defined(con_id) and is_defined(con_y)) {
                    if (auto ret = graph.connect_output(con_y, id, con_id);
                        !!ret)
                        return true;
                }
            }
        }

        return false;
    }

    bool read_graph_connection(const rapidjson::Value& val,
                               component&              compo,
                               graph_component&        graph) noexcept
    {
        auto_stack s(this, "component graph connection");

        return for_first_member(
          val, "type", [&](const auto& value) noexcept -> bool {
              return read_temp_string(value) and
                     ((temp_string == "input" and
                       read_graph_input_connection(val, compo, graph)) or
                      (temp_string == "output" and
                       read_graph_output_connection(val, compo, graph)) or
                      (temp_string == "input-pack" and
                       read_input_pack_connection(val, compo)) or
                      (temp_string == "output-pack" and
                       read_output_pack_connection(val, compo)));
          });
    }

    bool read_graph_connections(const rapidjson::Value& val,
                                component&              compo,
                                graph_component&        graph) noexcept
    {
        auto_stack s(this, "component graph connections");

        return is_value_array(val) and
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     return is_value_object(value) and
                            read_graph_connection(value, compo, graph);
                 });
    }

    bool read_graph_component(const rapidjson::Value& val,
                              component&              compo) noexcept
    {
        auto_stack s(this, "component graph");

        auto& graph       = mod().graph_components.alloc();
        compo.type        = component_type::graph;
        compo.id.graph_id = mod().graph_components.get_id(graph);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("graph-type"sv == name)
                  return value.IsString() &&
                         dispatch_graph_type(val, value, graph) &&
                         graph_component_build_graph(graph) &&
                         read_graph_children(val, graph);

              if ("connections"sv == name)
                  return read_graph_connections(value, compo, graph);

              return true;
          });
    }

    bool read_hsm_component(const rapidjson::Value& val,
                            component&              compo) noexcept
    {
        auto_stack s(this, "component hsm");

        auto& hsm       = mod().hsm_components.alloc();
        compo.type      = component_type::hsm;
        compo.id.hsm_id = mod().hsm_components.get_id(hsm);

        std::optional<source::source_type> type;
        std::optional<u64>                 id;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("states"sv == name)
                         return read_hsm_states(
                           value, hsm.machine.states, hsm.names, hsm.positions);

                     if ("top"sv == name)
                         return read_temp_u64(value) &&
                                copy_u64_to(hsm.machine.top_state);

                     if ("i1"sv == name)
                         return read_temp_i64(value) && copy_i64_to(hsm.i1);

                     if ("i2"sv == name)
                         return read_temp_i64(value) && copy_i64_to(hsm.i2);

                     if ("r1"sv == name)
                         return read_temp_real(value) && copy_real_to(hsm.r1);

                     if ("r2"sv == name)
                         return read_temp_real(value) && copy_real_to(hsm.r2);

                     if ("timeout"sv == name)
                         return read_temp_real(value) &&
                                copy_real_to(hsm.timeout);

                     if ("source-type"sv == name)
                         return read_temp_i64(value) && copy_i64_to(type);

                     if ("source-id"sv == name)
                         return read_temp_u64(value) && copy_u64_to(id);

                     if ("constants"sv == name)
                         return value.IsArray() and
                                  std::cmp_less_equal(
                                    value.GetArray().Size(),
                                    hierarchical_state_machine::max_constants),
                                for_each_array(
                                  value,
                                  [&](const auto  i,
                                      const auto& val) noexcept -> bool {
                                      return read_temp_real(val) &&
                                             copy_real_to(
                                               hsm.machine.constants[i]);
                                  });

                     return true;
                 }) and
               copy_to_source(type, id, hsm.src);
    }

    bool dispatch_component_type(const rapidjson::Value& val,
                                 component&              compo) noexcept
    {
        switch (compo.type) {
        case component_type::none:
            return true;

        case component_type::generic:
            return read_generic_component(val, compo);

        case component_type::grid:
            return read_grid_component(val, compo);

        case component_type::graph:
            return read_graph_component(val, compo);

        case component_type::hsm:
            return read_hsm_component(val, compo);
        }

        return error("unknown component type: {} ({})",
                     ordinal(compo.type),
                     component_type_names[ordinal(compo.type)]);
        ;
    }

    bool convert_to_component(component& compo) noexcept
    {
        if (auto type = get_component_type(temp_string); type.has_value()) {
            compo.type = type.value();
            return true;
        }

        return error("bad component type {}", temp_string);
    }

    bool read_input_port(const rapidjson::Value& val,
                         component::port_type&   port,
                         port_id                 id) noexcept
    {
        auto_stack s(this, "component input port");

        static constexpr std::string_view n[] = { "name", "type", "x", "y" };

        return for_members(val, n, [&](auto idx, const auto& value) noexcept {
            switch (idx) {
            case 0:
                return read_temp_string(value) &&
                       copy_string_to(port.get<port_str>(id));

            case 1:
                return read_temp_string(value) &&
                       copy_string_to(port.get<port_option>(id));

            case 2:
                return read_temp_real(value) &&
                       copy_real_to(port.get<position>(id).x);

            case 3:
                return read_temp_real(value) &&
                       copy_real_to(port.get<position>(id).y);

            default:
                return error("unknown element");
            }
        });
    }

    bool read_input_ports(const rapidjson::Value& val,
                          component::port_type&   port) noexcept
    {
        auto_stack s(this, "component input ports");

        return is_value_array(val) && port.can_alloc(val.GetArray().Size()) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     return read_input_port(value, port, port.alloc());
                 });
    }

    bool read_output_port(const rapidjson::Value& val,
                          component::port_type&   port,
                          port_id                 id) noexcept
    {
        auto_stack s(this, "component output port");

        static constexpr std::string_view n[] = { "name", "type", "x", "y" };

        return for_members(val, n, [&](auto idx, const auto& value) noexcept {
            switch (idx) {
            case 0:
                return read_temp_string(value) &&
                       copy_string_to(port.get<port_str>(id));

            case 1:
                return read_temp_string(value) &&
                       copy_string_to(port.get<port_option>(id));

            case 2:
                return read_temp_real(value) &&
                       copy_real_to(port.get<position>(id).x);

            case 3:
                return read_temp_real(value) &&
                       copy_real_to(port.get<position>(id).y);

            default:
                return error("unknown element");
            }
        });
    }

    bool read_output_ports(const rapidjson::Value& val,
                           component::port_type&   port) noexcept
    {
        auto_stack s(this, "component output ports");

        return is_value_array(val) && port.can_alloc(val.GetArray().Size()) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     return read_output_port(value, port, port.alloc());
                 });
    }

    bool read_component_colors(const rapidjson::Value& val,
                               std::array<float, 4>&   color) noexcept
    {
        auto_stack s(this, "component color");

        return is_value_array(val) && is_value_array_size_equal(val, 4) &&
               for_each_array(
                 val, [&](const auto i, const auto& value) noexcept -> bool {
                     return read_temp_real(value) && copy_real_to(color[i]);
                 });
    }

    bool read_component(const rapidjson::Value& val, component& compo) noexcept
    {
        auto_stack s(this, "component");

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("name"sv == name)
                  return read_temp_string(value) && copy_string_to(compo.name);
              if ("constant-sources"sv == name)
                  return read_constant_sources(value, compo.srcs) and
                         cache_constant_mapping_sort();
              if ("binary-file-sources"sv == name)
                  return read_binary_file_sources(value, compo.srcs) and
                         cache_binary_file_mapping_sort();
              if ("text-file-sources"sv == name)
                  return read_text_file_sources(value, compo.srcs) and
                         cache_text_file_mapping_sort();
              if ("random-sources"sv == name)
                  return read_random_sources(value, compo.srcs) and
                         cache_random_mapping_sort();
              if ("x"sv == name)
                  return read_input_ports(value, compo.x);
              if ("y"sv == name)
                  return read_output_ports(value, compo.y);
              if ("type"sv == name)
                  return read_temp_string(value) &&
                         convert_to_component(compo) &&
                         dispatch_component_type(val, compo);
              if ("colors"sv == name)
                  return read_component_colors(
                    value,
                    mod().components.get<component_color>(
                      mod().components.get_id(compo)));

              return true;
          });
    }

    bool cache_model_mapping_add(u64 id_in_file, u64 id) const noexcept
    {
        self.model_mapping.data.emplace_back(id_in_file, id);
        return true;
    }

    bool project_set(component_id c_id) noexcept
    {
        auto_stack s(this, "project set components");

        if (auto* compo = mod().components.try_to_get<component>(c_id); compo) {
            if (auto ret = pj().set(mod(), *compo); ret)
                return true;
            else
                return error("fail to build project");
        } else
            return error("fail to read component");
    }

    bool read_project_top_component(const rapidjson::Value& val) noexcept
    {
        auto_stack s(this, "project top component");

        small_string<31>   reg_name;
        directory_path_str dir_path;
        file_path_str      file_path;
        component_id       c_id = undefined<component_id>();

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("component-path"sv == name)
                         return read_temp_string(value) &&
                                copy_string_to(reg_name);

                     if ("component-directory"sv == name)
                         return read_temp_string(value) &&
                                copy_string_to(dir_path);

                     if ("component-file"sv == name)
                         return read_temp_string(value) &&
                                copy_string_to(file_path);

                     return true;
                 }) &&
               modeling_copy_component_id(
                 reg_name, dir_path, file_path, c_id) &&
               project_set(c_id);
    }

    template<typename T>
    bool vector_add(vector<T>& vec, const T t) noexcept
    {
        vec.emplace_back(t);
        return true;
    }

    template<typename T>
    bool vector_not_empty(const vector<T>& vec) noexcept
    {
        return !vec.empty();
    }

    bool read_real_parameter(const rapidjson::Value& val,
                             std::array<real, 8>&    reals) noexcept
    {
        auto_stack s(this, "project real parameter");

        return is_value_array(val) && is_value_array_size_equal(val, 8) &&
               for_each_array(
                 val, [&](const auto i, const auto& value) noexcept -> bool {
                     return read_temp_real(value) && copy_real_to(reals[i]);
                 });
    }

    bool read_integer_parameter(const rapidjson::Value& val,
                                std::array<i64, 8>&     integers) noexcept
    {
        auto_stack s(this, "project integer parameter");

        return is_value_array(val) && is_value_array_size_equal(val, 8) &&
               for_each_array(
                 val, [&](const auto i, const auto& value) noexcept -> bool {
                     return read_temp_i64(value) && copy_i64_to(integers[i]);
                 });
    }

    bool read_parameter(const rapidjson::Value& val, parameter& param) noexcept
    {
        auto_stack s(this, "project parameter");

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("real"sv == name)
                  return read_real_parameter(value, param.reals);
              if ("integer"sv == name)
                  return read_integer_parameter(value, param.integers);

              return true;
          });
    }

    bool global_parameter_init(const global_parameter_id id,
                               const tree_node_id        tn_id,
                               const model_id            mdl_id,
                               const parameter&          p) const noexcept
    {
        pj().parameters.get<tree_node_id>(id) = tn_id;
        pj().parameters.get<model_id>(id)     = mdl_id;
        pj().parameters.get<parameter>(id)    = p;

        return true;
    }

    bool read_global_parameter(const rapidjson::Value&   val,
                               const global_parameter_id id) noexcept
    {
        auto_stack s(this, "project global parameter");

        std::optional<tree_node_id> tn_id_opt;
        std::optional<model_id>     mdl_id_opt;
        std::optional<parameter>    parameter_opt;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     unique_id_path path;

                     if ("name"sv == name)
                         return read_temp_string(value) &&
                                copy_string_to(
                                  pj().parameters.get<name_str>(id));

                     if ("access"sv == name)
                         return read_project_unique_id_path(value, path) &&
                                convert_to_tn_model_ids(
                                  path, tn_id_opt, mdl_id_opt);

                     if ("parameter"sv == name) {
                         parameter p;
                         if (not read_parameter(value, p))
                             return false;

                         parameter_opt = p;
                         return true;
                     }

                     return true;
                 }) and
               optional_has_value(tn_id_opt) and
               optional_has_value(mdl_id_opt) and
               optional_has_value(parameter_opt) and
               global_parameter_init(
                 id, *tn_id_opt, *mdl_id_opt, *parameter_opt);
    }

    bool read_global_parameters(const rapidjson::Value& val) noexcept
    {
        auto_stack s(this, "project global parameters");

        i64 size = 0;

        return is_value_array(val) && copy_array_size(val, size) &&
               project_global_parameters_can_alloc(size) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     const auto id = pj().parameters.alloc();
                     return read_global_parameter(value, id);
                 });
    }

    bool read_project_parameters(const rapidjson::Value& val) noexcept
    {
        auto_stack s(this, "project parameters");

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("global"sv == name)
                  return read_global_parameters(value);

              return error("unknown element: {}", name);
          });
    }

    bool read_project_observations(const rapidjson::Value& val) noexcept
    {
        auto_stack s(this, "project parameters");

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("global"sv == name)
                  return read_project_plot_observations(value);

              if ("grid"sv == name)
                  return read_project_grid_observations(value);

              return error("unknown element: {}", name);
          });
    }

    bool convert_to_tn_model_ids(const unique_id_path&        path,
                                 std::optional<tree_node_id>& parent_id,
                                 std::optional<model_id>&     mdl_id) noexcept
    {
        auto_stack s(this, "project convert to tn model ids");

        if (auto ret_opt = pj().get_model_path(path); ret_opt.has_value()) {
            parent_id = ret_opt->first;
            mdl_id    = ret_opt->second;
            return true;
        }

        return error("fail to convert access to tree node and model ids");
    }

    bool convert_to_tn_model_ids(const unique_id_path& path,
                                 vector<tree_node_id>& parent_id,
                                 vector<model_id>&     mdl_id) noexcept
    {
        auto_stack s(this, "project convert to tn model ids");

        if (auto ret_opt = pj().get_model_path(path); ret_opt.has_value()) {
            parent_id.emplace_back(ret_opt->first);
            mdl_id.emplace_back(ret_opt->second);
            return true;
        }

        return error("fail to convert access to tree node and model ids");
    }

    bool convert_to_tn_id(const unique_id_path&        path,
                          std::optional<tree_node_id>& tn_id) noexcept
    {
        auto_stack s(this, "project convert to tn id");

        if (const auto id = pj().get_tn_id(path); is_defined(id)) {
            tn_id = id;
            return true;
        }

        return error("fail to convert access to tree node and model ids");
    }

    template<typename T, int L>
    bool clear(small_vector<T, L>& out) noexcept
    {
        out.clear();

        return true;
    }

    template<typename T, int L>
    bool push_back_string(small_vector<T, L>& out) noexcept
    {
        if (out.ssize() + 1 < out.capacity()) {
            out.emplace_back(
              std::string_view{ temp_string.data(), temp_string.size() });
            return true;
        }
        return false;
    }

    bool read_project_unique_id_path(const rapidjson::Value& val,
                                     unique_id_path&         out) noexcept
    {
        auto_stack s(this, "project unique id path");

        return is_value_array(val) &&
               is_value_array_size_less(val, out.capacity()) && clear(out) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     return read_temp_string(value) && push_back_string(out);
                 });
    }

    bool copy(const component_color& cc, color& c) noexcept
    {
        c = 0;
        c = (u32)((int)(std::clamp(cc[0], 0.f, 1.f) * 255.f + 0.5f));
        c |= ((u32)((int)(std::clamp(cc[1], 0.f, 1.f) * 255.0f + 0.5f)) << 8);
        c |= ((u32)((int)(std::clamp(cc[2], 0.f, 1.f) * 255.0f + 0.5f)) << 16);
        c |= ((u32)((int)(std::clamp(cc[3], 0.f, 1.f) * 255.0f + 0.5f)) << 24);
        return true;
    }

    bool read_color(const rapidjson::Value& val, color& c) noexcept
    {
        auto_stack      s(this, "load color");
        component_color cc{};

        return is_value_array(val) && is_value_array_size_equal(val, 4) &&
               for_each_array(
                 val,
                 [&](const auto i, const auto& value) noexcept -> bool {
                     return read_temp_real(value) && copy_real_to(cc[i]);
                 }) &&
               copy(cc, c);
    }

    bool plot_observation_init(variable_observer&                    plot,
                               const tree_node_id                    parent_id,
                               const model_id                        mdl_id,
                               const color                           c,
                               const variable_observer::type_options t,
                               const std::string_view name) noexcept
    {
        if (auto* tn = pj().tree_nodes.try_to_get(parent_id); tn) {
            plot.push_back(parent_id, mdl_id, c, t, name);
            return true;
        }

        return false;
    }

    bool read_project_plot_observation_child(const rapidjson::Value& val,
                                             variable_observer& plot) noexcept
    {
        auto_stack s(this, "project plot observation child");

        name_str                    str;
        std::optional<tree_node_id> parent_id;
        std::optional<model_id>     mdl_id;

        auto c = color(0xff0ff);
        auto t = variable_observer::type_options::line;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     unique_id_path path;
                     if ("name"sv == name)
                         return read_temp_string(value) && copy_string_to(str);

                     if ("access"sv == name)
                         return read_project_unique_id_path(value, path) &&
                                convert_to_tn_model_ids(
                                  path, parent_id, mdl_id);

                     if ("color"sv == name)
                         return read_color(value, c);

                     if ("type"sv == name)
                         return read_temp_string(value);

                     return error("unknown element {}", name);
                 }) &&
               optional_has_value(parent_id) and optional_has_value(mdl_id) and
               plot_observation_init(plot, *parent_id, *mdl_id, c, t, str.sv());
        return false;
    }

    bool copy_string_to(variable_observer::type_options& type) noexcept
    {
        if (temp_string == "line")
            type = variable_observer::type_options::line;

        if (temp_string == "dash")
            type = variable_observer::type_options::dash;

        return error("unknown element {}", temp_string);
    }

    bool read_project_plot_observation_children(
      const rapidjson::Value& val,
      variable_observer&      plot) noexcept
    {
        auto_stack s(this, "project plot observation children");

        i64 size = 0;

        return is_value_array(val) and
               copy_array_size(val, size) and // @TODO check un can-alloc
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     return is_value_object(value) and
                            read_project_plot_observation_child(value, plot);
                 });
    }

    bool read_project_plot_observation(const rapidjson::Value& val,
                                       variable_observer&      plot) noexcept
    {
        auto_stack s(this, "project plot observation");

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("name"sv == name)
                  return read_temp_string(value) && copy_string_to(plot.name);

              if ("models"sv == name)
                  return read_project_plot_observation_children(value, plot);

              return true;
          });
    }

    bool read_project_plot_observations(const rapidjson::Value& val) noexcept
    {
        auto_stack s(this, "project plot observations");

        i64 size = 0;

        return is_value_array(val) && copy_array_size(val, size) &&
               project_variable_observers_can_alloc(size) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     auto& plot = pj().variable_observers.alloc();
                     return read_project_plot_observation(value, plot);
                 });
    }

    bool grid_observation_init(grid_observer& grid,
                               tree_node_id   parent_id,
                               tree_node_id   grid_tn_id,
                               model_id       mdl_id) const
    {
        auto* parent     = pj().tree_nodes.try_to_get(parent_id);
        auto* mdl_parent = pj().tree_nodes.try_to_get(grid_tn_id);

        debug::ensure(parent and mdl_parent);

        if (parent and mdl_parent) {
            grid.parent_id = parent_id;
            grid.tn_id     = grid_tn_id;
            grid.mdl_id    = mdl_id;
            grid.compo_id  = mdl_parent->id;
            parent->grid_observer_ids.emplace_back(
              pj().grid_observers.get_id(grid));
            return true;
        }

        return false;
    }

    bool read_project_grid_observation(const rapidjson::Value& val,
                                       grid_observer&          grid) noexcept
    {
        auto_stack s(this, "project grid observation");

        std::optional<tree_node_id> parent_id;
        std::optional<tree_node_id> grid_tn_id;
        std::optional<model_id>     mdl_id;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     unique_id_path path;

                     if ("name"sv == name)
                         return read_temp_string(value) &&
                                copy_string_to(grid.name);

                     if ("grid"sv == name)
                         return read_project_unique_id_path(value, path) &&
                                convert_to_tn_id(path, parent_id);

                     if ("access"sv == name)
                         return read_project_unique_id_path(value, path) &&
                                convert_to_tn_model_ids(
                                  path, grid_tn_id, mdl_id);

                     return true;
                 }) and
               optional_has_value(parent_id) and
               optional_has_value(grid_tn_id) and optional_has_value(mdl_id) and
               grid_observation_init(grid, *parent_id, *grid_tn_id, *mdl_id);
    }

    bool read_project_grid_observations(const rapidjson::Value& val) noexcept
    {
        auto_stack s(this, "project grid observations");

        i64 size = 0;

        return is_value_array(val) && copy_array_size(val, size) &&
               project_grid_observers_can_alloc(size) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     auto& grid = pj().grid_observers.alloc();
                     return read_project_grid_observation(value, grid);
                 });
    }

    bool project_time_limit_affect(const double b,
                                   const double e) const noexcept
    {
        pj().sim.limits.set_bound(b, e);

        return true;
    }

    bool read_project(const rapidjson::Value& val) noexcept
    {
        auto_stack s(this, "project");

        double begin = { 0 };
        double end   = { 100 };

        return read_project_top_component(val) &&
               for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("begin"sv == name)
                         return read_temp_real(value) and copy_real_to(begin);

                     if ("end"sv == name)
                         return read_temp_real(value) and copy_real_to(end);

                     if ("parameters"sv == name)
                         return read_project_parameters(value);

                     if ("observations"sv == name)
                         return read_project_observations(value);

                     return true;
                 }) and
               project_time_limit_affect(begin, end);
    }

    status parse_component(const rapidjson::Document& doc,
                           component&                 compo) noexcept
    {
        debug::ensure(compo.state != component_status::unmodified);

        if (read_component(doc.GetObject(), compo)) {
            compo.state = component_status::unmodified;
            return success();
        }

        compo.state = component_status::unreadable;
        mod().clear(compo);

        return error_code(json_errc::invalid_component_format);
    }

    status parse_project(const rapidjson::Document& doc) noexcept
    {
        pj().clear();
        sim().clear();

        if (read_project(doc.GetObject()))
            return success();

        return error_code(json_errc::invalid_project_format);
    }
};

static status read_file_to_buffer(vector<char>& buffer, file& f) noexcept
{
    debug::ensure(f.is_open());
    debug::ensure(f.get_mode() == open_mode::read);

    if (not f.is_open() or f.get_mode() != open_mode::read)
        return error_code(file_errc::open_error);

    const auto len = f.length();
    if (std::cmp_less(len, 2))
        return error_code(file_errc::empty);

    buffer.resize(len);
    if (std::cmp_less(buffer.size(), len))
        return error_code(file_errc::memory_error);

    if (not f.read(buffer.data(), len))
        return error_code(file_errc::memory_error);

    return success();
}

static status parse_json_data(std::span<char>      buffer,
                              rapidjson::Document& doc) noexcept
{
    doc.Parse<rapidjson::kParseNanAndInfFlag>(buffer.data(), buffer.size());

    if (doc.HasParseError())
        return error_code(json_errc::invalid_format);

    return success();
}

//
//

struct json_archiver::impl {
    json_archiver& self;

    impl(json_archiver& self_) noexcept
      : self{ self_ }
    {}

    template<typename Writer, std::size_t QssLevel>
    void write(Writer& writer,
               const abstract_integrator<QssLevel>& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();
        writer.Key("X");
        writer.Double(p.reals[0]);
        writer.Key("dQ");
        writer.Double(p.reals[1]);
        writer.EndObject();
    }

    template<typename Writer, std::size_t QssLevel>
    void write(Writer& writer,
               const abstract_inverse<QssLevel>& /*dyn*/,
               const parameter& /*p*/) noexcept
    {
        writer.StartObject();
        writer.EndObject();
    }

    template<typename Writer, std::size_t QssLevel>
    void write(Writer& writer,
               const abstract_multiplier<QssLevel>& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();
        writer.Key("value-0");
        writer.Double(p.reals[0]);
        writer.Key("value-1");
        writer.Double(p.reals[1]);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss1_sum_2& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();

        writer.Key("value-0");
        writer.Double(p.reals[0]);
        writer.Key("value-1");
        writer.Double(p.reals[1]);

        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss1_sum_3& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();

        writer.Key("value-0");
        writer.Double(p.reals[0]);
        writer.Key("value-1");
        writer.Double(p.reals[1]);
        writer.Key("value-2");
        writer.Double(p.reals[2]);

        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss1_sum_4& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();

        writer.Key("value-0");
        writer.Double(p.reals[0]);
        writer.Key("value-1");
        writer.Double(p.reals[1]);
        writer.Key("value-2");
        writer.Double(p.reals[2]);
        writer.Key("value-3");
        writer.Double(p.reals[3]);

        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss1_wsum_2& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();

        writer.Key("value-0");
        writer.Double(p.reals[0]);
        writer.Key("value-1");
        writer.Double(p.reals[1]);

        writer.Key("coeff-0");
        writer.Double(p.reals[2]);
        writer.Key("coeff-1");
        writer.Double(p.reals[3]);

        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss1_wsum_3& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();

        writer.Key("value-0");
        writer.Double(p.reals[0]);
        writer.Key("value-1");
        writer.Double(p.reals[1]);
        writer.Key("value-2");
        writer.Double(p.reals[2]);

        writer.Key("coeff-0");
        writer.Double(p.reals[3]);
        writer.Key("coeff-1");
        writer.Double(p.reals[4]);
        writer.Key("coeff-2");
        writer.Double(p.reals[5]);

        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss1_wsum_4& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();

        writer.Key("value-0");
        writer.Double(p.reals[0]);
        writer.Key("value-1");
        writer.Double(p.reals[1]);
        writer.Key("value-2");
        writer.Double(p.reals[2]);
        writer.Key("value-3");
        writer.Double(p.reals[3]);

        writer.Key("coeff-0");
        writer.Double(p.reals[4]);
        writer.Key("coeff-1");
        writer.Double(p.reals[5]);
        writer.Key("coeff-2");
        writer.Double(p.reals[6]);
        writer.Key("coeff-3");
        writer.Double(p.reals[7]);

        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss2_sum_2& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();

        writer.Key("value-0");
        writer.Double(p.reals[0]);
        writer.Key("value-1");
        writer.Double(p.reals[1]);

        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss2_sum_3& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();

        writer.Key("value-0");
        writer.Double(p.reals[0]);
        writer.Key("value-1");
        writer.Double(p.reals[1]);
        writer.Key("value-2");
        writer.Double(p.reals[2]);

        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss2_sum_4& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();

        writer.Key("value-0");
        writer.Double(p.reals[0]);
        writer.Key("value-1");
        writer.Double(p.reals[1]);
        writer.Key("value-2");
        writer.Double(p.reals[2]);
        writer.Key("value-3");
        writer.Double(p.reals[3]);

        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss2_wsum_2& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();

        writer.Key("value-0");
        writer.Double(p.reals[0]);
        writer.Key("value-1");
        writer.Double(p.reals[1]);

        writer.Key("coeff-0");
        writer.Double(p.reals[2]);
        writer.Key("coeff-1");
        writer.Double(p.reals[3]);

        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss2_wsum_3& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();

        writer.Key("value-0");
        writer.Double(p.reals[0]);
        writer.Key("value-1");
        writer.Double(p.reals[1]);
        writer.Key("value-2");
        writer.Double(p.reals[2]);

        writer.Key("coeff-0");
        writer.Double(p.reals[3]);
        writer.Key("coeff-1");
        writer.Double(p.reals[4]);
        writer.Key("coeff-2");
        writer.Double(p.reals[5]);

        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss2_wsum_4& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();

        writer.Key("value-0");
        writer.Double(p.reals[0]);
        writer.Key("value-1");
        writer.Double(p.reals[1]);
        writer.Key("value-2");
        writer.Double(p.reals[2]);
        writer.Key("value-3");
        writer.Double(p.reals[3]);

        writer.Key("coeff-0");
        writer.Double(p.reals[4]);
        writer.Key("coeff-1");
        writer.Double(p.reals[5]);
        writer.Key("coeff-2");
        writer.Double(p.reals[6]);
        writer.Key("coeff-3");
        writer.Double(p.reals[7]);

        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss3_sum_2& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();

        writer.Key("value-0");
        writer.Double(p.reals[0]);
        writer.Key("value-1");
        writer.Double(p.reals[1]);

        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss3_sum_3& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();

        writer.Key("value-0");
        writer.Double(p.reals[0]);
        writer.Key("value-1");
        writer.Double(p.reals[1]);
        writer.Key("value-2");
        writer.Double(p.reals[2]);

        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss3_sum_4& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();

        writer.Key("value-0");
        writer.Double(p.reals[0]);
        writer.Key("value-1");
        writer.Double(p.reals[1]);
        writer.Key("value-2");
        writer.Double(p.reals[2]);
        writer.Key("value-3");
        writer.Double(p.reals[3]);

        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss3_wsum_2& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();

        writer.Key("value-0");
        writer.Double(p.reals[0]);
        writer.Key("value-1");
        writer.Double(p.reals[1]);

        writer.Key("coeff-0");
        writer.Double(p.reals[2]);
        writer.Key("coeff-1");
        writer.Double(p.reals[3]);

        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss3_wsum_3& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();

        writer.Key("value-0");
        writer.Double(p.reals[0]);
        writer.Key("value-1");
        writer.Double(p.reals[1]);
        writer.Key("value-2");
        writer.Double(p.reals[2]);

        writer.Key("coeff-0");
        writer.Double(p.reals[3]);
        writer.Key("coeff-1");
        writer.Double(p.reals[4]);
        writer.Key("coeff-2");
        writer.Double(p.reals[5]);

        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss3_wsum_4& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();

        writer.Key("value-0");
        writer.Double(p.reals[0]);
        writer.Key("value-1");
        writer.Double(p.reals[1]);
        writer.Key("value-2");
        writer.Double(p.reals[2]);
        writer.Key("value-3");
        writer.Double(p.reals[3]);

        writer.Key("coeff-0");
        writer.Double(p.reals[4]);
        writer.Key("coeff-1");
        writer.Double(p.reals[5]);
        writer.Key("coeff-2");
        writer.Double(p.reals[6]);
        writer.Key("coeff-3");
        writer.Double(p.reals[7]);

        writer.EndObject();
    }

    template<typename Writer, sz QssLevel>
    void write(Writer& writer,
               const abstract_integer<QssLevel>& /*dyn*/,
               const parameter& /*p*/) noexcept
    {
        writer.StartObject();
        writer.EndObject();
    }

    template<typename Writer, sz QssLevel>
    void write(Writer& writer,
               const abstract_compare<QssLevel>& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();
        writer.Key("a");
        writer.Double(p.reals[0]);
        writer.Key("b");
        writer.Double(p.reals[1]);
        writer.Key("a-less-b");
        writer.Double(p.reals[2]);
        writer.Key("not-a-less-b");
        writer.Double(p.reals[3]);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const counter& /*dyn*/,
               const parameter& /*p*/) noexcept
    {
        writer.StartObject();
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const queue& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();
        writer.Key("ta");
        writer.Double(p.reals[0]);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const dynamic_queue& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();
        writer.Key("source-ta-type");
        writer.Int64(p.integers[0]);
        writer.Key("source-ta-id");
        writer.Uint64(p.integers[1]);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const priority_queue& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();
        writer.Key("source-ta-type");
        writer.Int64(p.integers[0]);
        writer.Key("source-ta-id");
        writer.Uint64(p.integers[1]);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const generator& /*dyn*/,
               const parameter& p) noexcept
    {
        const auto options = to_unsigned(p.integers[0]);
        const auto flags   = bitflags<generator::option>(options);

        writer.StartObject();

        writer.Key("ta-is-external");
        writer.Bool(flags[generator::option::ta_use_source]);
        writer.Key("value-is-external");
        writer.Bool(flags[generator::option::value_use_source]);

        writer.Key("offset");
        writer.Double(p.reals[0]);
        writer.Key("source-ta-type");
        writer.Int64(p.integers[1]);
        writer.Key("source-ta-id");
        writer.Uint64(p.integers[2]);

        writer.Key("source-value-type");
        writer.Int64(p.integers[3]);
        writer.Key("source-value-id");
        writer.Uint64(p.integers[4]);

        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const constant& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();

        writer.Key("value");
        writer.Double(p.reals[0]);
        writer.Key("offset");
        writer.Double(p.reals[1]);

        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer&          writer,
               const component& c,
               const constant& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();
        writer.Key("value");
        writer.Double(p.reals[0]);
        writer.Key("offset");
        writer.Double(p.reals[1]);
        writer.Key("type");

        const auto type = (0 <= p.integers[0] && p.integers[0] < 5)
                            ? enum_cast<constant::init_type>(p.integers[0])
                            : constant::init_type::constant;

        switch (type) {
        case constant::init_type::constant:
            writer.String("constant");
            break;
        case constant::init_type::incoming_component_all:
            writer.String("incoming_component_all");
            break;
        case constant::init_type::outcoming_component_all:
            writer.String("outcoming_component_all");
            break;
        case constant::init_type::incoming_component_n: {
            writer.String("incoming_component_n");
            writer.Key("port");

            const auto port = enum_cast<port_id>(p.integers[1]);
            if (c.x.exists(port)) {
                const auto& str = c.x.get<port_str>(port);
                writer.String(str.c_str());
            } else {
                writer.String("");
            }
        } break;
        case constant::init_type::outcoming_component_n: {
            writer.String("outcoming_component_n");
            writer.Key("port");

            const auto port = enum_cast<port_id>(p.integers[1]);
            if (c.y.exists(port)) {
                const auto& str = c.y.get<port_str>(port);
                writer.String(str.c_str());
            } else {
                writer.String("");
            }
        } break;
        }

        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss1_cross& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();
        writer.Key("threshold");
        writer.Double(p.reals[0]);
        writer.Key("detect-up");
        writer.Bool(p.integers[0]);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss2_cross& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();
        writer.Key("threshold");
        writer.Double(p.reals[0]);
        writer.Key("detect-up");
        writer.Bool(p.integers[0]);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss3_cross& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();
        writer.Key("threshold");
        writer.Double(p.reals[0]);
        writer.Key("detect-up");
        writer.Bool(p.integers[0]);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss1_filter& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();
        writer.Key("lower-threshold");
        writer.Double(p.reals[0]);
        writer.Key("upper-threshold");
        writer.Double(p.reals[1]);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss2_filter& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();
        writer.Key("lower-threshold");
        writer.Double(p.reals[0]);
        writer.Key("upper-threshold");
        writer.Double(p.reals[1]);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss3_filter& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();
        writer.Key("lower-threshold");
        writer.Double(p.reals[0]);
        writer.Key("upper-threshold");
        writer.Double(p.reals[1]);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss1_power& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();
        writer.Key("n");
        writer.Double(p.reals[0]);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss2_power& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();
        writer.Key("n");
        writer.Double(p.reals[0]);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const qss3_power& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();
        writer.Key("n");
        writer.Double(p.reals[0]);
        writer.EndObject();
    }

    template<typename Writer, std::size_t QssLevel>
    void write(Writer& writer,
               const abstract_square<QssLevel>& /*dyn*/,
               const parameter& /*p*/) noexcept
    {
        writer.StartObject();
        writer.EndObject();
    }

    template<typename Writer, std::size_t PortNumber>
    void write(Writer& writer,
               const accumulator<PortNumber>& /*dyn*/,
               const parameter& /*p*/) noexcept
    {
        writer.StartObject();
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const time_func& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();
        writer.Key("function");
        writer.String(p.integers[0] == 0   ? "time"
                      : p.integers[0] == 1 ? "square"
                                           : "sin");
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const logical_and_2& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();
        writer.Key("value-0");
        writer.Bool(p.integers[0]);
        writer.Key("value-1");
        writer.Bool(p.integers[1]);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const logical_and_3& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();
        writer.Key("value-0");
        writer.Bool(p.integers[0]);
        writer.Key("value-1");
        writer.Bool(p.integers[1]);
        writer.Key("value-2");
        writer.Bool(p.integers[2]);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const logical_or_2& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();
        writer.Key("value-0");
        writer.Bool(p.integers[0]);
        writer.Key("value-1");
        writer.Bool(p.integers[1]);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const logical_or_3& /*dyn*/,
               const parameter& p) noexcept
    {
        writer.StartObject();
        writer.Key("value-0");
        writer.Bool(p.integers[0]);
        writer.Key("value-1");
        writer.Bool(p.integers[1]);
        writer.Key("value-2");
        writer.Bool(p.integers[2]);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer,
               const logical_invert& /*dyn*/,
               const parameter& /*p*/) noexcept
    {
        writer.StartObject();
        writer.EndObject();
    }

    template<typename Writer>
    void write_modeling_dynamics(const modeling& mod,
                                 Writer&         writer,
                                 const hsm_wrapper& /*dyn*/,
                                 const parameter& p) noexcept
    {
        writer.StartObject();

        writer.Key("hsm");
        writer.StartObject();

        const auto id = enum_cast<component_id>(p.integers[0]);
        auto*      c  = mod.components.try_to_get<component>(id);
        if (c)
            write_child_component_path(mod, *c, writer);

        writer.EndObject();

        writer.Key("i1");
        writer.Int64(p.integers[1]);
        writer.Key("i2");
        writer.Int64(p.integers[2]);
        writer.Key("r1");
        writer.Double(p.reals[0]);
        writer.Key("r2");
        writer.Double(p.reals[1]);
        writer.Key("timeout");
        writer.Double(p.reals[2]);

        if (c) {
            if (auto* hc = mod.hsm_components.try_to_get(c->id.hsm_id)) {
                if (hc->machine.is_using_source()) {
                    writer.Key("source-type");
                    writer.Int64(p.integers[3]);
                    writer.Key("source-id");
                    writer.Uint64(p.integers[4]);
                }
            }
        }

        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer&                                         writer,
               std::string_view                                name,
               const hierarchical_state_machine::state_action& state) noexcept
    {
        writer.Key(name.data(), static_cast<rapidjson::SizeType>(name.size()));
        writer.StartObject();
        writer.Key("var-1");
        writer.Int(static_cast<int>(state.var1));
        writer.Key("var-2");
        writer.Int(static_cast<int>(state.var2));
        writer.Key("type");
        writer.Int(static_cast<int>(state.type));

        if (state.var2 == hierarchical_state_machine::variable::constant_i) {
            writer.Key("constant-i");
            writer.Int(state.constant.i);
        } else if (state.var2 ==
                   hierarchical_state_machine::variable::constant_r) {
            writer.Key("constant-r");
            writer.Double(state.constant.f);
        }

        writer.EndObject();
    }

    template<typename Writer>
    void write(
      Writer&                                             writer,
      std::string_view                                    name,
      const hierarchical_state_machine::condition_action& state) noexcept
    {
        writer.Key(name.data(), static_cast<rapidjson::SizeType>(name.size()));
        writer.StartObject();
        writer.Key("var-1");
        writer.Int(static_cast<int>(state.var1));
        writer.Key("var-2");
        writer.Int(static_cast<int>(state.var2));
        writer.Key("type");
        writer.Int(static_cast<int>(state.type));

        if (state.type == hierarchical_state_machine::condition_type::port) {
            u8 port = 0, mask = 0;
            state.get(port, mask);

            writer.Key("port");
            writer.Int(static_cast<int>(port));
            writer.Key("mask");
            writer.Int(static_cast<int>(mask));
        }

        if (state.var2 == hierarchical_state_machine::variable::constant_i) {
            writer.Key("constant-i");
            writer.Int(state.constant.i);
        } else if (state.var2 ==
                   hierarchical_state_machine::variable::constant_r) {
            writer.Key("constant-r");
            writer.Double(state.constant.f);
        }

        writer.EndObject();
    }

    template<typename Writer>
    void write_constant_sources(const external_source& srcs, Writer& w) noexcept
    {
        w.Key("constant-sources");
        w.StartArray();

        const constant_source* src = nullptr;
        while (srcs.constant_sources.next(src)) {
            w.StartObject();
            w.Key("id");
            w.Uint64(ordinal(srcs.constant_sources.get_id(*src)));
            w.Key("name");
            w.String(src->name.c_str());
            w.Key("parameters");

            w.StartArray();
            for (auto e = src->length, i = 0u; i != e; ++i)
                w.Double(src->buffer[i]);
            w.EndArray();

            w.EndObject();
        }

        w.EndArray();
    }

    template<typename Writer>
    void write_binary_file_sources(
      const external_source&                     srcs,
      const data_array<file_path, file_path_id>& files,
      Writer&                                    w) noexcept
    {
        w.Key("binary-file-sources");
        w.StartArray();

        const binary_file_source* src = nullptr;
        while (srcs.binary_file_sources.next(src)) {
            if (const auto* f = files.try_to_get(src->file_id); f) {
                w.StartObject();
                w.Key("id");
                w.Uint64(ordinal(srcs.binary_file_sources.get_id(*src)));
                w.Key("name");
                w.String(src->name.c_str());
                w.Key("max-clients");
                w.Uint(src->max_clients);
                w.Key("path");
                w.String(f->path.data(),
                         static_cast<rapidjson::SizeType>(f->path.size()));
                w.EndObject();
            }
        }

        w.EndArray();
    }

    template<typename Writer>
    void write_text_file_sources(
      const external_source&                     srcs,
      const data_array<file_path, file_path_id>& files,
      Writer&                                    w) noexcept
    {
        w.Key("text-file-sources");
        w.StartArray();

        const text_file_source* src = nullptr;
        std::string             filepath;

        while (srcs.text_file_sources.next(src)) {
            if (const auto* f = files.try_to_get(src->file_id); f) {
                w.StartObject();
                w.Key("id");
                w.Uint64(ordinal(srcs.text_file_sources.get_id(*src)));
                w.Key("name");
                w.String(src->name.c_str());
                w.Key("path");
                w.String(f->path.data(),
                         static_cast<rapidjson::SizeType>(f->path.size()));
                w.EndObject();
            }
        }

        w.EndArray();
    }

    template<typename Writer>
    void write_binary_file_sources(const external_source& srcs,
                                   Writer&                w) noexcept
    {
        w.Key("binary-file-sources");
        w.StartArray();

        const binary_file_source* src = nullptr;
        while (srcs.binary_file_sources.next(src)) {
            const auto str = src->file_path.string();

            w.StartObject();
            w.Key("id");
            w.Uint64(ordinal(srcs.binary_file_sources.get_id(*src)));
            w.Key("name");
            w.String(src->name.c_str());
            w.Key("max-clients");
            w.Uint(src->max_clients);
            w.Key("path");
            w.String(str.data(), static_cast<rapidjson::SizeType>(str.size()));
            w.EndObject();
        }

        w.EndArray();
    }

    template<typename Writer>
    void write_text_file_sources(const external_source& srcs,
                                 Writer&                w) noexcept
    {
        w.Key("text-file-sources");
        w.StartArray();

        const text_file_source* src = nullptr;
        while (srcs.text_file_sources.next(src)) {
            const auto str = src->file_path.string();
            w.StartObject();
            w.Key("id");
            w.Uint64(ordinal(srcs.text_file_sources.get_id(*src)));
            w.Key("name");
            w.String(src->name.c_str());
            w.Key("path");
            w.String(str.data(), static_cast<rapidjson::SizeType>(str.size()));
            w.EndObject();
        }

        w.EndArray();
    }

    template<typename Writer>
    void write_random_sources(const external_source& srcs, Writer& w) noexcept
    {
        w.Key("random-sources");
        w.StartArray();

        const random_source* src = nullptr;
        while (srcs.random_sources.next(src)) {
            w.StartObject();
            w.Key("id");
            w.Uint64(ordinal(srcs.random_sources.get_id(*src)));
            w.Key("name");
            w.String(src->name.c_str());
            w.Key("type");
            w.String(distribution_str(src->distribution));

            switch (src->distribution) {
            case distribution_type::uniform_int:
                w.Key("a");
                w.Int(src->a32);
                w.Key("b");
                w.Int(src->b32);
                break;

            case distribution_type::uniform_real:
                w.Key("a");
                w.Double(src->a);
                w.Key("b");
                w.Double(src->b);
                break;

            case distribution_type::bernouilli:
                w.Key("p");
                w.Double(src->p);
                ;
                break;

            case distribution_type::binomial:
                w.Key("t");
                w.Int(src->t32);
                w.Key("p");
                w.Double(src->p);
                break;

            case distribution_type::negative_binomial:
                w.Key("t");
                w.Int(src->t32);
                w.Key("p");
                w.Double(src->p);
                break;

            case distribution_type::geometric:
                w.Key("p");
                w.Double(src->p);
                break;

            case distribution_type::poisson:
                w.Key("mean");
                w.Double(src->mean);
                break;

            case distribution_type::exponential:
                w.Key("lambda");
                w.Double(src->lambda);
                break;

            case distribution_type::gamma:
                w.Key("alpha");
                w.Double(src->alpha);
                w.Key("beta");
                w.Double(src->beta);
                break;

            case distribution_type::weibull:
                w.Key("a");
                w.Double(src->a);
                w.Key("b");
                w.Double(src->b);
                break;

            case distribution_type::exterme_value:
                w.Key("a");
                w.Double(src->a);
                w.Key("b");
                w.Double(src->b);
                break;

            case distribution_type::normal:
                w.Key("mean");
                w.Double(src->mean);
                w.Key("stddev");
                w.Double(src->stddev);
                break;

            case distribution_type::lognormal:
                w.Key("m");
                w.Double(src->m);
                w.Key("s");
                w.Double(src->s);
                break;

            case distribution_type::chi_squared:
                w.Key("n");
                w.Double(src->n);
                break;

            case distribution_type::cauchy:
                w.Key("a");
                w.Double(src->a);
                w.Key("b");
                w.Double(src->b);
                break;

            case distribution_type::fisher_f:
                w.Key("m");
                w.Double(src->m);
                w.Key("n");
                w.Double(src->n);
                break;

            case distribution_type::student_t:
                w.Key("n");
                w.Double(src->n);
                break;
            }

            w.EndObject();
        }

        w.EndArray();
    }

    template<typename Writer>
    void write_child_component_path(Writer&               w,
                                    const registred_path& reg,
                                    const dir_path&       dir,
                                    const file_path&      file) noexcept
    {
        w.Key("path");
        w.String(reg.name.begin(), reg.name.size());

        w.Key("directory");
        w.String(dir.path.begin(), dir.path.size());

        w.Key("file");
        w.String(file.path.begin(), file.path.size());
    }

    template<typename Writer>
    void write_child_component_path(const modeling&  mod,
                                    const component& compo,
                                    Writer&          w) noexcept
    {
        auto* reg = mod.registred_paths.try_to_get(compo.reg_path);
        debug::ensure(reg);
        debug::ensure(not reg->path.empty());
        debug::ensure(not reg->name.empty());

        auto* dir = mod.dir_paths.try_to_get(compo.dir);
        debug::ensure(dir);
        debug::ensure(not dir->path.empty());

        auto* file = mod.file_paths.try_to_get(compo.file);
        debug::ensure(file);
        debug::ensure(not file->path.empty());

        if (reg and dir and file)
            write_child_component_path(w, *reg, *dir, *file);
    }

    template<typename Writer>
    void write_child_component(const modeling&    mod,
                               const component_id compo_id,
                               Writer&            w) noexcept
    {
        if (auto* compo = mod.components.try_to_get<component>(compo_id);
            compo) {
            w.Key("component-type");
            w.String(component_type_names[ordinal(compo->type)]);

            switch (compo->type) {
            case component_type::none:
                break;
            case component_type::grid:
                write_child_component_path(mod, *compo, w);
                break;
            case component_type::graph:
                write_child_component_path(mod, *compo, w);
                break;
            case component_type::generic:
                write_child_component_path(mod, *compo, w);
                break;
            case component_type::hsm:
                write_child_component_path(mod, *compo, w);
                break;
            }
        } else {
            w.Key("component-type");
            w.String(component_type_names[ordinal(component_type::none)]);
        }
    }

    template<typename Writer>
    void write_child_model(const modeling&  mod,
                           const component& compo,
                           model&           mdl,
                           const parameter& p,
                           Writer&          w) noexcept
    {
        w.Key("dynamics");

        dispatch(mdl, [&]<typename Dynamics>(Dynamics& dyn) noexcept {
            if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
                write_modeling_dynamics(mod, w, dyn, p);
            } else if constexpr (std::is_same_v<Dynamics, constant>) {
                write(w, compo, dyn, p);
            } else {
                write(w, dyn, p);
            }
        });
    }

    template<typename Writer>
    void write_connection_packs(const modeling&  mod,
                                const component& compo,
                                Writer&          w) noexcept
    {
        for (const auto& con : compo.input_connection_pack) {
            if (compo.x.exists(con.parent_port) and
                mod.components.exists(con.child_component) and
                mod.components.get<component>(con.child_component)
                  .x.exists(con.child_port)) {
                w.StartObject();
                w.Key("type");
                w.String("input-pack");
                w.Key("port");
                w.String(compo.x.get<port_str>(con.parent_port).c_str());
                write_child_component(mod, con.child_component, w);
                w.String("child-port");
                w.String(mod.components.get<component>(con.child_component)
                           .x.get<port_str>(con.child_port)
                           .c_str());
                w.EndObject();
            }
        }

        for (const auto& con : compo.output_connection_pack) {
            if (compo.y.exists(con.parent_port) and
                mod.components.exists(con.child_component) and
                mod.components.get<component>(con.child_component)
                  .y.exists(con.child_port)) {
                w.StartObject();
                w.Key("type");
                w.String("output-pack");
                w.Key("port");
                w.String(compo.y.get<port_str>(con.parent_port).c_str());
                write_child_component(mod, con.child_component, w);
                w.String("child-port");
                w.String(mod.components.get<component>(con.child_component)
                           .y.get<port_str>(con.child_port)
                           .c_str());
                w.EndObject();
            }
        }
    }

    template<typename Writer>
    void write_child(const modeling&                 mod,
                     const component&                compo,
                     const generic_component&        gen,
                     const generic_component::child& ch,
                     Writer&                         w) noexcept
    {
        const auto child_id  = gen.children.get_id(ch);
        const auto child_idx = get_index(child_id);

        w.StartObject();
        w.Key("id");
        w.Uint64(get_index(child_id));

        w.Key("x");
        w.Double(gen.children_positions[child_idx].x);
        w.Key("y");
        w.Double(gen.children_positions[child_idx].y);
        w.Key("name");
        w.String(gen.children_names[child_idx].c_str());

        w.Key("configurable");
        w.Bool(ch.flags[child_flags::configurable]);

        w.Key("observable");
        w.Bool(ch.flags[child_flags::observable]);

        if (ch.type == child_type::component) {
            const auto compo_id = ch.id.compo_id;
            if (auto* compo = mod.components.try_to_get<component>(compo_id);
                compo) {
                w.Key("type");
                w.String("component");

                write_child_component(mod, compo_id, w);
            }
        } else {
            model mdl;
            mdl.type = ch.id.mdl_type;

            dispatch(mdl, [&]<typename Dynamics>(Dynamics& dyn) -> void {
                std::construct_at<Dynamics>(&dyn);
            });

            w.Key("type");
            w.String(dynamics_type_names[ordinal(ch.id.mdl_type)]);

            write_child_model(
              mod, compo, mdl, gen.children_parameters[child_idx], w);
        }

        w.EndObject();
    }

    template<typename Writer>
    void write_generic_component_children(const modeling&          mod,
                                          const component&         compo,
                                          const generic_component& simple_compo,
                                          Writer&                  w) noexcept
    {
        w.Key("children");
        w.StartArray();

        for (const auto& c : simple_compo.children)
            write_child(mod, compo, simple_compo, c, w);

        w.EndArray();
    }

    template<typename Writer>
    void write_component_ports(const component& compo, Writer& w) noexcept
    {
        if (not compo.x.empty()) {
            w.Key("x");
            w.StartArray();

            compo.x.for_each([&](auto /*id*/,
                                 const auto& type,
                                 const auto& str,
                                 auto&       pos) noexcept {
                w.StartObject();
                w.Key("name");
                w.String(str.c_str());
                w.Key("type");
                w.String(port_option_names[ordinal(type)]);
                w.Key("x");
                w.Double(pos.x);
                w.Key("y");
                w.Double(pos.y);
                w.EndObject();
            });

            w.EndArray();
        }

        if (not compo.y.empty()) {
            w.Key("y");
            w.StartArray();

            compo.y.for_each([&](auto /*id*/,
                                 const auto& type,
                                 const auto& str,
                                 auto&       pos) noexcept {
                w.StartObject();
                w.Key("name");
                w.String(str.c_str());
                w.Key("type");
                w.String(port_option_names[ordinal(type)]);
                w.Key("x");
                w.Double(pos.x);
                w.Key("y");
                w.Double(pos.y);
                w.EndObject();
            });

            w.EndArray();
        }
    }

    template<typename Writer>
    void write_input_connection(const modeling&                 mod,
                                const component&                parent,
                                const generic_component&        gen,
                                const port_id                   x,
                                const generic_component::child& dst,
                                const connection::port          dst_x,
                                Writer&                         w) noexcept
    {
        w.StartObject();
        w.Key("type");
        w.String("input");
        w.Key("port");
        w.String(parent.x.get<port_str>(x).c_str());
        w.Key("destination");
        w.Uint64(get_index(gen.children.get_id(dst)));
        w.Key("port-destination");

        if (dst.type == child_type::model) {
            w.Int(dst_x.model);
        } else {
            const auto* compo_child =
              mod.components.try_to_get<component>(dst.id.compo_id);
            if (compo_child and compo_child->x.exists(dst_x.compo))
                w.String(compo_child->x.get<port_str>(dst_x.compo).c_str());
        }

        w.EndObject();
    }

    template<typename Writer>
    void write_output_connection(const modeling&                 mod,
                                 const component&                parent,
                                 const generic_component&        gen,
                                 const port_id                   y,
                                 const generic_component::child& src,
                                 const connection::port          src_y,
                                 Writer&                         w) noexcept
    {
        w.StartObject();
        w.Key("type");
        w.String("output");
        w.Key("port");
        w.String(parent.y.get<port_str>(y).c_str());
        w.Key("source");
        w.Uint64(get_index(gen.children.get_id(src)));
        w.Key("port-source");

        if (src.type == child_type::model) {
            w.Int(src_y.model);
        } else {
            const auto* compo_child =
              mod.components.try_to_get<component>(src.id.compo_id);
            if (compo_child and compo_child->y.exists(src_y.compo)) {
                w.String(compo_child->y.get<port_str>(src_y.compo).c_str());
            }
        }
        w.EndObject();
    }

    template<typename Writer>
    void write_internal_connection(const modeling&                 mod,
                                   const generic_component&        gen,
                                   const generic_component::child& src,
                                   const connection::port          src_y,
                                   const generic_component::child& dst,
                                   const connection::port          dst_x,
                                   Writer&                         w) noexcept
    {
        const char* src_str = nullptr;
        const char* dst_str = nullptr;
        int         src_int = -1;
        int         dst_int = -1;

        if (src.type == child_type::component) {
            auto* compo = mod.components.try_to_get<component>(src.id.compo_id);
            if (compo and compo->y.exists(src_y.compo))
                src_str = compo->y.get<port_str>(src_y.compo).c_str();
        } else
            src_int = src_y.model;

        if (dst.type == child_type::component) {
            auto* compo = mod.components.try_to_get<component>(dst.id.compo_id);
            if (compo and compo->x.exists(dst_x.compo))
                dst_str = compo->x.get<port_str>(dst_x.compo).c_str();
        } else
            dst_int = dst_x.model;

        if ((src_str or src_int >= 0) and (dst_str or dst_int >= 0)) {
            w.StartObject();
            w.Key("type");
            w.String("internal");
            w.Key("source");
            w.Uint64(get_index(gen.children.get_id(src)));
            w.Key("port-source");
            if (src_str)
                w.String(src_str);
            else
                w.Int(src_int);

            w.Key("destination");
            w.Uint64(get_index(gen.children.get_id(dst)));
            w.Key("port-destination");
            if (dst_str)
                w.String(dst_str);
            else
                w.Int(dst_int);
            w.EndObject();
        }
    }

    template<typename Writer>
    void write_generic_component_connections(const modeling&          mod,
                                             const component&         compo,
                                             const generic_component& gen,
                                             Writer& w) noexcept
    {
        w.Key("connections");
        w.StartArray();

        for (const auto& con : gen.connections)
            if (auto* c_src = gen.children.try_to_get(con.src); c_src)
                if (auto* c_dst = gen.children.try_to_get(con.dst); c_dst)
                    write_internal_connection(mod,
                                              gen,
                                              *c_src,
                                              con.index_src,
                                              *c_dst,
                                              con.index_dst,
                                              w);

        for (const auto& con : gen.input_connections)
            if (auto* c = gen.children.try_to_get(con.dst); c)
                if (compo.x.exists(con.x))
                    write_input_connection(
                      mod, compo, gen, con.x, *c, con.port, w);

        for (const auto& con : gen.output_connections)
            if (auto* c = gen.children.try_to_get(con.src); c)
                if (compo.y.exists(con.y))
                    write_output_connection(
                      mod, compo, gen, con.y, *c, con.port, w);

        write_connection_packs(mod, compo, w);

        w.EndArray();
    }

    template<typename Writer>
    void write_generic_component(const modeling&          mod,
                                 const component&         compo,
                                 const generic_component& s_compo,
                                 Writer&                  w) noexcept
    {
        write_generic_component_children(mod, compo, s_compo, w);
        write_generic_component_connections(mod, compo, s_compo, w);
    }

    template<typename Writer>
    void write_grid_component(const modeling&       mod,
                              const component&      compo,
                              const grid_component& grid,
                              Writer&               w) noexcept
    {
        w.Key("rows");
        w.Int(grid.row());
        w.Key("columns");
        w.Int(grid.column());
        w.Key("in-connection-type");
        w.Int(ordinal(grid.in_connection_type));
        w.Key("out-connection-type");
        w.Int(ordinal(grid.out_connection_type));

        w.Key("children");
        w.StartArray();
        for (const auto elem : grid.children()) {
            w.StartObject();
            write_child_component(mod, elem, w);
            w.EndObject();
        }
        w.EndArray();

        w.Key("connections");
        w.StartArray();
        for (const auto& con : grid.input_connections) {
            const auto pos            = grid.pos(con.row, con.col);
            const auto child_compo_id = grid.children()[pos];

            if (const auto* c =
                  mod.components.try_to_get<component>(child_compo_id);
                c) {
                if (c->x.exists(con.id)) {
                    w.StartObject();
                    w.Key("type");
                    w.String("input");
                    w.Key("row");
                    w.Int(con.row);
                    w.Key("col");
                    w.Int(con.col);
                    w.Key("id");
                    w.String(c->x.get<port_str>(con.id).c_str());
                    w.Key("x");
                    w.String(compo.x.get<port_str>(con.x).c_str());
                    w.EndObject();
                }
            }
        }

        for (const auto& con : grid.output_connections) {
            const auto pos            = grid.pos(con.row, con.col);
            const auto child_compo_id = grid.children()[pos];

            if (const auto* c =
                  mod.components.try_to_get<component>(child_compo_id);
                c) {
                if (c->x.exists(con.id)) {
                    w.StartObject();
                    w.Key("type");
                    w.String("output");
                    w.Key("row");
                    w.Int(con.row);
                    w.Key("col");
                    w.Int(con.col);
                    w.Key("id");
                    w.String(c->y.get<port_str>(con.id).c_str());
                    w.Key("y");
                    w.String(compo.x.get<port_str>(con.y).c_str());
                    w.EndObject();
                }
            }
        }

        write_connection_packs(mod, compo, w);

        w.EndArray();
    }

    template<typename Writer>
    void write_graph_component(const modeling&        mod,
                               const component&       compo,
                               const graph_component& g,
                               Writer&                w) noexcept
    {
        w.Key("graph-type");

        switch (g.g_type) {
        case graph_component::graph_type::dot_file: {
            w.String("dot-file");
            auto&      p    = g.param.dot;
            dir_path*  dir  = nullptr;
            file_path* file = nullptr;
            if (dir = mod.dir_paths.try_to_get(p.dir); dir) {
                w.Key("dir");
                w.String(dir->path.begin(), dir->path.size());
            }

            if (file = mod.file_paths.try_to_get(p.file); file) {
                w.Key("file");
                w.String(file->path.begin(), file->path.size());
            }

            if (dir and file) {
                if (auto f = make_file(mod, *file); f.has_value()) {
                    if (not write_dot_file(mod, g.g, *f)) {
                        mod.journal.push(
                          log_level::error,
                          [](auto&       t,
                             auto&       m,
                             const auto& dir,
                             const auto& file) noexcept {
                              t = "Fail to write dot file";
                              format(m,
                                     "Fail to write {} in {}",
                                     dir.path.c_str(),
                                     file.path.c_str());
                          },
                          *dir,
                          *file);
                    }
                }
            } else {
                mod.journal.push(log_level::error,
                                 [](auto& t, auto& m) noexcept {
                                     t = "Fail to write dot file";
                                     m = "File path is undefined";
                                 });
            }
            break;
        }

        case graph_component::graph_type::scale_free: {
            w.String("scale-free");
            w.Key("alpha");
            w.Double(g.param.scale.alpha);
            w.Key("beta");
            w.Double(g.param.scale.beta);
            w.Key("nodes");
            w.Int(g.param.scale.nodes);

            w.Key("children");
            w.StartArray();
            for (const auto id : g.g.nodes) {
                w.StartObject();
                write_child_component(mod, g.g.node_components[id], w);
                w.EndObject();
            }
            w.EndArray();
            break;
        }

        case graph_component::graph_type::small_world: {
            w.String("small-world");
            w.Key("probability");
            w.Double(g.param.small.probability);
            w.Key("k");
            w.Int(g.param.small.k);
            w.Key("nodes");
            w.Int(g.param.small.nodes);

            w.Key("children");
            w.StartArray();
            for (const auto id : g.g.nodes) {

                w.StartObject();
                write_child_component(mod, g.g.node_components[id], w);
                w.EndObject();
            }
            w.EndArray();
            break;
        }
        }

        w.Key("connections");
        w.StartArray();
        for (const auto& con : g.input_connections) {
            if (not g.g.nodes.exists(con.v))
                continue;

            const auto child_compo_id = g.g.node_components[con.v];
            if (not mod.components.exists(child_compo_id))
                continue;

            const auto& c = mod.components.get<component>(child_compo_id);
            if (c.x.exists(con.id)) {
                w.StartObject();
                w.Key("type");
                w.String("input");
                w.Key("v");
                w.Int(get_index(con.v));
                w.Key("id");
                w.String(c.x.get<port_str>(con.id).c_str());
                w.Key("x");
                w.String(compo.x.get<port_str>(con.x).c_str());
                w.EndObject();
            }
        }

        for (const auto& con : g.output_connections) {
            if (not g.g.nodes.exists(con.v))
                continue;

            const auto child_compo_id = g.g.node_components[con.v];
            if (not mod.components.exists(child_compo_id))
                continue;

            const auto& c = mod.components.get<component>(child_compo_id);
            if (c.x.exists(con.id)) {
                w.StartObject();
                w.Key("type");
                w.String("output");
                w.Key("v");
                w.Int(get_index(con.v));
                w.Key("id");
                w.String(c.y.get<port_str>(con.id).c_str());
                w.Key("y");
                w.String(compo.x.get<port_str>(con.y).c_str());
                w.EndObject();
            }
        }

        write_connection_packs(mod, compo, w);

        w.EndArray();
    }

    template<typename Writer>
    void write_hierarchical_state_machine_state(
      const hierarchical_state_machine::state& state,
      const std::string_view&                  name,
      const position&                          pos,
      const std::integral auto                 index,
      Writer&                                  w) noexcept
    {
        w.StartObject();
        w.Key("id");
        w.Uint(static_cast<u32>(index));

        if (not name.empty()) {
            w.Key("name");
            w.String(name.data(),
                     static_cast<rapidjson::SizeType>(name.size()));
        }

        write(w, "enter", state.enter_action);
        write(w, "exit", state.exit_action);
        write(w, "if", state.if_action);
        write(w, "else", state.else_action);
        write(w, "condition", state.condition);

        w.Key("if-transition");
        w.Int(state.if_transition);
        w.Key("else-transition");
        w.Int(state.else_transition);
        w.Key("super-id");
        w.Int(state.super_id);
        w.Key("sub-id");
        w.Int(state.sub_id);

        w.Key("x");
        w.Double(pos.x);
        w.Key("y");
        w.Double(pos.y);

        w.EndObject();
    }

    template<typename Writer>
    void write_hierarchical_state_machine(const hierarchical_state_machine& hsm,
                                          std::span<const name_str> names,
                                          std::span<const position> positions,
                                          Writer&                   w) noexcept
    {
        w.Key("states");
        w.StartArray();

        constexpr auto length = hierarchical_state_machine::max_number_of_state;
        constexpr auto invalid = hierarchical_state_machine::invalid_state_id;

        std::array<bool, length> states_to_write{};
        states_to_write.fill(false);

        if (hsm.top_state != invalid)
            states_to_write[hsm.top_state] = true;

        for (auto i = 0; i != length; ++i) {
            if (hsm.states[i].if_transition != invalid)
                states_to_write[hsm.states[i].if_transition] = true;
            if (hsm.states[i].else_transition != invalid)
                states_to_write[hsm.states[i].else_transition] = true;
            if (hsm.states[i].super_id != invalid)
                states_to_write[hsm.states[i].super_id] = true;
            if (hsm.states[i].sub_id != invalid)
                states_to_write[hsm.states[i].sub_id] = true;
        }

        if (not positions.empty()) {
            for (auto i = 0; i != length; ++i) {
                if (states_to_write[i]) {
                    write_hierarchical_state_machine_state(
                      hsm.states[i], names[i].sv(), positions[i], i, w);
                }
            }
        }
        w.EndArray();

        w.Key("top");
        w.Uint(hsm.top_state);
    }

    template<typename Writer>
    void write_hsm_component(const hsm_component& hsm, Writer& w) noexcept
    {
        write_hierarchical_state_machine(
          hsm.machine,
          std::span{ hsm.names.begin(), hsm.names.size() },
          std::span{ hsm.positions.begin(), hsm.positions.size() },
          w);

        w.Key("i1");
        w.Int(hsm.i1);
        w.Key("i2");
        w.Int(hsm.i2);
        w.Key("r1");
        w.Double(hsm.r1);
        w.Key("r2");
        w.Double(hsm.r2);
        w.Key("timeout");
        w.Double(hsm.timeout);
        w.Key("source-type");
        w.Int(ordinal(hsm.src.type));
        w.Key("source-id");

        switch (hsm.src.type) {
        case source::source_type::binary_file:
            w.Uint(get_index(hsm.src.id.binary_file_id));
            break;
        case source::source_type::text_file:
            w.Uint(get_index(hsm.src.id.text_file_id));
            break;
        case source::source_type::random:
            w.Uint(get_index(hsm.src.id.random_id));
            break;
        case source::source_type::constant:
            w.Uint(get_index(hsm.src.id.constant_id));
            break;
        }

        w.Key("constants");
        w.StartArray();
        for (const auto& c : hsm.machine.constants)
            w.Double(c);
        w.EndArray();
    }

    template<typename Writer>
    void do_component_save(Writer& w, modeling& mod, component& compo) noexcept
    {
        w.StartObject();

        w.Key("name");
        w.String(compo.name.c_str());

        write_constant_sources(compo.srcs, w);
        write_binary_file_sources(compo.srcs, mod.file_paths, w);
        write_text_file_sources(compo.srcs, mod.file_paths, w);
        write_random_sources(compo.srcs, w);

        w.Key("colors");
        w.StartArray();
        auto& color =
          mod.components.get<component_color>(mod.components.get_id(compo));
        w.Double(color[0]);
        w.Double(color[1]);
        w.Double(color[2]);
        w.Double(color[3]);
        w.EndArray();

        write_component_ports(compo, w);

        w.Key("type");
        w.String(component_type_names[ordinal(compo.type)]);

        switch (compo.type) {
        case component_type::none:
            break;

        case component_type::generic: {
            auto* p = mod.generic_components.try_to_get(compo.id.generic_id);
            if (p)
                write_generic_component(mod, compo, *p, w);
        } break;

        case component_type::grid: {
            auto* p = mod.grid_components.try_to_get(compo.id.grid_id);
            if (p)
                write_grid_component(mod, compo, *p, w);
        } break;

        case component_type::graph: {
            auto* p = mod.graph_components.try_to_get(compo.id.graph_id);
            if (p)
                write_graph_component(mod, compo, *p, w);
        } break;

        case component_type::hsm: {
            auto* p = mod.hsm_components.try_to_get(compo.id.hsm_id);
            if (p)
                write_hsm_component(*p, w);
        } break;
        }

        w.EndObject();
    }

    /*****************************************************************************
     *
     * project file write part
     *
     ****************************************************************************/

    template<typename Writer>
    void write_color(Writer& w, std::array<u8, 4> color) noexcept
    {
        w.StartArray();
        w.Uint(color[0]);
        w.Uint(color[1]);
        w.Uint(color[2]);
        w.Uint(color[3]);
        w.EndArray();
    }

    template<typename Writer>
    void write_project_unique_id_path(Writer&               w,
                                      const unique_id_path& path) noexcept
    {
        w.StartArray();
        for (const auto& elem : path)
            w.String(elem.data(), elem.size());
        w.EndArray();
    }

    constexpr static std::array<u8, 4> color_white{ 255, 255, 255, 0 };

    template<typename Writer>
    void do_project_save_parameters(Writer& w, project& pj) noexcept
    {
        w.Key("parameters");

        w.StartObject();
        do_project_save_global_parameters(w, pj);
        w.EndObject();
    }

    template<typename Writer>
    void do_project_save_plot_observations(Writer& w, project& pj) noexcept
    {
        w.Key("global");
        w.StartArray();

        for_each_data(pj.variable_observers, [&](auto& plot) noexcept {
            unique_id_path path;

            w.StartObject();
            w.Key("name");
            w.String(plot.name.begin(), plot.name.size());

            w.Key("models");
            w.StartArray();
            plot.for_each([&](const auto id) noexcept {
                w.StartObject();
                const auto idx = get_index(id);
                const auto tn  = plot.get_tn_ids()[idx];
                const auto mdl = plot.get_mdl_ids()[idx];
                const auto str = plot.get_names()[idx];

                if (not str.empty()) {
                    w.Key("name");
                    w.String(str.data(), static_cast<int>(str.size()));
                }

                w.Key("access");
                pj.build_unique_id_path(tn, mdl, path);
                write_project_unique_id_path(w, path);

                w.Key("color");
                write_color(w, color_white);

                w.Key("type");
                w.String("line");
                w.EndObject();
            });
            w.EndArray();

            w.EndObject();
        });

        w.EndArray();
    }

    template<typename Writer>
    void do_project_save_grid_observations(Writer& w, project& pj) noexcept
    {
        w.Key("grid");
        w.StartArray();

        for_each_data(pj.grid_observers, [&](auto& grid) noexcept {
            w.StartObject();
            w.Key("name");
            w.String(grid.name.begin(), grid.name.size());

            unique_id_path path;
            w.Key("grid");
            pj.build_unique_id_path(grid.parent_id, path);
            write_project_unique_id_path(w, path);

            w.Key("access");
            pj.build_unique_id_path(grid.tn_id, grid.mdl_id, path);
            write_project_unique_id_path(w, path);

            w.EndObject();
        });

        w.EndArray();
    }

    template<typename Writer>
    void do_project_save_observations(Writer& w, project& pj) noexcept
    {
        w.Key("observations");

        w.StartObject();
        do_project_save_plot_observations(w, pj);
        do_project_save_grid_observations(w, pj);
        w.EndObject();
    }

    template<typename Writer>
    void write_parameter(Writer& w, const parameter& param) noexcept
    {
        w.StartObject();
        w.Key("real");
        w.StartArray();
        for (auto elem : param.reals)
            w.Double(elem);
        w.EndArray();
        w.Key("integer");
        w.StartArray();
        for (auto elem : param.integers)
            w.Int64(elem);
        w.EndArray();
        w.EndObject();
    }

    template<typename Writer>
    void do_project_save_global_parameters(Writer& w, project& pj) noexcept
    {
        w.Key("global");
        w.StartArray();

        pj.parameters.for_each([&](const auto /*id*/,
                                   const auto& name,
                                   const auto  tn_id,
                                   const auto  mdl_id,
                                   const auto& p) noexcept {
            w.StartObject();
            w.Key("name");
            w.String(name.begin(), name.size());

            unique_id_path path;
            w.Key("access");
            pj.build_unique_id_path(tn_id, mdl_id, path);
            write_project_unique_id_path(w, path);

            w.Key("parameter");
            write_parameter(w, p);

            w.EndObject();
        });

        w.EndArray();
    }

    template<typename Writer>
    void do_project_save_component(Writer&               w,
                                   component&            compo,
                                   const registred_path& reg,
                                   const dir_path&       dir,
                                   const file_path&      file) noexcept
    {
        w.Key("component-type");
        w.String(component_type_names[ordinal(compo.type)]);

        switch (compo.type) {
        case component_type::generic:
        case component_type::grid:
        case component_type::hsm:
            w.Key("component-path");
            w.String(reg.name.c_str(), reg.name.size(), false);

            w.Key("component-directory");
            w.String(dir.path.c_str(), dir.path.size(), false);

            w.Key("component-file");
            w.String(file.path.c_str(), file.path.size(), false);
            break;

        default:
            break;
        };
    }

    template<typename Writer>
    status do_project_save(Writer&    w,
                           project&   pj,
                           modeling&  mod,
                           component& compo) noexcept
    {
        auto* reg = mod.registred_paths.try_to_get(compo.reg_path);
        if (!reg)
            return new_error(modeling_errc::recorded_directory_error);
        if (reg->path.empty())
            return new_error(modeling_errc::directory_error);
        if (reg->name.empty())
            return new_error(modeling_errc::file_error);

        auto* dir = mod.dir_paths.try_to_get(compo.dir);
        if (!dir)
            return new_error(modeling_errc::directory_error);
        if (dir->path.empty())
            return new_error(modeling_errc::directory_error);

        auto* file = mod.file_paths.try_to_get(compo.file);
        if (!file)
            return new_error(modeling_errc::file_error);
        if (file->path.empty())
            return new_error(modeling_errc::file_error);

        w.StartObject();

        w.Key("begin");
        w.Double(pj.sim.limits.begin());
        w.Key("end");
        w.Double(pj.sim.limits.end());

        do_project_save_component(w, compo, *reg, *dir, *file);
        do_project_save_parameters(w, pj);
        do_project_save_observations(w, pj);
        w.EndObject();

        return success();
    }
};

void json_dearchiver::destroy() noexcept
{
    buffer.destroy();
    stack.destroy();

    model_mapping.data.destroy();
    constant_mapping.data.destroy();
    binary_file_mapping.data.destroy();
    random_mapping.data.destroy();
    text_file_mapping.data.destroy();
}

void json_dearchiver::clear() noexcept
{
    buffer.clear();
    stack.clear();

    model_mapping.data.clear();
    constant_mapping.data.clear();
    binary_file_mapping.data.clear();
    random_mapping.data.clear();
    text_file_mapping.data.clear();
}

status irt::json_dearchiver::set_buffer(const u32 buffer_size) noexcept
{
    if (std::cmp_less(buffer.capacity(), buffer_size)) {
        buffer.resize(buffer_size);

        if (std::cmp_less(buffer.capacity(), buffer_size))
            return new_error(json_errc::memory_error);
    }

    return success();
}

status json_dearchiver::operator()(modeling&        mod,
                                   component&       compo,
                                   std::string_view path,
                                   file&            io) noexcept
{
    debug::ensure(io.is_open());
    debug::ensure(io.get_mode() == open_mode::read);
    clear();

    rapidjson::Document doc;

    return read_file_to_buffer(buffer, io)
      .and_then([&]() {
          return parse_json_data(std::span(buffer.data(), buffer.size()), doc);
      })
      .and_then([&]() {
          json_dearchiver::impl i(*this, mod, path);
          return i.parse_component(doc, compo);
      });
}

status json_dearchiver::operator()(project&         pj,
                                   modeling&        mod,
                                   simulation&      sim,
                                   std::string_view path,
                                   file&            io) noexcept
{
    debug::ensure(io.is_open());
    debug::ensure(io.get_mode() == open_mode::read);
    clear();

    rapidjson::Document doc;

    return read_file_to_buffer(buffer, io)
      .and_then([&]() {
          return parse_json_data(std::span(buffer.data(), buffer.size()), doc);
      })
      .and_then([&]() {
          json_dearchiver::impl i(*this, mod, sim, pj, path);
          return i.parse_project(doc);
      });
}

status json_dearchiver::operator()(modeling&       mod,
                                   component&      compo,
                                   std::span<char> io) noexcept
{
    clear();
    rapidjson::Document doc;

    return parse_json_data(io, doc).and_then([&]() {
        json_dearchiver::impl i(*this, mod);
        return i.parse_component(doc, compo);
    });
}

status json_dearchiver::operator()(project&        pj,
                                   modeling&       mod,
                                   simulation&     sim,
                                   std::span<char> io) noexcept
{
    clear();
    rapidjson::Document doc;

    return parse_json_data(io, doc).and_then([&]() {
        json_dearchiver::impl i(*this, mod, sim, pj);
        return i.parse_project(doc);
    });
}

status json_archiver::operator()(modeling&                   mod,
                                 component&                  compo,
                                 file&                       io,
                                 json_archiver::print_option print) noexcept
{
    clear();

    debug::ensure(io.is_open());
    debug::ensure(io.get_mode() == open_mode::write);

    if (not(io.is_open() and io.get_mode() == open_mode::write))
        return new_error(file_errc::open_error);

    auto fp = reinterpret_cast<FILE*>(io.get_handle());
    buffer.resize(4096);

    rapidjson::FileWriteStream os(fp, buffer.data(), buffer.size());
    rapidjson::PrettyWriter<rapidjson::FileWriteStream,
                            rapidjson::UTF8<>,
                            rapidjson::UTF8<>,
                            rapidjson::CrtAllocator,
                            rapidjson::kWriteNanAndInfFlag>
      w(os);

    json_archiver::impl i{ *this };

    switch (print) {
    case json_archiver::print_option::indent_2:
        w.SetIndent(' ', 2);
        i.do_component_save(w, mod, compo);
        break;

    case json_archiver::print_option::indent_2_one_line_array:
        w.SetIndent(' ', 2);
        w.SetFormatOptions(rapidjson::kFormatSingleLineArray);
        i.do_component_save(w, mod, compo);
        break;

    default:
        i.do_component_save(w, mod, compo);
        break;
    }

    return success();
}

status json_archiver::operator()(modeling&                   mod,
                                 component&                  compo,
                                 vector<char>&               out,
                                 json_archiver::print_option print) noexcept
{
    clear();

    rapidjson::StringBuffer buffer;
    buffer.Reserve(4096u);

    json_archiver::impl i{ *this };

    switch (print) {
    case json_archiver::print_option::indent_2: {
        rapidjson::PrettyWriter<rapidjson::StringBuffer,
                                rapidjson::UTF8<>,
                                rapidjson::UTF8<>,
                                rapidjson::CrtAllocator,
                                rapidjson::kWriteNanAndInfFlag>
          w(buffer);
        w.SetIndent(' ', 2);
        i.do_component_save(w, mod, compo);
    } break;

    case json_archiver::print_option::indent_2_one_line_array: {
        rapidjson::PrettyWriter<rapidjson::StringBuffer,
                                rapidjson::UTF8<>,
                                rapidjson::UTF8<>,
                                rapidjson::CrtAllocator,
                                rapidjson::kWriteNanAndInfFlag>
          w(buffer);
        w.SetIndent(' ', 2);
        w.SetFormatOptions(rapidjson::kFormatSingleLineArray);
        i.do_component_save(w, mod, compo);
    } break;

    default: {
        rapidjson::Writer<rapidjson::StringBuffer,
                          rapidjson::UTF8<>,
                          rapidjson::UTF8<>,
                          rapidjson::CrtAllocator,
                          rapidjson::kWriteNanAndInfFlag>
          w(buffer);
        i.do_component_save(w, mod, compo);
    } break;
    }

    auto length = buffer.GetSize();
    auto str    = buffer.GetString();
    out.resize(static_cast<int>(length));
    std::copy_n(str, length, out.data());

    return success();
}

status json_archiver::operator()(project&     pj,
                                 modeling&    mod,
                                 file&        io,
                                 print_option print_options) noexcept
{
    clear();

    debug::ensure(io.is_open());
    debug::ensure(io.get_mode() == open_mode::write);

    if (not(io.is_open() and io.get_mode() == open_mode::write))
        return new_error(file_errc::open_error);

    auto* compo  = mod.components.try_to_get<component>(pj.head());
    auto* parent = pj.tn_head();

    if (not(compo and parent))
        return new_error(project_errc::empty_project);

    debug::ensure(mod.components.get_id(*compo) == parent->id);

    auto* reg = mod.registred_paths.try_to_get(compo->reg_path);
    if (!reg)
        return new_error(modeling_errc::recorded_directory_error);

    auto* dir = mod.dir_paths.try_to_get(compo->dir);
    if (!dir)
        return new_error(modeling_errc::directory_error);

    auto* file = mod.file_paths.try_to_get(compo->file);
    if (!file)
        return new_error(modeling_errc::file_error);

    auto fp = reinterpret_cast<FILE*>(io.get_handle());
    clear();
    buffer.resize(4096);

    rapidjson::FileWriteStream os(fp, buffer.data(), buffer.size());
    rapidjson::PrettyWriter<rapidjson::FileWriteStream,
                            rapidjson::UTF8<>,
                            rapidjson::UTF8<>,
                            rapidjson::CrtAllocator,
                            rapidjson::kWriteNanAndInfFlag>
                        w(os);
    json_archiver::impl i{ *this };

    switch (print_options) {
    case print_option::indent_2:
        w.SetIndent(' ', 2);
        return i.do_project_save(w, pj, mod, *compo);

    case print_option::indent_2_one_line_array:
        w.SetIndent(' ', 2);
        w.SetFormatOptions(rapidjson::kFormatSingleLineArray);
        return i.do_project_save(w, pj, mod, *compo);

    default:
        return i.do_project_save(w, pj, mod, *compo);
    }
}

status json_archiver::operator()(project&      pj,
                                 modeling&     mod,
                                 vector<char>& out,
                                 print_option  print_options) noexcept
{
    clear();

    auto* compo  = mod.components.try_to_get<component>(pj.head());
    auto* parent = pj.tn_head();

    if (!(compo && parent))
        return new_error(project_errc::empty_project);

    debug::ensure(mod.components.get_id(*compo) == parent->id);

    rapidjson::StringBuffer rbuffer;
    rbuffer.Reserve(4096u);
    json_archiver::impl i{ *this };

    switch (print_options) {
    case print_option::indent_2: {
        rapidjson::PrettyWriter<rapidjson::StringBuffer,
                                rapidjson::UTF8<>,
                                rapidjson::UTF8<>,
                                rapidjson::CrtAllocator,
                                rapidjson::kWriteNanAndInfFlag>
          w(rbuffer);
        w.SetIndent(' ', 2);
        i.do_project_save(w, pj, mod, *compo);
    } break;

    case print_option::indent_2_one_line_array: {
        rapidjson::PrettyWriter<rapidjson::StringBuffer,
                                rapidjson::UTF8<>,
                                rapidjson::UTF8<>,
                                rapidjson::CrtAllocator,
                                rapidjson::kWriteNanAndInfFlag>
          w(rbuffer);
        w.SetIndent(' ', 2);
        w.SetFormatOptions(rapidjson::kFormatSingleLineArray);
        i.do_project_save(w, pj, mod, *compo);
    } break;

    default: {
        rapidjson::Writer<rapidjson::StringBuffer,
                          rapidjson::UTF8<>,
                          rapidjson::UTF8<>,
                          rapidjson::CrtAllocator,
                          rapidjson::kWriteNanAndInfFlag>
          w(rbuffer);
        i.do_project_save(w, pj, mod, *compo);
    } break;
    }

    auto length = rbuffer.GetSize();
    auto str    = rbuffer.GetString();
    out.resize(length + 1);
    std::copy_n(str, length, out.data());
    out.back() = '\0';

    return success();
}

void json_archiver::destroy() noexcept
{
    buffer.destroy();

    model_mapping.data.destroy();
    constant_mapping.data.destroy();
    binary_file_mapping.data.destroy();
    random_mapping.data.destroy();
    text_file_mapping.data.destroy();
}

void json_archiver::clear() noexcept
{
    buffer.clear();

    model_mapping.data.clear();
    constant_mapping.data.clear();
    binary_file_mapping.data.clear();
    random_mapping.data.clear();
    text_file_mapping.data.clear();
}

} //  irt
