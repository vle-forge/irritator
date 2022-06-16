// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "internal.hpp"

#include <irritator/file.hpp>
#include <irritator/io.hpp>

#include <rapidjson/error/en.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/reader.h>
#include <rapidjson/writer.h>

namespace irt {

using json_in_stream  = rapidjson::FileReadStream;
using json_reader     = rapidjson::Reader;
using json_out_stream = rapidjson::FileWriteStream;
using json_writer     = rapidjson::PrettyWriter<json_out_stream>;

struct component_setting_handler
{
    enum class stack
    {
        empty,
        top,
        paths,
        paths_array,
        path_object,

        is_fixed_window_placement,
        binary_file_source_capacity,
        children_capacity,
        component_capacity,
        connection_capacity,
        constant_source_capacity,
        description_capacity,
        file_path_capacity,
        model_capacity,
        parameter_capacity,
        port_capacity,
        random_source_capacity,
        random_generator_seed,
        text_file_source_capacity,
        tree_capacity,

        priority
    };

    application*   app;
    registred_path current;
    stack          top;

    bool read_name;
    bool read_path;
    i32  priority;

    component_setting_handler(application* app_) noexcept
      : app(app_)
      , top(stack::empty)
      , read_name(false)
      , read_path(false)
      , priority(0)
    {}

    bool Null() { return false; }

    bool Bool(bool b)
    {
        if (top == stack::is_fixed_window_placement) {
            app->mod_init.is_fixed_window_placement = b;
            app->is_fixed_window_placement          = b;
            top                                     = stack::top;
            return true;
        }

        return false;
    }

    bool Double(double /*d*/) { return false; }

    bool Uint(unsigned u) { return affect(u); }
    bool Int64(int64_t i) { return affect(i); }
    bool Uint64(uint64_t u) { return affect(u); }
    bool Int(int i) { return affect(i); }

    bool RawNumber(const char* /*str*/,
                   rapidjson::SizeType /*length*/,
                   bool /*copy*/)
    {
        return false;
    }

    bool String(const char* str, rapidjson::SizeType /*length*/, bool /*copy*/)
    {
        if (top == stack::path_object) {
            if (read_name) {
                current.name.assign(str);
                read_name = false;
                return true;
            }

            if (read_path) {
                current.path.assign(str);
                read_path = false;
                return true;
            }
        }

        return false;
    }

    template<typename Target, typename Source>
    bool affect(Target& target, const Source source) noexcept
    {
        if (is_numeric_castable<Target>(source)) {
            target = static_cast<Target>(source);
            return true;
        }

        return false;
    }

    template<typename Integer>
    bool affect(Integer value) noexcept
    {
        bool ret = false;

        if (top == stack::binary_file_source_capacity)
            ret = affect(app->mod_init.binary_file_source_capacity, value);
        else if (top == stack::children_capacity)
            ret = affect(app->mod_init.children_capacity, value);
        else if (top == stack::component_capacity)
            ret = affect(app->mod_init.component_capacity, value);
        else if (top == stack::connection_capacity)
            ret = affect(app->mod_init.connection_capacity, value);
        else if (top == stack::constant_source_capacity)
            ret = affect(app->mod_init.constant_source_capacity, value);
        else if (top == stack::description_capacity)
            ret = affect(app->mod_init.description_capacity, value);
        else if (top == stack::file_path_capacity)
            ret = affect(app->mod_init.file_path_capacity, value);
        else if (top == stack::model_capacity)
            ret = affect(app->mod_init.model_capacity, value);
        else if (top == stack::parameter_capacity)
            ret = affect(app->mod_init.parameter_capacity, value);
        else if (top == stack::port_capacity)
            ret = affect(app->mod_init.port_capacity, value);
        else if (top == stack::random_source_capacity)
            ret = affect(app->mod_init.random_source_capacity, value);
        else if (top == stack::random_generator_seed)
            ret = affect(app->mod_init.random_generator_seed, value);
        else if (top == stack::text_file_source_capacity)
            ret = affect(app->mod_init.text_file_source_capacity, value);
        else if (top == stack::tree_capacity)
            ret = affect(app->mod_init.tree_capacity, value);
        else if (top == stack::priority) {
            ret = affect(priority, value);
            top = stack::path_object;
            return ret;
        }

        top = stack::top;

        return ret;
    }

    bool StartObject()
    {
        if (top == stack::empty) {
            top = stack::top;
            return true;
        }

        if (top == stack::paths_array) {
            top = stack::path_object;
            return true;
        }

        return false;
    }

    bool Key(const char* str, rapidjson::SizeType length, bool /*copy*/)
    {
        std::string_view sv{ str, length };

        if (top == stack::top) {
            if (sv == "paths") {
                top = stack::paths;
            } else if (sv == "is_fixed_window_placement") {
                top = stack::is_fixed_window_placement;
            } else if (sv == "binary_file_source_capacity") {
                top = stack::binary_file_source_capacity;
            } else if (sv == "children_capacity") {
                top = stack::children_capacity;
            } else if (sv == "component_capacity") {
                top = stack::component_capacity;
            } else if (sv == "connection_capacity") {
                top = stack::connection_capacity;
            } else if (sv == "constant_source_capacity") {
                top = stack::constant_source_capacity;
            } else if (sv == "description_capacity") {
                top = stack::description_capacity;
            } else if (sv == "file_path_capacity") {
                top = stack::file_path_capacity;
            } else if (sv == "model_capacity") {
                top = stack::model_capacity;
            } else if (sv == "parameter_capacity") {
                top = stack::parameter_capacity;
            } else if (sv == "port_capacity") {
                top = stack::port_capacity;
            } else if (sv == "random_source_capacity") {
                top = stack::random_source_capacity;
            } else if (sv == "random_generator_seed") {
                top = stack::random_generator_seed;
            } else if (sv == "text_file_source_capacity") {
                top = stack::text_file_source_capacity;
            } else if (sv == "tree_capacity") {
                top = stack::tree_capacity;
            } else {
                return false;
            }

            return true;
        }

        if (top == stack::path_object) {
            if (sv == "name") {
                read_name = true;
                return true;
            } else if (sv == "path") {
                read_path = true;
                return true;
            } else if (sv == "priority") {
                top = stack::priority;
                return true;
            } else {
                return false;
            }

            return true;
        }

        return false;
    }

    bool EndObject(rapidjson::SizeType /*memberCount*/)
    {
        if (top == stack::path_object) {
            bool            found   = false;
            registred_path* reg_dir = nullptr;
            while (app->c_editor.mod.registred_paths.next(reg_dir)) {
                if (reg_dir->path == current.path) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                auto& new_reg = app->c_editor.mod.registred_paths.alloc();
                auto  id = app->c_editor.mod.registred_paths.get_id(new_reg);
                new_reg.name     = current.name;
                new_reg.path     = current.path;
                new_reg.priority = static_cast<i8>(priority);
                new_reg.status   = registred_path::status_option::unread;

                app->c_editor.mod.component_repertories.emplace_back(id);
            }

            top = stack::paths_array;

            return true;
        }

        if (top == stack::paths) {
            top = stack::top;
            return true;
        }

        if (top == stack::top) {
            top = stack::empty;
            return true;
        }

        return false;
    }

    bool StartArray()
    {
        if (top == stack::paths) {
            top = stack::paths_array;
            return true;
        }

        return false;
    }

