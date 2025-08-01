// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_EDITOR_2021
#define ORG_VLEPROJECT_IRRITATOR_APP_EDITOR_2021

#include <irritator/core.hpp>
#include <irritator/modeling.hpp>

namespace irt {

class application;

/**
 * Displays widgets to control the parameter @c p according to the dynamics
 * type @c type.
 *
 * @return @c true if a modification of a parameter occured during the display,
 * @c false otherwise.
 */
bool show_parameter_editor(application&     app,
                           external_source& srcs,
                           dynamics_type    type,
                           parameter&       p) noexcept;

bool show_external_sources_combo(external_source& srcs,
                                 const char*      title,
                                 source&          src) noexcept;

bool show_external_sources_combo(external_source&     srcs,
                                 const char*          title,
                                 source::id_type&     src_id,
                                 source::source_type& src_type) noexcept;

/**
 * Display widgets to control the HSM component from modeling.
 *
 * @param app Get access to @c modeling and @c component_selector.
 * @param p Access to @c parameter (@c p must be a parameter of an @c
 * hsm_rapper).
 *
 * @return True if the parameter @c is changed.
 */
bool show_extented_hsm_parameter(const application& app, parameter& p) noexcept;

/**
 * Display widgets to control the constant model incoming/outcoming port.
 *
 * The parameter @c p must be a parameter of an @c hsm_rapper.
 *
 * @return True if the parameter @c is changed.
 */
bool show_extented_constant_parameter(const modeling&    mod,
                                      const component_id id,
                                      parameter&         p) noexcept;

} // irt

#endif
