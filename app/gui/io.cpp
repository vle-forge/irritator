// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "internal.hpp"

#include <irritator/file.hpp>
#include <irritator/io.hpp>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/reader.h>
#include <rapidjson/writer.h>

namespace irt {

// @TODO rename component_setting_handler into settings_handler
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
    {
    }

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
                new_reg.status   = registred_path::state::unread;

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
            auto* fp = reinterpret_cast<FILE*>(f.get_handle());
            char  buffer[4096];
            rapidjson::FileReadStream is(fp, buffer, sizeof(buffer));
            rapidjson::Reader         reader;

            if (!reader.Parse(is, handler)) {
                const auto error_code   = reader.GetParseErrorCode();
                const auto error_offset = reader.GetErrorOffset();

                auto  file = path->generic_string();
                auto& n    = notifications.alloc(log_level::error);
                n.title    = "Fail to parse settings file";
                format(n.message,
                       "Error {} at offset {} in file {}",
                       rapidjson::GetParseError_En(error_code),
                       error_offset,
                       reinterpret_cast<const char*>(file.c_str()));
                notifications.enable(n);
                ret = status::io_file_format_error;
            } else {
                auto& n = notifications.alloc(log_level::notice);
                n.title = "Load settings file";
                notifications.enable(n);
            }
        } else {
            auto& n   = notifications.alloc(log_level::error);
            n.title   = "Fail to open settings file";
            n.message = filename;
            notifications.enable(n);
            ret = status::io_file_format_error;
        }
    } else {
        auto& n = notifications.alloc(log_level::error);
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
            auto* fp = reinterpret_cast<FILE*>(f.get_handle());
            char  buffer[4096];
            rapidjson::FileWriteStream os(fp, buffer, sizeof(buffer));
            rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(os);

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

            auto& n = notifications.alloc(log_level::notice);
            n.title = "Save settings file";
            notifications.enable(n);
        } else {
            auto& n   = notifications.alloc(log_level::error);
            n.title   = "Fail to open settings file";
            n.message = filename;
            notifications.enable(n);

            ret = status::io_file_format_error;
        }
    } else {
        auto& n = notifications.alloc(log_level::critical);
        n.title = "Fail to create settings file name";
        notifications.enable(n);

        ret = status::io_file_format_error;
    }

    return ret;
}

// Gui

void component_editor::save_project(const char* filename) noexcept
{
    json_cache cache;

    if (auto ret = project_save(mod, cache, filename); is_bad(ret)) {
        auto* app = container_of(this, &application::c_editor);
        auto& n   = app->notifications.alloc(log_level::error);
        n.title   = "Save project fail";
        format(n.message, "Can not access file `{}'", filename);
        app->notifications.enable(n);
    } else {
        mod.state = modeling_status::unmodified;
        auto* app = container_of(this, &application::c_editor);
        auto& n   = app->notifications.alloc(log_level::notice);
        n.title   = "The file was saved successfully.";
        app->notifications.enable(n);
    }
}

void component_editor::load_project(const char* filename) noexcept
{
    json_cache cache;

    if (auto ret = project_load(mod, cache, filename); is_bad(ret)) {
        auto* app = container_of(this, &application::c_editor);
        auto& n   = app->notifications.alloc(log_level::error);
        n.title   = "Load project fail";
        format(n.message, "Can not access file `{}'", filename);
        app->notifications.enable(n);
    } else {
        auto* app = container_of(this, &application::c_editor);
        auto& n   = app->notifications.alloc(log_level::notice);
        n.title   = "The file was loaded successfully.";
        app->notifications.enable(n);
        mod.state = modeling_status::unmodified;
    }
}

// Tasks to load/save project file.

void task_load_project(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<simulation_task*>(param);
    g_task->state = task_status::started;

    auto  id   = enum_cast<registred_path_id>(g_task->param_1);
    auto* file = g_task->app->c_editor.mod.registred_paths.try_to_get(id);
    if (file) {
        g_task->app->c_editor.load_project(file->path.c_str());
        g_task->app->c_editor.mod.registred_paths.free(*file);
    }

    g_task->state = task_status::finished;
}

void task_save_project(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<simulation_task*>(param);
    g_task->state = task_status::started;

    auto  id   = enum_cast<registred_path_id>(g_task->param_1);
    auto* file = g_task->app->c_editor.mod.registred_paths.try_to_get(id);
    if (file) {
        g_task->app->c_editor.save_project(file->path.c_str());
        g_task->app->c_editor.mod.registred_paths.free(*file);
    }

    g_task->state = task_status::finished;
}

} // irt
