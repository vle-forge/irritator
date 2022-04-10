// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"

namespace irt {

void application::show_simulation_window() noexcept
{
    if (ImGui::BeginTabBar("##Simulation")) {
        if (ImGui::BeginTabItem("Log")) {
            log_w.show();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Data")) {
            show_external_sources();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

} // namespace irt
