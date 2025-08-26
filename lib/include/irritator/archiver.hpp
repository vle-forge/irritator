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

    table<u64, u64>                   model_mapping;
    table<u64, constant_source_id>    constant_mapping;
    table<u64, binary_file_source_id> binary_file_mapping;
    table<u64, random_source_id>      random_mapping;
    table<u64, text_file_source_id>   text_file_mapping;

    struct impl;

public:
    json_dearchiver() noexcept = default;

    status set_buffer(const u32 buffer_size) noexcept;

    //! Load a component structure from a json file.
    status operator()(modeling&        mod,
                      component&       compo,
                      std::string_view path,
                      file&            io) noexcept;

    //! Load a project from a project json file.
    status operator()(project&         pj,
                      modeling&        mod,
                      simulation&      sim,
                      std::string_view path,
                      file&            io) noexcept;

    //! Load a component structure from a json file.
    status operator()(modeling&       mod,
                      component&      compo,
                      std::span<char> io) noexcept;

    //! Load a project from a project json file.
    status operator()(project&        pj,
                      modeling&       mod,
                      simulation&     sim,
                      std::span<char> io) noexcept;

    void destroy() noexcept;
    void clear() noexcept;
};

class json_archiver
{
private:
    vector<char> buffer;

    table<u64, u64>                   model_mapping;
    table<u64, constant_source_id>    constant_mapping;
    table<u64, binary_file_source_id> binary_file_mapping;
    table<u64, random_source_id>      random_mapping;
    table<u64, text_file_source_id>   text_file_mapping;
    table<u64, hsm_id>                sim_hsms_mapping;

    struct impl;

public:
    //! Control the json output stream (memory or file) pretty print.
    enum class print_option {
        off,                    //! disable pretty print.
        indent_2,               //! enable pretty print, use 2 spaces as indent.
        indent_2_one_line_array //! idem but merge simple array in one line.
    };

    //! Save a component structure into a json file.
    status operator()(modeling&    mod,
                      component&   compo,
                      file&        io,
                      print_option print_options = print_option::off) noexcept;

    //! Save a component structure into a json file.
    status operator()(modeling&     mod,
                      component&    compo,
                      vector<char>& out,
                      print_option  print_options = print_option::off) noexcept;

    //! Save a project from the current modeling.
    status operator()(project&     pj,
                      modeling&    mod,
                      file&        io,
                      print_option print_options = print_option::off) noexcept;

    //! Save a project from the current modeling.
    status operator()(project&      pj,
                      modeling&     mod,
                      vector<char>& buffer,
                      print_option  print_options = print_option::off) noexcept;

    void destroy() noexcept;
    void clear() noexcept;
};

class binary_archiver
{
public:
    enum class error_code {
        not_enough_memory = 1,
        write_error,
        read_error,
        format_error,
        header_error,
        unknown_model_error,
        unknown_model_port_error,
    };

    bool simulation_save(simulation& sim, file& io) noexcept;
    bool simulation_save(simulation& sim, memory& io) noexcept;
    bool simulation_load(simulation& sim, file& io) noexcept;
    bool simulation_load(simulation& sim, memory& io) noexcept;

    void clear_cache() noexcept;

    error_code ec; //!< If main functions returns false, @c ec variable stores
                   //!< the error code.

private:
    struct impl; //!< Get access to observation attributes without complex code.

    bool report_error(error_code ec) noexcept;
    bool report_error(error_code ec) const noexcept;

    table<u32, model_id>              to_models;
    table<u32, constant_source_id>    to_constant;
    table<u32, binary_file_source_id> to_binary;
    table<u32, text_file_source_id>   to_text;
    table<u32, random_source_id>      to_random;
};

} // namespace irt

#endif