    bool EndArray(rapidjson::SizeType /*elementCount*/)
    {
        if (top == stack::paths_array) {
            top = stack::top;
            return true;
        }

        return false;
    }
};

status application::load_settings() noexcept
{
    status ret = status::success;

    component_setting_handler handler{ this };

    if (auto path = get_settings_filename(); path) {
        auto u8str    = path->u8string();
        auto filename = reinterpret_cast<const char*>(u8str.c_str());

        if (file f{ filename, open_mode::read }; f.is_open()) {
            auto*          fp = reinterpret_cast<FILE*>(f.get_handle());
            char           buffer[4096];
            json_in_stream is(fp, buffer, sizeof(buffer));
            json_reader    reader;

            if (!reader.Parse(is, handler)) {
                const auto error_code   = reader.GetParseErrorCode();
                const auto error_offset = reader.GetErrorOffset();

                auto  file = path->generic_string();
                auto& n    = notifications.alloc(notification_type::error);
                n.title    = "Fail to parse settings file";
                format(n.message,
                       "Error {} at offset {} in file {}",
                       rapidjson::GetParseError_En(error_code),
                       error_offset,
                       reinterpret_cast<const char*>(file.c_str()));
                notifications.enable(n);
                ret = status::io_file_format_error;
            } else {
                auto& n = notifications.alloc(notification_type::success);
                n.title = "Load settings file";
                notifications.enable(n);
            }
        } else {
            auto& n   = notifications.alloc(notification_type::error);
            n.title   = "Fail to open settings file";
            n.message = filename;
            notifications.enable(n);
            ret = status::io_file_format_error;
        }
    } else {
        auto& n = notifications.alloc(notification_type::error);
        n.title = "Fail to create settings file name";
        notifications.enable(n);
        ret = status::io_file_format_error;
    }

    return ret;
}

status application::save_settings() noexcept
{
    status ret = status::success;

    if (auto path = get_settings_filename(); path) {
        auto  u8str    = path->u8string();
        auto* filename = reinterpret_cast<const char*>(u8str.c_str());

        if (file f{ filename, open_mode::write }; f.is_open()) {
            auto*           fp = reinterpret_cast<FILE*>(f.get_handle());
            char            buffer[4096];
            json_out_stream os(fp, buffer, sizeof(buffer));
            json_writer     writer(os);

            writer.StartObject();
            writer.Key("paths");

            writer.StartArray();
            registred_path* dir = nullptr;
            while (c_editor.mod.registred_paths.next(dir)) {
                writer.StartObject();
                writer.Key("name");
                writer.String(dir->name.c_str());
                writer.Key("path");
                writer.String(dir->path.c_str());
                writer.Key("priority");
                writer.Int(dir->priority);
                writer.EndObject();
            }
            writer.EndArray();

            writer.Key("is_fixed_window_placement");
            writer.Bool(is_fixed_window_placement);
            writer.Key("model_capacity");
            writer.Int(mod_init.model_capacity);
            writer.Key("tree_capacity");
            writer.Int(mod_init.tree_capacity);
            writer.Key("description_capacity");
            writer.Int(mod_init.description_capacity);
            writer.Key("component_capacity");
            writer.Int(mod_init.component_capacity);
            writer.Key("file_path_capacity");
            writer.Int(mod_init.file_path_capacity);
            writer.Key("children_capacity");
            writer.Int(mod_init.children_capacity);
            writer.Key("connection_capacity");
            writer.Int(mod_init.connection_capacity);
            writer.Key("port_capacity");
            writer.Int(mod_init.port_capacity);
            writer.Key("parameter_capacity");
            writer.Int(mod_init.parameter_capacity);
            writer.Key("constant_source_capacity");
            writer.Int(mod_init.constant_source_capacity);
            writer.Key("binary_file_source_capacity");
            writer.Int(mod_init.binary_file_source_capacity);
            writer.Key("text_file_source_capacity");
            writer.Int(mod_init.text_file_source_capacity);
            writer.Key("random_source_capacity");
            writer.Int(mod_init.random_source_capacity);
            writer.Key("random_generator_seed");
            writer.Uint64(mod_init.random_generator_seed);

            writer.EndObject();

            auto& n = notifications.alloc(notification_type::success);
            n.title = "Save settings file";
            notifications.enable(n);
        } else {
            auto& n   = notifications.alloc(notification_type::error);
            n.title   = "Fail to open settings file";
            n.message = filename;
            notifications.enable(n);

            ret = status::io_file_format_error;
        }
    } else {
        auto& n = notifications.alloc(notification_type::error);
        n.title = "Fail to create settings file name";
        notifications.enable(n);

        ret = status::io_file_format_error;
    }

    return ret;
}

enum class stack_element_type
{
    empty,

    component_path,
    component_directory,
    component_file,

    top_object,
    top_key_parameters,
    top_key_parameters_array,

    parameter_object,
    parameter_access,
    parameter_access_array,
    parameter_model,

    parameter_X,
    parameter_dQ,
    parameter_n,
    parameter_value0,
    parameter_value1,
    parameter_value2,
    parameter_value3,
    parameter_coeff0,
    parameter_coeff1,
    parameter_coeff2,
    parameter_coeff3,
    parameter_value,
    parameter_reset,
    parameter_step_size,
    parameter_past_length,
    parameter_adapt_state,
    parameter_zero_init_offset,
    parameter_ta,
    parameter_source_ta,
    parameter_stop_on_error,
    parameter_offset,
    parameter_source_value,
    parameter_threshold,
    parameter_function,
    parameter_lower_threshold,
    parameter_upper_threshold,
    parameter_samplerate,

    parameter_source_ta_type,
    parameter_source_ta_id,
    parameter_source_value_type,
    parameter_source_value_id,

