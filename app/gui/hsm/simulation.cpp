// Copyright (c) 2024 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"

namespace irt {

static void reinit(hsm_simulation_editor& hsm_ed, hsm_component_id id) noexcept
{
    hsm_ed.current_id = id;
}

bool hsm_simulation_editor::show_observations(tree_node& /*tn*/,
                                              component& /*compo*/,
                                              hsm_component& hsm) noexcept
{
    auto& ed  = container_of(this, &simulation_editor::hsm_sim);
    auto& app = container_of(&ed, &application::simulation_ed);

    const auto hsm_id = app.mod.hsm_components.get_id(hsm);
    if (hsm_id != current_id)
        reinit(*this, hsm_id);

    return false;
}

} // namespace irt
