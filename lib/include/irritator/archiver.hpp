// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_ARCHIVER_2023
#define ORG_VLEPROJECT_IRRITATOR_ARCHIVER_2023

#include <irritator/core.hpp>
#include <irritator/file.hpp>
#include <irritator/modeling.hpp>

#include <span>
#include <string_view>

namespace irt {

enum class json_pretty_print : u8 {
    off,
    indent_2,
    /// Like @c indent_2 with the simple arrays on one line.
    indent_2_one_line_array
};

/**
 * Reads the component @c compo_id from a json @c .irt file.
 *
 * On error, the component is left @c unreadable if the json is malformed and
 * @c pending otherwise, for example when it depends on a component not read
 * yet.
 *
 * @param path Only used in the log messages.
 */
status load_component(const file_access& files,
                      component_access&  ids,
                      component_id       compo_id,
                      file&              io,
                      std::string_view   path = {}) noexcept;

status load_component(const file_access&    files,
                      component_access&     ids,
                      component_id          compo_id,
                      std::span<const char> json) noexcept;

/**
 * Reads a project from a json @c .pirt file. The project is cleared first.
 *
 * @param path Only used in the log messages.
 */
status load_project(project&                pj,
                    const file_access&      files,
                    const component_access& ids,
                    file&                   io,
                    std::string_view        path = {}) noexcept;

status load_project(project&                pj,
                    const file_access&      files,
                    const component_access& ids,
                    std::span<const char>   json) noexcept;

status save_component(
  const file_access&      files,
  const component_access& ids,
  component_id            compo_id,
  file&                   io,
  json_pretty_print       print = json_pretty_print::off) noexcept;

expected<vector<char>> save_component(
  const file_access&      files,
  const component_access& ids,
  component_id            compo_id,
  json_pretty_print       print = json_pretty_print::off) noexcept;

status save_project(const project&          pj,
                    const file_access&      files,
                    const component_access& ids,
                    file&                   io,
                    json_pretty_print print = json_pretty_print::off) noexcept;

expected<vector<char>> save_project(
  const project&          pj,
  const file_access&      files,
  const component_access& ids,
  json_pretty_print       print = json_pretty_print::off) noexcept;

/**
 * Writes the journal of the commands applied to the simulation of @c pj
 * (see @c command_journal) in json. A model with a unique identifier (a
 * model "configurable" or "observable") is named by its tree node and its
 * identifier, the others by their identifier in the simulation: a journal is
 * replayed by a simulation built from the same project.
 */
status save_journal(const project&         pj,
                    const command_journal& journal,
                    file&                  io,
                    json_pretty_print print = json_pretty_print::off) noexcept;

expected<vector<char>> save_journal(
  const project&         pj,
  const command_journal& journal,
  json_pretty_print      print = json_pretty_print::off) noexcept;

/**
 * Reads a journal written by @c save_journal() into @c journal, cleared
 * first. The names are resolved in @c pj, whose simulation is built (@c
 * project::simulation_init()): replay it with @c simulation_player::start().
 *
 * @return @c project_errc::journal_format_error if the json is not a
 * journal, @c project_errc::journal_model_error if a name is unknown or if a
 * constant is not a constant.
 */
status load_journal(const project&   pj,
                    command_journal& journal,
                    file&            io) noexcept;

status load_journal(const project&        pj,
                    command_journal&      journal,
                    std::span<const char> json) noexcept;

} // namespace irt

#endif