    parameter_detect_up
};

void write(json_writer& writer, bool is_ta_type, const source& src) noexcept
{
    u32 a, b;
    unpack_doubleword(src.id, &a, &b);

    if (is_ta_type) {
        writer.StartObject();
        writer.Key("source-ta-id");
        writer.Int(b);
        writer.Key("source-ta-type");
        writer.Int(src.type);
    } else {
        writer.StartObject();
        writer.Key("source-value-id");
        writer.Int(b);
        writer.Key("source-value-type");
        writer.Int(src.type);
    }
}

template<int QssLevel>
bool load(abstract_integrator<QssLevel>& dyn,
          const stack_element_type       type,
          const double                   value) noexcept
{
    if (type == stack_element_type::parameter_X) {
        dyn.default_X = static_cast<real>(value);
        return true;
    }

    if (type == stack_element_type::parameter_dQ) {
        dyn.default_dQ = static_cast<real>(value);
        return true;
    }

    return false;
}

void write(json_writer& writer, const qss1_integrator& dyn) noexcept
{
    writer.Key("qss1_integrator");

    writer.StartObject();
    writer.Key("X");
    writer.Double(dyn.default_X);
    writer.Key("dQ");
    writer.Double(dyn.default_dQ);
    writer.EndObject();
}

void write(json_writer& writer, const qss2_integrator& dyn) noexcept
{
    writer.Key("qss2_integrator");

    writer.StartObject();
    writer.Key("X");
    writer.Double(dyn.default_X);
    writer.Key("dQ");
    writer.Double(dyn.default_dQ);
    writer.EndObject();
}

void write(json_writer& writer, const qss3_integrator& dyn) noexcept
{
    writer.Key("qss3_integrator");

    writer.StartObject();
    writer.Key("X");
    writer.Double(dyn.default_X);
    writer.Key("dQ");
    writer.Double(dyn.default_dQ);
    writer.EndObject();
}

void write(json_writer& writer, const qss1_multiplier& /*dyn*/) noexcept
{
    writer.Key("qss1_multiplier");

    writer.StartObject();
    writer.EndObject();
}

void write(json_writer& writer, const qss1_sum_2& /*dyn*/) noexcept
{
    writer.Key("qss1_sum_2");

    writer.StartObject();
    writer.EndObject();
}

void write(json_writer& writer, const qss1_sum_3& /*dyn*/) noexcept
{
    writer.Key("qss1_sum_3");

    writer.StartObject();
    writer.EndObject();
}

void write(json_writer& writer, const qss1_sum_4& /*dyn*/) noexcept
{
    writer.Key("qss1_sum_4");

    writer.StartObject();
    writer.EndObject();
}

template<int QssLevel>
bool load(abstract_wsum<QssLevel, 2>& dyn,
          const stack_element_type    type,
          const double                value) noexcept
{
    if (type == stack_element_type::parameter_coeff0) {
        dyn.default_input_coeffs[0] = static_cast<real>(value);
        return true;
    }

    if (type == stack_element_type::parameter_coeff1) {
        dyn.default_input_coeffs[1] = static_cast<real>(value);
        return true;
    }

    return false;
}

template<int QssLevel>
bool load(abstract_wsum<QssLevel, 3>& dyn,
          const stack_element_type    type,
          const double                value) noexcept
{
    if (type == stack_element_type::parameter_coeff0) {
        dyn.default_input_coeffs[0] = static_cast<real>(value);
        return true;
    }

    if (type == stack_element_type::parameter_coeff1) {
        dyn.default_input_coeffs[1] = static_cast<real>(value);
        return true;
    }

    if (type == stack_element_type::parameter_coeff2) {
        dyn.default_input_coeffs[2] = static_cast<real>(value);
        return true;
    }

    return false;
}

template<int QssLevel>
bool load(abstract_wsum<QssLevel, 4>& dyn,
          const stack_element_type    type,
          const double                value) noexcept
{
    if (type == stack_element_type::parameter_coeff0) {
        dyn.default_input_coeffs[0] = static_cast<real>(value);
        return true;
    }

    if (type == stack_element_type::parameter_coeff1) {
        dyn.default_input_coeffs[1] = static_cast<real>(value);
        return true;
    }

    if (type == stack_element_type::parameter_coeff2) {
        dyn.default_input_coeffs[2] = static_cast<real>(value);
        return true;
    }

    if (type == stack_element_type::parameter_coeff3) {
        dyn.default_input_coeffs[3] = static_cast<real>(value);
        return true;
    }

    return false;
}

void write(json_writer& writer, const qss1_wsum_2& dyn) noexcept
{
    writer.Key("qss1_wsum_2");

    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.EndObject();
}

void write(json_writer& writer, const qss1_wsum_3& dyn) noexcept
{
    writer.Key("qss1_wsum_3");

    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);
    writer.EndObject();
}

void write(json_writer& writer, const qss1_wsum_4& dyn) noexcept
{
    writer.Key("qss1_wsum_4");

    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);
    writer.Key("coeff-3");
    writer.Double(dyn.default_input_coeffs[3]);
    writer.EndObject();
}

void write(json_writer& writer, const qss2_multiplier& /*dyn*/) noexcept
{
    writer.Key("qss2_multiplier");

    writer.StartObject();
    writer.EndObject();
}

void write(json_writer& writer, const qss2_sum_2& /*dyn*/) noexcept
{
    writer.Key("qss2_sum_2");

    writer.StartObject();
    writer.EndObject();
}

void write(json_writer& writer, const qss2_sum_3& /*dyn*/) noexcept
{
    writer.Key("qss2_sum_3");

    writer.StartObject();
    writer.EndObject();
}

void write(json_writer& writer, const qss2_sum_4& /*dyn*/) noexcept
{
    writer.Key("qss2_sum_4");

    writer.StartObject();
    writer.EndObject();
}

void write(json_writer& writer, const qss2_wsum_2& dyn) noexcept
{
    writer.Key("qss2_wsum_2");

    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.EndObject();
}

void write(json_writer& writer, const qss2_wsum_3& dyn) noexcept
{
    writer.Key("qss2_wsum_3");

    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);
    writer.EndObject();
}

void write(json_writer& writer, const qss2_wsum_4& dyn) noexcept
{
    writer.Key("qss2_wsum_4");

    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);
    writer.Key("coeff-3");
    writer.Double(dyn.default_input_coeffs[3]);
    writer.EndObject();
}

void write(json_writer& writer, const qss3_multiplier& /*dyn*/) noexcept
{
    writer.Key("qss3_multiplier");

    writer.StartObject();
    writer.EndObject();
}

void write(json_writer& writer, const qss3_sum_2& /*dyn*/) noexcept
{
    writer.Key("qss3_sum_2");

    writer.StartObject();
    writer.EndObject();
}

void write(json_writer& writer, const qss3_sum_3& /*dyn*/) noexcept
{
    writer.Key("qss3_sum_3");

    writer.StartObject();
    writer.EndObject();
}

void write(json_writer& writer, const qss3_sum_4& /*dyn*/) noexcept
{
    writer.Key("qss3_sum_4");

    writer.StartObject();
    writer.EndObject();
}

void write(json_writer& writer, const qss3_wsum_2& dyn) noexcept
{
    writer.Key("qss3_wsum_2");

    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.EndObject();
}

void write(json_writer& writer, const qss3_wsum_3& dyn) noexcept
{
    writer.Key("qss3_wsum_3");

    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);
    writer.EndObject();
}

void write(json_writer& writer, const qss3_wsum_4& dyn) noexcept
{
    writer.Key("qss3_wsum_4");

    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);
    writer.Key("coeff-3");
    writer.Double(dyn.default_input_coeffs[3]);
    writer.EndObject();
}

bool load(integrator&              dyn,
          const stack_element_type type,
          const double             value) noexcept
{
    if (type == stack_element_type::parameter_value) {
        dyn.default_current_value = static_cast<real>(value);
        return true;
    }

    if (type == stack_element_type::parameter_reset) {
        dyn.default_reset_value = static_cast<real>(value);
        return true;
    }

    return false;
}

void write(json_writer& writer, const integrator& dyn) noexcept
{
    writer.Key("integrator");

    writer.StartObject();
    writer.Key("value");
    writer.Double(dyn.default_current_value);
    writer.Key("reset");
    writer.Double(dyn.default_reset_value);
    writer.EndObject();
}

bool load(quantifier&              dyn,
          const stack_element_type type,
          const double             value) noexcept
{
    if (type == stack_element_type::parameter_step_size) {
        dyn.default_step_size = static_cast<real>(value);
        return true;
    }

    return false;
}

bool load(quantifier&              dyn,
          const stack_element_type type,
          const int                value) noexcept
{
    if (type == stack_element_type::parameter_past_length) {
        dyn.default_past_length = value;
        return true;
    }

    return false;
}

