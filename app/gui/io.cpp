// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"

#include <fstream>

#include <fmt/format.h>

#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/reader.h>
#include <rapidjson/writer.h>

namespace irt {

struct component_setting_handler
{
    enum class stack
    {
        empty,
        top,
        paths,
        paths_array,
        path_object,
    };

    application* app;
    dir_path     current;
    stack        top;

    i32  priority;
    i32  i32_empty_value;
    u64  u64_empty_value;
    i32* i32_value;
    u64* u64_value;

    component_setting_handler(application* app_) noexcept
      : app(app_)
      , top(stack::empty)
      , i32_empty_value(0)
      , u64_empty_value(0)
      , i32_value(&i32_empty_value)
      , u64_value(&u64_empty_value)
    {}

    bool Null() { return false; }
    bool Bool(bool /*b*/) { return false; }
    bool Double(double /*d*/) { return false; }

    bool Uint(unsigned u)
    {
        *i32_value = static_cast<i32>(u);
        return true;
    }

    bool Int64(int64_t i)
    {
        *u64_value = static_cast<u64>(i);
        return true;
    }

    bool Uint64(uint64_t u)
    {
        *u64_value = u;
        return true;
    }

    bool Int(int i)
    {
        *i32_value = i;
        return true;
    }

    bool RawNumber(const char* /*str*/,
                   rapidjson::SizeType /*length*/,
                   bool /*copy*/)
    {
        return false;
    }

    bool String(const char* str, rapidjson::SizeType /*length*/, bool /*copy*/)
    {
        current.path.assign(str);

        return true;
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
                return true;
            } else if (sv == "binary_file_source_capacity") {
                i32_value = &app->mod_init.binary_file_source_capacity;
            } else if (sv == "children_capacity") {
                i32_value = &app->mod_init.children_capacity;
            } else if (sv == "component_capacity") {
                i32_value = &app->mod_init.component_capacity;
            } else if (sv == "connection_capacity") {
                i32_value = &app->mod_init.connection_capacity;
            } else if (sv == "constant_source_capacity") {
                i32_value = &app->mod_init.constant_source_capacity;
            } else if (sv == "description_capacity") {
                i32_value = &app->mod_init.description_capacity;
            } else if (sv == "file_path_capacity") {
                i32_value = &app->mod_init.file_path_capacity;
            } else if (sv == "model_capacity") {
                i32_value = &app->mod_init.model_capacity;
            } else if (sv == "observer_capacity") {
                i32_value = &app->mod_init.observer_capacity;
            } else if (sv == "port_capacity") {
                i32_value = &app->mod_init.port_capacity;
            } else if (sv == "random_source_capacity") {
                i32_value = &app->mod_init.random_source_capacity;
            } else if (sv == "random_generator_seed") {
                u64_value = &app->mod_init.random_generator_seed;
            } else if (sv == "text_file_source_capacity") {
                i32_value = &app->mod_init.text_file_source_capacity;
            } else if (sv == "tree_capacity") {
                i32_value = &app->mod_init.tree_capacity;
            } else {
                return false;
            }

            return true;
        }

        if (top == stack::path_object) {
            if (sv == "path") {
                return true;
            } else if (std::strcmp(str, "priority") == 0) {
                i32_value = &priority;
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
            bool      found = false;
            dir_path* dir   = nullptr;
            while (app->c_editor.mod.dir_paths.next(dir)) {
                if (dir->path == current.path) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                auto& new_dir    = app->c_editor.mod.dir_paths.alloc();
                new_dir.path     = current.path;
                new_dir.priority = static_cast<i8>(priority);
                top              = stack::paths_array;
            }
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

        if (top == stack::top) {
            top = stack::empty;
            return true;
        }

        return false;
    }
};

status application::load_settings() noexcept
{
    component_setting_handler handler{ this };

    FILE* fp = std::fopen("settings.json", "r");
    if (!fp)
        return status::io_file_format_error;

    char                      buffer[4096];
    rapidjson::FileReadStream is(fp, buffer, sizeof(buffer));
    rapidjson::Reader         reader;

    if (!reader.Parse(is, handler))
        return status::io_file_format_error;

    return status::success;
}

status application::save_settings() noexcept
{
    FILE* fp = std::fopen("settings.json", "w");
    if (!fp)
        return status::io_file_format_error;

    char                       buffer[4096];
    rapidjson::FileWriteStream os(fp, buffer, sizeof(buffer));
    rapidjson::Writer          writer(os);

    writer.StartObject();
    writer.Key("paths");

    writer.StartArray();
    dir_path* dir = nullptr;
    while (c_editor.mod.dir_paths.next(dir)) {
        writer.StartObject();
        writer.Key("path");
        writer.String(dir->path.c_str());
        writer.Key("priority");
        writer.Int(dir->priority);
        writer.EndObject();
    }
    writer.EndArray();

    writer.Key("model_capacity");
    writer.Int(mod_init.model_capacity);
    writer.Key("tree_capacity");
    writer.Int(mod_init.tree_capacity);
    writer.Key("description_capacity");
    writer.Int(mod_init.description_capacity);
    writer.Key("component_capacity");
    writer.Int(mod_init.component_capacity);
    writer.Key("observer_capacity");
    writer.Int(mod_init.observer_capacity);
    writer.Key("file_path_capacity");
    writer.Int(mod_init.file_path_capacity);
    writer.Key("children_capacity");
    writer.Int(mod_init.children_capacity);
    writer.Key("connection_capacity");
    writer.Int(mod_init.connection_capacity);
    writer.Key("port_capacity");
    writer.Int(mod_init.port_capacity);
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

    std::fclose(fp);

    return status::success;
}

} // irt