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

struct settings_handler
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

    application&   app;
    registred_path current;
    stack          top;

    i32  priority;
    bool read_name;
    bool read_path;

    settings_handler(application& app_) noexcept
      : app(app_)
      , top(stack::empty)
      , priority(0)
      , read_name(false)
      , read_path(false)
    {
    }

    bool Null() { return false; }

    bool Bool(bool b)
    {
        if (top == stack::is_fixed_window_placement) {
            app.mod_init.is_fixed_window_placement = b;
            top                                    = stack::top;
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
            ret = affect(app.mod_init.binary_file_source_capacity, value);
        else if (top == stack::children_capacity)
            ret = affect(app.mod_init.children_capacity, value);
        else if (top == stack::component_capacity)
            ret = affect(app.mod_init.component_capacity, value);
        else if (top == stack::connection_capacity)
            ret = affect(app.mod_init.connection_capacity, value);
        else if (top == stack::constant_source_capacity)
            ret = affect(app.mod_init.constant_source_capacity, value);
        else if (top == stack::description_capacity)
            ret = affect(app.mod_init.description_capacity, value);
        else if (top == stack::file_path_capacity)
            ret = affect(app.mod_init.file_path_capacity, value);
        else if (top == stack::model_capacity)
            ret = affect(app.mod_init.model_capacity, value);
        else if (top == stack::parameter_capacity)
            ret = affect(app.mod_init.parameter_capacity, value);
        else if (top == stack::port_capacity)
            ret = affect(app.mod_init.port_capacity, value);
        else if (top == stack::random_source_capacity)
            ret = affect(app.mod_init.random_source_capacity, value);
        else if (top == stack::random_generator_seed)
            ret = affect(app.mod_init.random_generator_seed, value);
        else if (top == stack::text_file_source_capacity)
            ret = affect(app.mod_init.text_file_source_capacity, value);
        else if (top == stack::tree_capacity)
            ret = affect(app.mod_init.tree_capacity, value);
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
            while (app.mod.registred_paths.next(reg_dir)) {
                if (reg_dir->path == current.path) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                auto& new_reg    = app.mod.registred_paths.alloc();
                auto  id         = app.mod.registred_paths.get_id(new_reg);
                new_reg.name     = current.name;
                new_reg.path     = current.path;
                new_reg.priority = static_cast<i8>(priority);
                new_reg.status   = registred_path::state::unread;

                app.mod.component_repertories.emplace_back(id);
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

struct settings_parser
{
    application& app;

    settings_parser(application& app_) noexcept
      : app(app_)
    {
    }

    bool build_notification_load_success() noexcept
    {
        auto& n = app.notifications.alloc(log_level::critical);
        n.title = "Success to read settings";

        return true;
    }

    bool build_notification_save_success() noexcept
    {
        auto& n = app.notifications.alloc(log_level::critical);
        n.title = "Success to write settings";

        return true;
    }

    bool build_notification_bad_access(const std::u8string_view str) noexcept
    {
        auto& n = app.notifications.alloc(log_level::critical);
        n.title = "Fail to access settings";
        format(n.message,
               "Fail to access settings file in {}.",
               std::string_view(reinterpret_cast<const char*>(str.data()),
                                str.size()));

        return false;
    }

    bool build_notification_bad_open(const std::u8string_view str) noexcept
    {
        auto& n = app.notifications.alloc(log_level::critical);
        n.title = "Fail to access settings";
        format(n.message,
               "Fail to open settings file `{}'.",
               std::string_view(reinterpret_cast<const char*>(str.data()),
                                str.size()));

        return false;
    }

    bool build_notification_bad_parse(const std::u8string_view  str,
                                      rapidjson::ParseErrorCode error_code,
                                      size_t error_offset) noexcept
    {
        auto& n = app.notifications.alloc(log_level::critical);
        n.title = "Fail to parse settings";
        format(n.message,
               "Fail to parse settings file `{}'. Error `{}' at offset `{}'.",
               std::string_view(reinterpret_cast<const char*>(str.data()),
                                str.size()),
               rapidjson::GetParseError_En(error_code),
               error_offset);

        return false;
    }

    bool get_utf8_string_from_path(const std::filesystem::path& p,
                                   std::u8string&               str) noexcept
    {
        try {
            str = p.u8string();
        } catch (...) {
            str.clear();
        }

        return !str.empty() ? true
                            : build_notification_bad_access(p.u8string());
    }

    bool get_settings_filename(std::filesystem::path& p) noexcept
    {
        if (auto path = irt::get_settings_filename(); path)
            p = *path;
        else
            p.clear();

        return !p.empty() ? true : build_notification_bad_access(p.u8string());
    }

    bool get_settings_filename(std::u8string& str) noexcept
    {
        std::filesystem::path p;

        return get_settings_filename(p) && get_utf8_string_from_path(p, str);
    }

    bool open_settings_file(const std::u8string& filename,
                            file&                f,
                            open_mode            mode) noexcept
    {
        f.open(reinterpret_cast<const char*>(filename.c_str()), mode);

        return f.is_open() ? true : build_notification_bad_open(filename);
    }

    bool parse_settings_file(const std::u8string&       filename,
                             rapidjson::FileReadStream& is) noexcept
    {
        settings_handler  handler(app);
        rapidjson::Reader reader;

        return reader.Parse(is, handler)
                 ? true
                 : build_notification_bad_parse(filename,
                                                reader.GetParseErrorCode(),
                                                reader.GetErrorOffset());
    }

    bool read_settings_file(const std::u8string& src, file& f) noexcept
    {
        auto* fp = reinterpret_cast<FILE*>(f.get_handle());
        char  buffer[4096];

        rapidjson::FileReadStream is(fp, buffer, sizeof(buffer));

        return parse_settings_file(src, is);
    }

    bool write_settings_file(
      rapidjson::PrettyWriter<rapidjson::FileWriteStream>& w) noexcept
    {
        w.StartObject();
        w.Key("paths");

        w.StartArray();
        registred_path* dir = nullptr;
        while (app.mod.registred_paths.next(dir)) {
            w.StartObject();
            w.Key("name");
            w.String(dir->name.c_str());
            w.Key("path");
            w.String(dir->path.c_str());
            w.Key("priority");
            w.Int(dir->priority);
            w.EndObject();
        }
        w.EndArray();

        w.Key("model_capacity");
        w.Int(app.mod_init.model_capacity);
        w.Key("tree_capacity");
        w.Int(app.mod_init.tree_capacity);
        w.Key("description_capacity");
        w.Int(app.mod_init.description_capacity);
        w.Key("component_capacity");
        w.Int(app.mod_init.component_capacity);
        w.Key("file_path_capacity");
        w.Int(app.mod_init.file_path_capacity);
        w.Key("children_capacity");
        w.Int(app.mod_init.children_capacity);
        w.Key("connection_capacity");
        w.Int(app.mod_init.connection_capacity);
        w.Key("port_capacity");
        w.Int(app.mod_init.port_capacity);
        w.Key("parameter_capacity");
        w.Int(app.mod_init.parameter_capacity);
        w.Key("constant_source_capacity");
        w.Int(app.mod_init.constant_source_capacity);
        w.Key("binary_file_source_capacity");
        w.Int(app.mod_init.binary_file_source_capacity);
        w.Key("text_file_source_capacity");
        w.Int(app.mod_init.text_file_source_capacity);
        w.Key("random_source_capacity");
        w.Int(app.mod_init.random_source_capacity);
        w.Key("random_generator_seed");
        w.Uint64(app.mod_init.random_generator_seed);

        w.EndObject();

        return true;
    }

    bool write_settings_file(const std::u8string& src, file& f) noexcept
    {
        auto* fp = reinterpret_cast<FILE*>(f.get_handle());
        char  buffer[4096];

        rapidjson::FileWriteStream os(fp, buffer, sizeof(buffer));
        rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(os);

        return write_settings_file(writer);
    }

    bool load_settings() noexcept
    {
        std::u8string filename;
        file          f;

        return get_settings_filename(filename) &&
               open_settings_file(filename, f, open_mode::read) &&
               read_settings_file(filename, f) &&
               build_notification_load_success();
    }

    bool save_settings() noexcept
    {
        std::u8string filename;
        file          f;

        return get_settings_filename(filename) &&
               open_settings_file(filename, f, open_mode::write) &&
               write_settings_file(filename, f) &&
               build_notification_save_success();
    }
};

status application::load_settings() noexcept
{
    settings_parser parser(*this);

    return parser.load_settings() ? status::success
                                  : status::io_file_format_error;
}

status application::save_settings() noexcept
{
    settings_parser parser(*this);

    return parser.save_settings() ? status::success
                                  : status::io_file_format_error;
}

} // irt