bool load(quantifier&              dyn,
          const stack_element_type type,
          const std::string_view   value) noexcept
{
    if (type == stack_element_type::parameter_adapt_state) {
        dyn.default_adapt_state =
          value == "possible"    ? quantifier::adapt_state::possible
          : value == "impossibe" ? quantifier::adapt_state::impossible
                                 : quantifier::adapt_state::done;
        return true;
    }

    return false;
}

bool load(quantifier&              dyn,
          const stack_element_type type,
          const bool               value) noexcept
{
    if (type == stack_element_type::parameter_zero_init_offset) {
        dyn.default_zero_init_offset = value;
        return true;
    }

    return false;
}

void write(json_writer& writer, const quantifier& dyn) noexcept
{
    writer.Key("quantifier");

    writer.StartObject();
    writer.Key("step-size");
    writer.Double(dyn.default_step_size);
    writer.Key("past-length");
    writer.Int(dyn.default_past_length);
    writer.Key("adapt-state");
    writer.String((dyn.default_adapt_state == quantifier::adapt_state::possible)
                    ? "possible "
                  : dyn.default_adapt_state ==
                      quantifier::adapt_state::impossible
                    ? "impossibe "
                    : "done ");
    writer.Key("zero-init-offset");
    writer.Bool(dyn.default_zero_init_offset);
    writer.EndObject();
}

bool load(adder_2&                 dyn,
          const stack_element_type type,
          const double             value) noexcept
{
    if (type == stack_element_type::parameter_value0) {
        dyn.default_values[0] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_value1) {
        dyn.default_values[1] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_coeff0) {
        dyn.default_input_coeffs[0] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_coeff1) {
        dyn.default_input_coeffs[1] = to_real(value);
        return true;
    }

    return false;
}

void write(json_writer& writer, const adder_2& dyn) noexcept
{
    writer.Key("adder_2");

    writer.StartObject();
    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.EndObject();
}

bool load(adder_3&                 dyn,
          const stack_element_type type,
          const double             value) noexcept
{
    if (type == stack_element_type::parameter_value0) {
        dyn.default_values[0] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_value1) {
        dyn.default_values[1] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_value2) {
        dyn.default_values[2] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_coeff0) {
        dyn.default_input_coeffs[0] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_coeff1) {
        dyn.default_input_coeffs[1] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_coeff2) {
        dyn.default_input_coeffs[2] = to_real(value);
        return true;
    }

    return false;
}

void write(json_writer& writer, const adder_3& dyn) noexcept
{
    writer.Key("adder_3");

    writer.StartObject();
    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Double(dyn.default_values[2]);
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);
    writer.EndObject();
}

bool load(adder_4&                 dyn,
          const stack_element_type type,
          const double             value) noexcept
{
    if (type == stack_element_type::parameter_value0) {
        dyn.default_values[0] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_value1) {
        dyn.default_values[1] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_value2) {
        dyn.default_values[2] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_value3) {
        dyn.default_values[3] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_coeff0) {
        dyn.default_input_coeffs[0] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_coeff1) {
        dyn.default_input_coeffs[1] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_coeff2) {
        dyn.default_input_coeffs[2] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_coeff3) {
        dyn.default_input_coeffs[3] = to_real(value);
        return true;
    }

    return false;
}

void write(json_writer& writer, const adder_4& dyn) noexcept
{
    writer.Key("adder_2");

    writer.StartObject();
    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Double(dyn.default_values[2]);
    writer.Key("value-3");
    writer.Double(dyn.default_values[3]);
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);
    writer.Key("coeff-3");
    writer.Double(dyn.default_input_coeffs[3]);
    writer.EndObject();
}

bool load(mult_2&                  dyn,
          const stack_element_type type,
          const double             value) noexcept
{
    if (type == stack_element_type::parameter_value0) {
        dyn.default_values[0] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_value1) {
        dyn.default_values[1] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_coeff0) {
        dyn.default_input_coeffs[0] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_coeff1) {
        dyn.default_input_coeffs[1] = to_real(value);
        return true;
    }

    return false;
}

