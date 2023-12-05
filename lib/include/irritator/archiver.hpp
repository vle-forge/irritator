// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_ARCHIVER_2023
#define ORG_VLEPROJECT_IRRITATOR_ARCHIVER_2023

#include <irritator/core.hpp>
#include <irritator/file.hpp>
#include <irritator/modeling.hpp>

namespace irt {

class json_archiver
{
public:
    enum class part {
        read_file_error,
        json_format_error, //<! Add a e_json with offset and error code.
        simulation_parser,
        component_parser,
        project_parser,
        settings_parser
    };

    //! Control the json output stream (memory or file) pretty print.
    enum class print_option {
        off,                    //! disable pretty print.
        indent_2,               //! enable pretty print, use 2 spaces as indent.
        indent_2_one_line_array //! idem but merge simple array in one line.
    };

    //! Load a simulation structure from a json file.
    status simulation_load(simulation& sim, cache_rw& cache, file& io) noexcept;

    //! Load a simulation structure from a json memory buffer. This function is
    //! mainly used in unit-test to check i/o functions.
    status simulation_load(simulation&     sim,
                           cache_rw&       cache,
                           std::span<char> in) noexcept;

    //! Save a component structure into a json memory buffer. This function is
    //! mainly used in unit-test to check i/o functions.
    status simulation_save(
      const simulation& sim,
      cache_rw&         cache,
      vector<char>&     out,
      print_option      print_options = print_option::off) noexcept;

    //! Save a component structure into a json file.
    status simulation_save(
      const simulation& sim,
      cache_rw&         cache,
      const char*       filename,
      print_option      print_options = print_option::off) noexcept;

    //! Load a component structure from a json file.
    status component_load(modeling&  mod,
                          component& compo,
                          cache_rw&  cache,
                          file&      io) noexcept;

    //! Load a component structure from a json file.
    status component_load(modeling&       mod,
                          component&      compo,
                          cache_rw&       cache,
                          std::span<char> buffer) noexcept;

    //! Save a component structure into a json file.
    status component_save(
      modeling&    mod,
      component&   compo,
      cache_rw&    cache,
      const char*  filename,
      print_option print_options = print_option::off) noexcept;

    //! Save a component structure into a json file.
    status component_save(
      modeling&     mod,
      component&    compo,
      cache_rw&     cache,
      vector<char>& out,
      print_option  print_options = print_option::off) noexcept;

    //! Load a project from a project json file.
    status project_load(project&    pj,
                        modeling&   mod,
                        simulation& sim,
                        cache_rw&   cache,
                        file&       io) noexcept;

    //! Load a project from a project json file.
    status project_load(project&        pj,
                        modeling&       mod,
                        simulation&     sim,
                        cache_rw&       cache,
                        std::span<char> buffer) noexcept;

    //! Save a project from the current modeling.
    status project_save(
      project&     pj,
      modeling&    mod,
      simulation&  sim,
      cache_rw&    cache,
      file&        f,
      print_option print_options = print_option::off) noexcept;

    //! Save a project from the current modeling.
    status project_save(
      project&      pj,
      modeling&     mod,
      simulation&   sim,
      cache_rw&     cache,
      vector<char>& buffer,
      print_option  print_options = print_option::off) noexcept;

private:
};

class binary_archiver
{
public:
    struct not_enough_memory {};
    struct open_file_error {};
    struct write_file_error {};
    struct file_format_error {};
    struct unknown_model_error {};
    struct unknown_model_port_error {};

    status simulation_save(simulation& sim, file& io) noexcept;

    status simulation_save(simulation& sim, memory& io) noexcept;

    status simulation_load(simulation& sim, file& io) noexcept;

    status simulation_load(simulation& sim, memory& io) noexcept;

    //! @TODO Merge the binary_archiver::cache system with the modeling
    //! cache_rw.
    struct cache {
        table<u32, model_id>              to_models;
        table<u32, hsm_id>                to_hsms;
        table<u32, constant_source_id>    to_constant;
        table<u32, binary_file_source_id> to_binary;
        table<u32, text_file_source_id>   to_text;
        table<u32, random_source_id>      to_random;

        void clear() noexcept;
    };

private:
    cache c;
};

} // namespace irt

#endif
