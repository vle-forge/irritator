// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "internal.hpp"

#include <imgui_internal.h>

namespace irt {

void HelpMarker(const char* desc) noexcept
{
    try {
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    } catch (const std::exception& /*e*/) {
    }
}

} // namespace irt

namespace ImGui {

bool CheckBoxTristate(const char* label, int* v_tristate) noexcept
{
    bool ret;
    if (*v_tristate == -1) {
        ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
        bool b = false;
        ret    = ImGui::Checkbox(label, &b);
        if (ret)
            *v_tristate = 0;
        ImGui::PopItemFlag();
    } else {
        bool b     = (*v_tristate != 0);
        bool old_b = b;

        ret = ImGui::Checkbox(label, &b);
        if (ret) {
            if (old_b == true)
                *v_tristate = -1;
            else
                *v_tristate = 1;
        }
    }
    return ret;
}

} // namespace ImGui