void write(json_writer& writer, const mult_2& dyn) noexcept
{
    writer.Key("mult_2");

    writer.StartObject();
    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("coeff-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_values[1]);
    writer.EndObject();
}

bool load(mult_3&                  dyn,
          const stack_element_type type,
          const double             value) noexcept
{
    if (type == stack_element_type::parameter_value0) {
        dyn.default_values[0] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_value1) {
        dyn.default_values[1] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_value2) {
        dyn.default_values[2] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_coeff0) {
        dyn.default_input_coeffs[0] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_coeff1) {
        dyn.default_input_coeffs[1] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_coeff2) {
        dyn.default_input_coeffs[2] = to_real(value);
        return true;
    }

    return false;
}

void write(json_writer& writer, const mult_3& dyn) noexcept
{
    writer.Key("mult_3");

    writer.StartObject();
    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Double(dyn.default_values[2]);
    writer.Key("coeff-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_values[2]);
    writer.EndObject();
}

bool load(mult_4&                  dyn,
          const stack_element_type type,
          const double             value) noexcept
{
    if (type == stack_element_type::parameter_value0) {
        dyn.default_values[0] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_value1) {
        dyn.default_values[1] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_value2) {
        dyn.default_values[2] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_value3) {
        dyn.default_values[3] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_coeff0) {
        dyn.default_input_coeffs[0] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_coeff1) {
        dyn.default_input_coeffs[1] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_coeff2) {
        dyn.default_input_coeffs[2] = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_coeff3) {
        dyn.default_input_coeffs[3] = to_real(value);
        return true;
    }

    return false;
}

void write(json_writer& writer, const mult_4& dyn) noexcept
{
    writer.Key("mult_4");

    writer.StartObject();
    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Double(dyn.default_values[2]);
    writer.Key("value-3");
    writer.Double(dyn.default_values[3]);
    writer.Key("coeff-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_values[2]);
    writer.Key("coeff-3");
    writer.Double(dyn.default_values[3]);
    writer.EndObject();
}

void write(json_writer& writer, const counter& /*dyn*/) noexcept
{
    writer.Key("counter");

    writer.StartObject();
    writer.EndObject();
}

bool load(queue&                   dyn,
          const stack_element_type type,
          const double             value) noexcept
{
    if (type == stack_element_type::parameter_ta) {
        dyn.default_ta = to_real(value);
        return true;
    }

    return false;
}

void write(json_writer& writer, const queue& dyn) noexcept
{
    writer.Key("queue");

    writer.StartObject();
    writer.Key("ta");
    writer.Double(dyn.default_ta);
    writer.EndObject();
}

bool load(dynamic_queue&           dyn,
          const stack_element_type type,
          const int                value) noexcept
{
    if (type == stack_element_type::parameter_source_ta_type) {
        dyn.default_source_ta.type = value;
        return true;
    }

    if (type == stack_element_type::parameter_source_ta_id) {
        auto left = unpack_doubleword_right(dyn.default_source_ta.id);

        dyn.default_source_ta.id =
          make_doubleword(left, static_cast<u32>(value));

        return true;
    }

    return false;
}

bool load(dynamic_queue&           dyn,
          const stack_element_type type,
          const bool               value) noexcept
{
    if (type == stack_element_type::parameter_stop_on_error) {
        dyn.stop_on_error = value;
        return true;
    }

    return false;
}

void write(json_writer& writer, const dynamic_queue& dyn) noexcept
{
    writer.Key("dynamic_queue");

    writer.StartObject();
    write(writer, true, dyn.default_source_ta);
    writer.Key("stop-on-error");
    writer.Bool(dyn.stop_on_error);
    writer.EndObject();
}

bool load(priority_queue&          dyn,
          const stack_element_type type,
          const double             value) noexcept
{
    if (type == stack_element_type::parameter_ta) {
        dyn.default_ta = to_real(value);
        return true;
    }

    return false;
}

bool load(priority_queue&          dyn,
          const stack_element_type type,
          const int                value) noexcept
{
    if (type == stack_element_type::parameter_source_ta_type) {
        dyn.default_source_ta.type = value;
        return true;
    }

    if (type == stack_element_type::parameter_source_ta_id) {
        auto left = unpack_doubleword_right(dyn.default_source_ta.id);

        dyn.default_source_ta.id =
          make_doubleword(left, static_cast<u32>(value));

        return true;
    }

    return false;
}

bool load(priority_queue&          dyn,
          const stack_element_type type,
          const bool               value) noexcept
{
    if (type == stack_element_type::parameter_stop_on_error) {
        dyn.stop_on_error = value;
        return true;
    }

    return false;
}

void write(json_writer& writer, const priority_queue& dyn) noexcept
{
    writer.Key("priority_queue");

    writer.StartObject();
    writer.Key("ta");
    writer.Double(dyn.default_ta);
    write(writer, true, dyn.default_source_ta);
    writer.Key("stop-on-error");
    writer.Bool(dyn.stop_on_error);
    writer.EndObject();
}

bool load(generator&               dyn,
          const stack_element_type type,
          const double             value) noexcept
{
    if (type == stack_element_type::parameter_offset) {
        dyn.default_offset = to_real(value);
        return true;
    }

    return false;
}
bool load(generator&               dyn,
          const stack_element_type type,
          const int                value) noexcept
{
    if (type == stack_element_type::parameter_source_ta_type) {
        dyn.default_source_ta.type = value;
        return true;
    }

    if (type == stack_element_type::parameter_source_ta_id) {
        auto left = unpack_doubleword_right(dyn.default_source_ta.id);

        dyn.default_source_ta.id =
          make_doubleword(left, static_cast<u32>(value));

        return true;
    }

    if (type == stack_element_type::parameter_source_value_type) {
        dyn.default_source_value.type = value;
        return true;
    }

    if (type == stack_element_type::parameter_source_value_id) {
        auto left = unpack_doubleword_right(dyn.default_source_value.id);

        dyn.default_source_value.id =
          make_doubleword(left, static_cast<u32>(value));

        return true;
    }
    return false;
}

bool load(generator&               dyn,
          const stack_element_type type,
          const bool               value) noexcept
{
    if (type == stack_element_type::parameter_stop_on_error) {
        dyn.stop_on_error = value;
        return true;
    }

    return false;
}

void write(json_writer& writer, const generator& dyn) noexcept
{
    writer.Key("generator");

    writer.StartObject();
    writer.Key("offset");
    writer.Double(dyn.default_offset);
    write(writer, true, dyn.default_source_ta);
    write(writer, false, dyn.default_source_value);
    writer.Key("stop-on-error");
    writer.Bool(dyn.stop_on_error);
    writer.EndObject();
}

bool load(constant&                dyn,
          const stack_element_type type,
          const double             value) noexcept
{
    if (type == stack_element_type::parameter_value) {
        dyn.default_value = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_offset) {
        dyn.default_offset = to_real(value);
        return true;
    }

    return false;
}

void write(json_writer& writer, const constant& dyn) noexcept
{
    writer.Key("constant");

    writer.StartObject();
    writer.Key("value");
    writer.Double(dyn.default_value);
    writer.Key("offset");
    writer.Double(dyn.default_offset);
    writer.EndObject();
}

template<int QssLevel>
bool load(abstract_cross<QssLevel>& dyn,
          const stack_element_type  type,
          const double              value) noexcept
{
    if (type == stack_element_type::parameter_threshold) {
        dyn.default_threshold = to_real(value);
        return true;
    }

    return false;
}

template<int QssLevel>
bool load(abstract_cross<QssLevel>& dyn,
          const stack_element_type  type,
          const bool                value) noexcept
{
    if (type == stack_element_type::parameter_detect_up) {
        dyn.default_detect_up = value;
        return true;
    }

    return false;
}

void write(json_writer& writer, const qss1_cross& dyn) noexcept
{
    writer.Key("qss1_cross");

    writer.StartObject();
    writer.Key("threshold");
    writer.Double(dyn.default_threshold);
    writer.Key("detect-up");
    writer.Bool(dyn.default_detect_up);
    writer.EndObject();
}

void write(json_writer& writer, const qss2_cross& dyn) noexcept
{
    writer.Key("qss2_cross");

    writer.StartObject();
    writer.Key("threshold");
    writer.Double(dyn.default_threshold);
    writer.Key("detect-up");
    writer.Bool(dyn.default_detect_up);
    writer.EndObject();
}

void write(json_writer& writer, const qss3_cross& dyn) noexcept
{
    writer.Key("qss3_cross");

    writer.StartObject();
    writer.Key("threshold");
    writer.Double(dyn.default_threshold);
    writer.Key("detect-up");
    writer.Bool(dyn.default_detect_up);
    writer.EndObject();
}

template<int QssLevel>
bool load(abstract_power<QssLevel>& dyn,
          const stack_element_type  type,
          const double              value) noexcept
{
    if (type == stack_element_type::parameter_n) {
        dyn.default_n = to_real(value);
        return true;
    }

    return false;
}

void write(json_writer& writer, const qss1_power& dyn) noexcept
{
    writer.Key("qss1_power");

    writer.StartObject();
    writer.Key("n");
    writer.Double(dyn.default_n);
    writer.EndObject();
}

void write(json_writer& writer, const qss2_power& dyn) noexcept
{
    writer.Key("qss2_power");

    writer.StartObject();
    writer.Key("n");
    writer.Double(dyn.default_n);
    writer.EndObject();
}

void write(json_writer& writer, const qss3_power& dyn) noexcept
{
    writer.Key("qss3_power");

    writer.StartObject();
    writer.Key("n");
    writer.Double(dyn.default_n);
    writer.EndObject();
}

void write(json_writer& writer, const qss1_square& /*dyn*/) noexcept
{
    writer.Key("qss1_square");

    writer.StartObject();
    writer.EndObject();
}

void write(json_writer& writer, const qss2_square& /*dyn*/) noexcept
{
    writer.Key("qss2_square");

    writer.StartObject();
    writer.EndObject();
}

void write(json_writer& writer, const qss3_square& /*dyn*/) noexcept
{
    writer.Key("qss3_square");

    writer.StartObject();
    writer.EndObject();
}

bool load(cross&                   dyn,
          const stack_element_type type,
          const double             value) noexcept
{
    if (type == stack_element_type::parameter_threshold) {
        dyn.default_threshold = to_real(value);
        return true;
    }

    return false;
}

void write(json_writer& writer, const cross& dyn) noexcept
{
    writer.Key("cross");

    writer.StartObject();
    writer.Key("threshold");
    writer.Double(dyn.default_threshold);
    writer.EndObject();
}

void write(json_writer& writer, const accumulator_2& /*dyn*/) noexcept
{
    writer.Key("accumulator_2");

    writer.StartObject();
    writer.EndObject();
}

bool load(time_func&               dyn,
          const stack_element_type type,
          const std::string_view   value) noexcept
{
    if (type == stack_element_type::parameter_function) {
        if (value == "time") {
            dyn.default_f = &time_function;
            return true;
        }

        if (value == "square") {
            dyn.default_f = &square_time_function;
            return true;
        }
    }

    return false;
}

void write(json_writer& writer, const time_func& dyn) noexcept
{
    writer.Key("time_func");

    writer.StartObject();
    writer.Key("function");
    writer.String(dyn.default_f == &time_function ? "time" : "square");
    writer.EndObject();
}

bool load(filter&                  dyn,
          const stack_element_type type,
          const double             value) noexcept
{
    if (type == stack_element_type::parameter_lower_threshold) {
        dyn.default_lower_threshold = to_real(value);
        return true;
    }

    if (type == stack_element_type::parameter_upper_threshold) {
        dyn.default_upper_threshold = to_real(value);
        return true;
    }

    return false;
}

void write(json_writer& writer, const filter& dyn) noexcept
{
    writer.Key("filter");

    writer.StartObject();
    writer.Key("lower-threshold");
    writer.Double(dyn.default_lower_threshold);
    writer.Key("upper-threshold");
    writer.Double(dyn.default_upper_threshold);
    writer.EndObject();
}

bool load(hsm_wrapper& /*dyn*/,
          const stack_element_type /*type*/,
          const double /*value*/) noexcept
{
    return false;
}

void write(json_writer& writer, const hsm_wrapper& /*dyn*/) noexcept
{
    writer.Key("hsm");
}

inline bool convert_parameter_key(const std::string_view str,
                                  stack_element_type*    type) noexcept
{
    struct str_enum
    {
        constexpr str_enum(const std::string_view str_,
                           stack_element_type     type_) noexcept
          : str{ str_ }
          , type{ type_ }
        {}

        const std::string_view str;
        stack_element_type     type;
    };

    static constexpr str_enum table[] = {
        { "adapt-state", stack_element_type::parameter_adapt_state },
        { "coeff0", stack_element_type::parameter_coeff0 },
        { "coeff1", stack_element_type::parameter_coeff1 },
        { "coeff2", stack_element_type::parameter_coeff2 },
        { "coeff3", stack_element_type::parameter_coeff3 },
        { "detect-up", stack_element_type::parameter_detect_up },
        { "dQ", stack_element_type::parameter_dQ },
        { "function", stack_element_type::parameter_function },
        { "lower-threshold", stack_element_type::parameter_lower_threshold },
        { "n", stack_element_type::parameter_n },
        { "offset", stack_element_type::parameter_offset },
        { "past-length", stack_element_type::parameter_past_length },
        { "reset", stack_element_type::parameter_reset },
        { "samplerate", stack_element_type::parameter_samplerate },
        { "source-ta", stack_element_type::parameter_source_ta },
        { "source-value", stack_element_type::parameter_source_value },
        { "step-size", stack_element_type::parameter_step_size },
        { "stop-on-error", stack_element_type::parameter_stop_on_error },
        { "ta", stack_element_type::parameter_ta },
        { "threshold", stack_element_type::parameter_threshold },
        { "samplerate", stack_element_type::parameter_samplerate },
        { "source-ta-id", stack_element_type::parameter_source_ta_id },
        { "source-ta-type", stack_element_type::parameter_source_ta_type },
        { "source-value-id", stack_element_type::parameter_source_value_id },
        { "source-value-type",
          stack_element_type::parameter_source_value_type },
        { "upper-threshold", stack_element_type::parameter_upper_threshold },
        { "value", stack_element_type::parameter_value },
        { "value0", stack_element_type::parameter_value0 },
        { "value1", stack_element_type::parameter_value1 },
        { "value2", stack_element_type::parameter_value2 },
        { "value3", stack_element_type::parameter_value3 },
        { "X", stack_element_type::parameter_X },
        { "zero-init-offset", stack_element_type::parameter_zero_init_offset }
    };

    const auto it = std::lower_bound(
      std::begin(table),
      std::end(table),
      str,
      [](const str_enum& l, const std::string_view r) { return l.str < r; });

    if (it != std::end(table) && it->str == str) {
        *type = it->type;
        return true;
    }

    return false;
}

/*****************************************************************************
 *
 * Project file read part
 *
 ****************************************************************************/

template<typename T>
concept has_load_bool_function = requires(const T& t)
{
    load(t, stack_element_type{}, bool{});
};

template<typename T>
concept has_load_double_function = requires(const T& t)
{
    load(t, stack_element_type{}, double{});
};

template<typename T>
concept has_load_int_function = requires(const T& t)
{
    load(t, stack_element_type{}, int{});
};

template<typename T>
concept has_load_string_function = requires(const T& t)
{
    load(t, stack_element_type{}, std::string_view{});
};

struct project_loader_handler
{
    project_loader_handler(modeling& mod_) noexcept
      : mod{ mod_ }
    {
        stack.emplace_back(stack_element_type::empty);
    }

    bool is_top(const stack_element_type type) const noexcept
    {
        bool ret = !stack.empty();
        return ret ? stack.back() == type : false;
    }

    template<typename... Args>
    constexpr bool is_top_match(Args... args) const noexcept
    {
        const bool ret = !stack.empty();
        const auto top = stack.back();

        return ret && ((top == args) || ... || false);
    }

    bool Null() { return false; }

    bool Bool(bool b)
    {
        if (current_model) {
            const auto top = stack.back();
            stack.pop_back();

            return dispatch(*current_model,
                            [top, b]<typename Dynamics>(Dynamics& dyn) -> bool {
                                if constexpr (has_load_bool_function<Dynamics>)
                                    return load(dyn, top, b);

                                return false;
                            });
        }

        return false;
    }

    bool Double(double d)
    {
        if (current_model) {
            const auto top = stack.back();
            stack.pop_back();

            return dispatch(
              *current_model,
              [top, d]<typename Dynamics>(Dynamics& dyn) -> bool {
                  if constexpr (has_load_double_function<Dynamics>)
                      return load(dyn, top, d);

                  return false;
              });
        }

        return false;
    }

    bool Uint(unsigned /*u*/) { return false; }

    bool Int64(int64_t /*i*/) { return false; }

    bool Uint64(uint64_t /*u*/) { return false; }

    bool Int(int i)
    {
        if (is_top(stack_element_type::parameter_access_array)) {
            parent_vector.emplace_back(i);
            return true;
        }

        if (current_model) {
            const auto top = stack.back();
            stack.pop_back();

            return dispatch(*current_model,
                            [top, i]<typename Dynamics>(Dynamics& dyn) -> bool {
                                if constexpr (has_load_int_function<Dynamics>)
                                    return load(dyn, top, i);

                                return false;
                            });
        }

        return false;
    }

    bool RawNumber(const char* /*str*/,
                   rapidjson::SizeType /*length*/,
                   bool /*copy*/)
    {
        return true;
    }

    bool String(const char* str, rapidjson::SizeType length, bool /*copy*/)
    {
        std::string_view sv{ str, length };
        bool             found = false;

        if (is_top(stack_element_type::component_path)) {
            stack.pop_back();
            for (auto id : mod.component_repertories) {
                if (auto* reg = mod.registred_paths.try_to_get(id); reg) {
                    if (reg->name == sv) {
                        reg_id = id;
                        found  = true;
                        break;
                    }
                }
            }

            if (!found) {
                error_message = fmt::format("Unknown registred path `{}'", sv);
            }

            return found;
        }

        if (is_top(stack_element_type::component_directory)) {
            stack.pop_back();
            if (auto* reg = mod.registred_paths.try_to_get(reg_id); reg) {
                for (auto id : reg->children) {
                    if (auto* dir = mod.dir_paths.try_to_get(id); dir) {
                        if (dir->path == sv) {
                            dir_id = id;
                            found  = true;
                            break;
                        }
                    }
                }

                if (!found) {
                    error_message =
                      fmt::format("Unknown directory `{}' in path `{}'",
                                  sv,
                                  reg->name.sv());
                }
            } else {
                error_message = "Registed path is undefined";
                return false;
            }

            return found;
        }

        if (is_top(stack_element_type::component_file)) {
            stack.pop_back();
            if (auto* reg = mod.registred_paths.try_to_get(reg_id); reg) {
                if (auto* dir = mod.dir_paths.try_to_get(dir_id); dir) {
                    for (auto id : dir->children) {
                        if (auto* file = mod.file_paths.try_to_get(id); file) {
                            if (file->path == sv) {
                                file_id = id;
                                found   = true;
                                break;
                            }
                        }
                    }

                    if (!found) {
                        error_message = fmt::format(
                          "Unknown file `{}' in directory `{}' in path `{}'",
                          sv,
                          dir->path.sv(),
                          reg->name.sv());
                    }
                } else {
                    error_message = "Directory path is undefined";
                }
            } else {
                error_message = "Registed path is undefined";
            }

            return found;
        }

        if (current_model) {
            const auto top = stack.back();
            stack.pop_back();

            return dispatch(
              *current_model,
              [top, sv]<typename Dynamics>(Dynamics& dyn) -> bool {
                  if constexpr (has_load_string_function<Dynamics>)
                      return load(dyn, top, sv);

                  return false;
              });
        }

        return false;
    }

    bool StartObject()
    {
        if (is_top(stack_element_type::empty)) {
            stack.emplace_back(stack_element_type::top_object);
            return true;
        }

        if (is_top(stack_element_type::top_key_parameters_array)) {
            stack.emplace_back(stack_element_type::parameter_object);
            return true;
        }

        return false;
    }

    bool Key(const char* str, rapidjson::SizeType length, bool /*copy*/)
    {
        std::string_view sv{ str, length };

        if (is_top(stack_element_type::top_object)) {
            if (sv == "component-path") {
                stack.emplace_back(stack_element_type::component_path);
                return true;
            }

            if (sv == "component-directory") {
                stack.emplace_back(stack_element_type::component_directory);
                return true;
            }

            if (sv == "component-file") {
                stack.emplace_back(stack_element_type::component_file);
                return true;
            }

            if (sv == "parameters") {
                stack.emplace_back(stack_element_type::top_key_parameters);
                return true;
            }

            return false;
        }

        if (is_top(stack_element_type::parameter_object)) {
            if (sv == "access") {
                stack.emplace_back(stack_element_type::parameter_access_array);
                return true;
            }

            if (convert(sv, &current_type)) {
                stack.emplace_back(stack_element_type::parameter_model);
                current_model = nullptr;
                return true;
            }

            return false;
        }

        if (is_top(stack_element_type::parameter_model)) {
            stack_element_type key;
            if (convert_parameter_key(sv, &key)) {
                stack.emplace_back(key);
                return true;
            }
        }

        return false;
    }

    bool EndObject(rapidjson::SizeType /*memberCount*/)
    {
        if (is_top(stack_element_type::top_object)) {
            stack.pop_back();
            return true;
        }

        if (is_top(stack_element_type::parameter_object)) {
            stack.pop_back();
            return true;
        }

        if (is_top(stack_element_type::parameter_model)) {
            current_model = nullptr;
            stack.pop_back();
            return true;
        }

        return false;
    }

    bool StartArray()
    {
        if (is_top(stack_element_type::top_key_parameters)) {
            stack.emplace_back(stack_element_type::top_key_parameters_array);
            return true;
        }

        if (is_top(stack_element_type::parameter_access)) {
            parent_vector.clear();
            stack.emplace_back(stack_element_type::parameter_access_array);
            return true;
        }

        return false;
    }

    bool EndArray(rapidjson::SizeType /*elementCount*/)
    {
        if (is_top(stack_element_type::top_key_parameters_array)) {
            stack.pop_back();
            stack.pop_back();
            return true;
        }

        if (is_top(stack_element_type::parameter_access_array)) {
            auto& head       = mod.tree_nodes.get(mod.head);
            auto& compo_head = mod.components.get(head.id);
            auto* compo      = &compo_head;

            if (parent_vector.empty()) // bad access format
                return false;

            for (i32 i = 0, e = parent_vector.ssize(); i != e; ++i) {
                auto* child_id = compo->child_mapping_io.get(parent_vector[i]);
                if (!child_id)
                    return false; // bad access unknown child

                auto* child = compo->children.try_to_get(*child_id);
                if (!child)
                    return false; // bad access unknown child

                if (i + 1 < e) {
                    if (child->type == child_type::model)
                        return false; // bad access forma error

                    auto id = enum_cast<component_id>(child->id);
                    compo   = mod.components.try_to_get(id);

                    if (!compo)
                        return false; // bad access
                } else {
                    if (child->type == child_type::component)
                        return false; // leaf must be a model

                    auto  id  = enum_cast<model_id>(child->id);
                    auto* mdl = compo->models.try_to_get(id);

                    if (!mdl)
                        return false;

                    current_model = mdl;
                    break;
                }
            }

            if (current_model) {
                parent_vector.clear();
                stack.pop_back();
                return true;
            } else {
                return false;
            }
        }

        return false;
    }

    registred_path_id reg_id{ 0 };
    dir_path_id       dir_id{ 0 };
    file_path_id      file_id{ 0 };

    vector<stack_element_type> stack;
    vector<int>                parent_vector;
    std::string                error_message;
    dynamics_type              current_type  = dynamics_type::accumulator_2;
    model*                     current_model = nullptr;
    modeling&                  mod;
};

status modeling::load_project(const char* filename) noexcept
{
    irt::status            ret = status::success;
    project_loader_handler handler{ *this };

    if (file f{ filename, open_mode::read }; f.is_open()) {
        auto*          fp = reinterpret_cast<FILE*>(f.get_handle());
        char           buffer[4096];
        json_in_stream is{ fp, buffer, sizeof(buffer) };
        json_reader    reader;

        if (!reader.Parse(is, handler))
            ret = status::io_file_format_error;
        else
            state = modeling_status::unmodified;
    } else {
        ret = status::io_file_format_error;
    }

    return ret;
}

void component_editor::load_project(const char* filename) noexcept
{
    project_loader_handler handler{ mod };

    if (file f{ filename, open_mode::read }; f.is_open()) {
        auto*          fp = reinterpret_cast<FILE*>(f.get_handle());
        char           buffer[4096];
        json_in_stream is{ fp, buffer, sizeof(buffer) };
        json_reader    reader;

        if (!reader.Parse(is, handler)) {
            const auto error_code   = reader.GetParseErrorCode();
            const auto error_offset = reader.GetErrorOffset();

            auto* app = container_of(this, &application::c_editor);
            auto& n   = app->notifications.alloc(notification_type::error);
            n.title   = "Fail to parse project file";
            format(n.message,
                   "Error {} at offset {} in file {}",
                   rapidjson::GetParseError_En(error_code),
                   error_offset,
                   filename);
            app->notifications.enable(n);
        } else {
            auto* app = container_of(this, &application::c_editor);
            auto& n   = app->notifications.alloc(notification_type::success);
            n.title   = "The file was loaded successfully.";
            app->notifications.enable(n);

            mod.state = modeling_status::unmodified;
        }
    } else {
        auto* app = container_of(this, &application::c_editor);
        auto& n   = app->notifications.alloc(notification_type::error);
        n.title   = "Load project fail";
        format(n.message, "Can not access file `{}'", filename);
        app->notifications.enable(n);
    }
}

/*****************************************************************************
 *
 * project file write part
 *
 ****************************************************************************/

void write_node(json_writer& writer, modeling& mod, tree_node& parent) noexcept
{
    tree_node* tree = nullptr;
    while (mod.tree_nodes.next(tree)) {
        auto& compo = mod.components.get(tree->id);

        for (auto pair : tree->parameters.data) {
            auto* src = compo.models.try_to_get(pair.id);
            auto* dst = compo.models.try_to_get(pair.value);

            if (src && dst) {
                irt_assert(src->type == dst->type);

                writer.StartObject();

                writer.Key("access");
                writer.StartArray();

                {
                    auto* p = parent.tree.get_parent();
                    while (p) {
                        writer.Int(
                          static_cast<int>(get_index(p->id_in_parent)));
                        p = p->tree.get_parent();
                    }
                }

                writer.EndArray();

                dispatch(*dst,
                         [&writer]<typename Dynamics>(Dynamics& dyn) -> void {
                             write(writer, dyn);
                         });

                writer.EndObject();
            }
        }
    }
}

struct project_writer
{
    project_writer(FILE* fp_)
      : os(fp_, buffer, sizeof(buffer))
      , writer(os)
    {}

    char            buffer[4096];
    json_out_stream os;
    json_writer     writer;

private:
};

status modeling::save_project(const char* filename) noexcept
{
    irt::status ret = status::success;

    if (auto* parent = tree_nodes.try_to_get(head); parent) {
        if (file f{ filename, open_mode::write }; f.is_open()) {
            auto* fp = reinterpret_cast<FILE*>(f.get_handle());

            auto& compo = components.get(parent->id);
            auto* reg   = registred_paths.try_to_get(compo.reg_path);
            auto* dir   = dir_paths.try_to_get(compo.dir);
            auto* file  = file_paths.try_to_get(compo.file);

            if (!(reg && dir && file)) {
                ret = status::io_file_format_error;
            } else {
                try {
                    project_writer pw(fp);

                    pw.writer.StartObject();
                    pw.writer.Key("component-path");
                    pw.writer.String(reg->name.c_str(),
                                     static_cast<u32>(reg->name.size()),
                                     false);

                    pw.writer.Key("component-directory");
                    pw.writer.String(dir->path.begin(),
                                     static_cast<u32>(dir->path.size()),
                                     false);

                    pw.writer.Key("component-file");
                    pw.writer.String(file->path.begin(),
                                     static_cast<u32>(file->path.size()),
                                     false);

                    pw.writer.Key("parameters");
                    pw.writer.StartArray();
                    write_node(pw.writer, *this, *parent);
                    pw.writer.EndArray();
                    pw.writer.EndObject();

                    state = modeling_status::unmodified;
                } catch (...) {
                    ret = status::io_file_format_error;
                }
            }
        } else {
            ret = status::io_file_format_error;
        }
    } else {
        ret = status::io_file_format_error;
    }

    return ret;
}

void component_editor::save_project(const char* filename) noexcept
{
    if (auto* parent = mod.tree_nodes.try_to_get(mod.head); parent) {
        if (file f{ filename, open_mode::write }; f.is_open()) {
            auto* fp = reinterpret_cast<FILE*>(f.get_handle());

            auto& compo = mod.components.get(parent->id);
            auto* reg   = mod.registred_paths.try_to_get(compo.reg_path);
            auto* dir   = mod.dir_paths.try_to_get(compo.dir);
            auto* file  = mod.file_paths.try_to_get(compo.file);

            if (reg && dir && file) {
                project_writer pw(fp);
                try {
                    pw.writer.StartObject();
                    pw.writer.Key("component-path");
                    pw.writer.String(reg->name.c_str(),
                                     static_cast<u32>(reg->name.size()),
                                     false);

                    pw.writer.Key("component-directory");
                    pw.writer.String(dir->path.begin(),
                                     static_cast<u32>(dir->path.size()),
                                     false);

                    pw.writer.Key("component-file");
                    pw.writer.String(file->path.begin(),
                                     static_cast<u32>(file->path.size()),
                                     false);

                    pw.writer.Key("parameters");
                    pw.writer.StartArray();
                    write_node(pw.writer, mod, *parent);
                    pw.writer.EndArray();
                    pw.writer.EndObject();

                    mod.state = modeling_status::unmodified;

                    auto* app = container_of(this, &application::c_editor);
                    auto& n =
                      app->notifications.alloc(notification_type::success);
                    n.title = "The file was saved successfully.";
                    app->notifications.enable(n);
                } catch (...) {
                    auto* app = container_of(this, &application::c_editor);
                    auto& n =
                      app->notifications.alloc(notification_type::error);
                    n.title   = "Save project fail";
                    n.message = "Internal error in Json writer";
                    app->notifications.enable(n);
                }
            }
        } else {
            auto* app = container_of(this, &application::c_editor);
            auto& n   = app->notifications.alloc(notification_type::error);
            n.title   = "Save project fail";
            format(n.message, "Can not access file `{}'", filename);
            app->notifications.enable(n);
        }
    } else {
        auto* app = container_of(this, &application::c_editor);
        auto& n   = app->notifications.alloc(notification_type::error);
        n.title   = "Save project fail";
        format(n.message, "Can not access file `{}'", filename);
        app->notifications.enable(n);
    }
}

void load_project(void* param) noexcept
{
    auto* g_task       = reinterpret_cast<gui_task*>(param);
    g_task->state      = gui_task_status::started;
    g_task->app->state = application_status_read_only_modeling;

    auto  id   = enum_cast<registred_path_id>(g_task->param_1);
    auto* file = g_task->app->c_editor.mod.registred_paths.try_to_get(id);
    if (file) {
        g_task->app->c_editor.load_project(file->path.c_str());
        g_task->app->c_editor.mod.registred_paths.free(*file);
    }

    g_task->state = gui_task_status::finished;
}

void save_project(void* param) noexcept
{
    auto* g_task       = reinterpret_cast<gui_task*>(param);
    g_task->state      = gui_task_status::started;
    g_task->app->state = application_status_read_only_modeling;

    auto  id   = enum_cast<registred_path_id>(g_task->param_1);
    auto* file = g_task->app->c_editor.mod.registred_paths.try_to_get(id);
    if (file) {
        g_task->app->c_editor.save_project(file->path.c_str());
        g_task->app->c_editor.mod.registred_paths.free(*file);
    }

    g_task->state = gui_task_status::finished;
}

} // irt
