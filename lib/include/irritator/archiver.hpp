// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_ARCHIVER_2023
#define ORG_VLEPROJECT_IRRITATOR_ARCHIVER_2023

#include <irritator/core.hpp>
#include <irritator/file.hpp>
#include <irritator/modeling.hpp>

namespace irt {

class json_dearchiver
{
private:
    vector<char> buffer;
    vector<i32>  stack;

    table<u64, u64>                            model_mapping;
    table<u64, external_source_definition::id> srcs_mapping;

    struct impl;

public:
    json_dearchiver() noexcept = default;

    status set_buffer(const u32 buffer_size) noexcept;

    //! Load a component structure from a json file.
    status operator()(const file_access& files,
                      component_access&  ids,
                      std::string_view   path,
                      const component_id compo_id,
                      component&         compo,
                      file&              io) noexcept;

    //! Load a project from a project json file.
    status operator()(project&                pj,
                      const file_access&      files,
                      const component_access& ids,
                      std::string_view        path,
                      file&                   io) noexcept;

    //! Load a component structure from a json file.
    status operator()(const file_access& files,
                      component_access&  ids,
                      const component_id compo_id,
                      component&         compo,
                      std::span<char>    io) noexcept;

    //! Load a project from a project json file.
    status operator()(project&                pj,
                      const file_access&      files,
                      const component_access& ids,
                      std::span<char>         io) noexcept;

    void destroy() noexcept;
    void clear() noexcept;
};

class json_archiver
{
private:
    vector<char> buffer;

    table<u64, u64>                            model_mapping;
    table<u64, external_source_definition::id> srcs_mapping;
    table<u64, hsm_id>                         sim_hsms_mapping;

    struct impl;

public:
    //! Control the json output stream (memory or file) pretty print.
    enum class print_option : u8 {
        off,                    //! disable pretty print.
        indent_2,               //! enable pretty print, use 2 spaces as indent.
        indent_2_one_line_array //! idem but merge simple array in one line.
    };

    //! Save a component structure into a json file.
    status operator()(const file_access&      files,
                      const component_access& ids,
                      const component_id      compo_id,
                      file&                   io,
                      print_option print_options = print_option::off) noexcept;

    //! Save a component structure into a json file.
    status operator()(const file_access&      files,
                      const component_access& ids,
                      const component_id      compo_id,
                      vector<char>&           out,
                      print_option print_options = print_option::off) noexcept;

    //! Save a project from the current modeling.
    status operator()(project&                pj,
                      const file_access&      files,
                      const component_access& ids,
                      file&                   io,
                      print_option print_options = print_option::off) noexcept;

    //! Save a project from the current modeling.
    status operator()(project&                pj,
                      const file_access&      files,
                      const component_access& ids,
                      vector<char>&           buffer,
                      print_option print_options = print_option::off) noexcept;

    void destroy() noexcept;
    void clear() noexcept;
};

} // namespace irt

#endif
