// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_EDITOR_2021
#define ORG_VLEPROJECT_IRRITATOR_APP_EDITOR_2021

#include <irritator/core.hpp>
#include <irritator/modeling.hpp>

namespace irt {

class application;

/** Displays widgets to control the parameter @c p according to the dynamics
 * type @c type and the list of external source from @c modeling class.
 *
 * @return @c true if a modification of a parameter occured during the display,
 * @c false otherwise.
 */
bool show_parameter_editor(application&                app,
                           external_source_definition& srcs,
                           dynamics_type               type,
                           parameter&                  p) noexcept;

/** Displays widgets to control the parameter @c p according to the dynamics
 * type @c type and the list of external source from @c simulation class.
 *
 * @return @c true if a modification of a parameter occured during the display,
 * @c false otherwise.
 */
bool show_parameter_editor(application&     app,
                           external_source& srcs,
                           dynamics_type    type,
                           parameter&       p) noexcept;

/** Displays @c ImGui::ComboBox to select a external source from the simulation
 * class and assign @c src_type and @c src_id according to user choices. */
bool show_external_sources_combo(const char*      title,
                                 external_source& srcs,
                                 i64&             param) noexcept;

/** Displays @c ImGui::ComboBox to select a external source from the modeling
 * class and assign @c id according to use choice. */
bool show_external_sources_combo(const char*                 title,
                                 external_source_definition& srcs,
                                 i64&                        param) noexcept;

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
