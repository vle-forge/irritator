// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_EDITOR_2021
#define ORG_VLEPROJECT_IRRITATOR_APP_EDITOR_2021

#include <irritator/core.hpp>
#include <irritator/ext.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

namespace irt {

struct application;

bool show_parameter_editor(application&     app,
                           external_source& srcs,
                           dynamics_type    type,
                           parameter&       p) noexcept;

bool show_external_sources_combo(external_source& srcs,
                                 const char*      title,
                                 source&          src) noexcept;

bool show_external_sources_combo(external_source&     srcs,
                                 const char*          title,
                                 u64&                 src_id,
                                 source::source_type& src_type) noexcept;

/**
   Show the control to select an existing HSM component from modeling.

   @return True if the parameter @c is changed.
 */
bool show_hsm_component_id_parameter(const modeling& mod,
                                     parameter&      p) noexcept;

/**
   Get the list of input port for the specified dynamics.

   @param type
   @return An empty @c std::span if input port does not exist.
 */
auto get_dynamics_input_names(const dynamics_type type) noexcept
  -> std::span<const std::string_view>;

/**
   Get the list of input port for the specified dynamics.

   @param type
   @return An empty @c std::span if input port does not exist.
 */
auto get_dynamics_output_names(const dynamics_type type) noexcept
  -> std::span<const std::string_view>;

/**
   Get the input and output port name for the specified dynamics.

   @param [in] type
   @return A pair of input and output port names. If the dynamics does not have
   input or output port, the std::span will empty.
 */
auto get_dynamics_input_output_names(const dynamics_type type) noexcept
  -> std::pair<std::span<const std::string_view>,
               std::span<const std::string_view>>;

} // irt

#endif
