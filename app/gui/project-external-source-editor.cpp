// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/io.hpp>

#include "application.hpp"
#include "dialog.hpp"
#include "internal.hpp"

namespace irt {

template<typename DataArray>
static status try_allocate_external_source(application&    app,
                                           project_editor& pj,
                                           source_type     type,
                                           DataArray&      d) noexcept
{
    if (not d.can_alloc(1)) {
        if (not d.template grow<3, 2>())
            return new_error(external_source_errc::memory_error);
    }

    [[maybe_unused]] auto& src = d.alloc();

    using underlying_type = typename DataArray::value_type;

    if constexpr (std::is_same_v<underlying_type, constant_source> or
                  std::is_same_v<underlying_type, random_source>)
        app.start_init_source(app.pjs.get_id(pj), ordinal(d.get_id(src)), type);

    return success();
}

static status display_allocate_external_source(application&    app,
                                               project_editor& ed,
                                               source_type     part) noexcept
{
    switch (part) {
    case source_type::constant:
        return try_allocate_external_source(
          app, ed, part, ed.pj.sim.srcs.constant_sources);

    case source_type::binary_file:
        return try_allocate_external_source(
          app, ed, part, ed.pj.sim.srcs.binary_file_sources);

    case source_type::text_file:
        return try_allocate_external_source(
          app, ed, part, ed.pj.sim.srcs.text_file_sources);

    case source_type::random:
        return try_allocate_external_source(
          app, ed, part, ed.pj.sim.srcs.random_sources);
    }

    irt::unreachable();
}

static void display_allocate_external_source(application&    app,
                                             project_editor& ed) noexcept
{
    const auto& style = ImGui::GetStyle();
    const auto  width =
      (ImGui::GetContentRegionAvail().x - 4.f * style.ItemSpacing.x) / 4.f;
    auto button_sz = ImVec2(width, 20);

    std::optional<source_type> part;

    if (ImGui::Button("+constant", button_sz))
        part = source_type::constant;
    ImGui::SameLine();
    if (ImGui::Button("+text file", button_sz))
        part = source_type::text_file;
    ImGui::SameLine();
    if (ImGui::Button("+binary file", button_sz))
        part = source_type::binary_file;
    ImGui::SameLine();
    if (ImGui::Button("+random", button_sz))
        part = source_type::random;

    if (part.has_value()) {
        auto ret = display_allocate_external_source(app, ed, *part);
        if (not ret.has_value()) {
            switch (ret.error().cat()) {
            case category::external_source:
                app.jn.push(log_level::error, [&](auto& title, auto&) {
                    format(title,
                           "Fail to initialize {} source",
                           external_source_str(*part));
                    // TODO More.
                });
                break;

            default:
                app.jn.push(log_level::error, [&](auto& title, auto&) {
                    format(title,
                           "Fail to initialize {} source",
                           external_source_str(*part));
                });
            }
        }
    }
}

auto show_data_file_input(const modeling&    mod,
                          const dir_path_id  dir_id,
                          const file_path_id file_id) noexcept -> file_path_id
{
    auto ret = undefined<file_path_id>();

    if (auto* dir = mod.dir_paths.try_to_get(dir_id); dir) {
        const auto preview = [](file_path* f) noexcept -> const char* {
            return f ? f->path.c_str() : "-";
        }(mod.file_paths.try_to_get(file_id));
        ret = file_id;

        if (ImGui::BeginCombo("Select file", preview)) {
            for (const auto f_id : dir->children) {
                if (const auto* file = mod.file_paths.try_to_get(f_id); file) {
                    const auto str = file->path.sv();
                    const auto dot = str.find_last_of(".");
                    const auto ext = str.substr(dot);

                    if (not(ext == ".irt" or ext == ".txt")) {
                        if (ImGui::Selectable(file->path.c_str(),
                                              file_id == f_id)) {
                            ret = f_id;
                        }
                    }
                }
            }
            ImGui::EndCombo();
        }
    } else {
        ImGui::TextFormatDisabled("This component is not saved");
    }

    return ret;
}

bool show_random_distribution_input(
  external_source_definition::random_source& src) noexcept
{
    int up           = 0;
    int current_item = ordinal(src.type);
    int old_current  = ordinal(src.type);

    if (ImGui::Combo("Distribution",
                     &current_item,
                     irt::distribution_type_string,
                     IM_ARRAYSIZE(irt::distribution_type_string))) {
        if (current_item != old_current) {
            src.type = enum_cast<distribution_type>(current_item);
            src.ints.fill(0);
            src.reals.fill(0.0);
            up++;
        }
    }

    switch (src.type) {
    case distribution_type::uniform_int: {
        if (old_current != current_item) {
            src.ints[0] = 0;
            src.ints[1] = 100;
        }

        auto a = src.ints[0];
        auto b = src.ints[1];

        if (ImGui::InputInt("a", &a)) {
            up++;

            if (a < b)
                src.ints[0] = a;
            else
                src.ints[1] = a + 1;
        }

        if (ImGui::InputInt("b", &b)) {
            up++;

            if (a < b)
                src.ints[1] = b;
            else
                src.ints[0] = b - 1;
        }
    } break;

    case distribution_type::uniform_real: {
        if (old_current != current_item) {
            src.reals[0] = 0.0;
            src.reals[1] = 1.0;
        }

        auto a = src.reals[0];
        auto b = src.reals[1];
        if (ImGui::InputDouble("a", &a)) {
            ++up;
            if (a < b)
                src.reals[0] = a;
            else
                src.reals[1] = a + 1;
        }

        if (ImGui::InputDouble("b", &b)) {
            ++up;
            if (a < b)
                src.reals[0] = a;
            else
                src.reals[1] = a + 1;
        }
    } break;

    case distribution_type::bernouilli:
        if (old_current != current_item)
            src.reals[0] = 0.5;

        if (auto p = src.reals[0];
            ImGui::InputDouble("p", &p) and 0.0 <= p and p <= 1.0) {
            src.reals[0] = p;
            ++up;
        }
        break;

    case distribution_type::binomial:
        if (old_current != current_item) {
            src.reals[0] = 0.5;
            src.ints[0]  = 1;
        }

        if (auto p = src.reals[0];
            ImGui::InputDouble("p", &p) and 0.0 <= p and p <= 1.0) {
            src.reals[0] = p;
            ++up;
        }

        if (auto t = src.ints[0]; ImGui::InputInt("t", &t) and t >= 0) {
            src.ints[0] = t;
            ++up;
        }
        break;

    case distribution_type::negative_binomial:
        if (old_current != current_item) {
            src.reals[0] = 0.5;
            src.ints[0]  = 1;
        }

        if (auto p = src.reals[0];
            ImGui::InputDouble("p", &p) and 0.0 < p and p <= 1.0) {
            src.reals[0] = p;
            ++up;
        }

        if (auto t = src.ints[0]; ImGui::InputInt("t", &t) and 0 < t) {
            src.ints[0] = t;
            ++up;
        }
        break;

    case distribution_type::geometric:
        if (old_current != current_item)
            src.reals[0] = 0.5;

        if (auto p = src.reals[0];
            ImGui::InputDouble("p", &p) and 0.0 <= p and p < 1.0) {
            src.reals[0] = p;
            ++up;
        }
        break;

    case distribution_type::poisson:
        if (old_current != current_item)
            src.reals[0] = 0.5;

        if (auto m = src.reals[0]; ImGui::InputDouble("mean", &m) and 0.0 < m) {
            src.reals[0] = m;
            ++up;
        }
        break;

    case distribution_type::exponential:
        if (old_current != current_item)
            src.reals[0] = 1.0;

        if (auto l = src.reals[0];
            ImGui::InputDouble("lambda", &l) and 0.0 < l) {
            src.reals[0] = l;
            ++up;
        }
        break;

    case distribution_type::gamma:
        if (old_current != current_item) {
            src.reals[0] = 1.0;
            src.reals[1] = 1.0;
        }

        if (auto a = src.reals[0];
            ImGui::InputDouble("alpha", &a) and 0.0 < a) {
            src.reals[0] = a;
            ++up;
        }

        if (auto b = src.reals[1]; ImGui::InputDouble("beta", &b) and 0.0 < b) {
            src.reals[1] = b;
            ++up;
        }
        break;

    case distribution_type::weibull:
        if (old_current != current_item) {
            src.reals[0] = 1.0;
            src.reals[1] = 1.0;
        }

        if (auto a = src.reals[0]; ImGui::InputDouble("a", &a) and 0.0 < a) {
            src.reals[0] = a;
            ++up;
        }

        if (auto b = src.reals[1]; ImGui::InputDouble("b", &b) and 0.0 < b) {
            src.reals[1] = b;
            ++up;
        }
        break;

    case distribution_type::exterme_value:
        if (old_current != current_item) {
            src.reals[0] = 1.0;
            src.reals[1] = 0.0;
        }

        if (auto a = src.reals[0]; ImGui::InputDouble("a", &a)) {
            src.reals[0] = a;
            ++up;
        }

        if (auto b = src.reals[1]; ImGui::InputDouble("b", &b) and 0.0 < b) {
            src.reals[1] = b;
            ++up;
        }
        break;

    case distribution_type::normal:
        if (old_current != current_item) {
            src.reals[0] = 0.0;
            src.reals[1] = 1.0;
        }

        if (auto m = src.reals[0]; ImGui::InputDouble("mean", &m)) {
            src.reals[0] = m;
            ++up;
        }

        if (auto s = src.reals[1];
            ImGui::InputDouble("stddev", &s) and 0.0 < s) {
            src.reals[1] = s;
            ++up;
        }
        break;

    case distribution_type::lognormal:
        if (old_current != current_item) {
            src.reals[0] = 0.0;
            src.reals[1] = 1.0;
        }

        if (auto m = src.reals[0]; ImGui::InputDouble("mean", &m)) {
            src.reals[0] = m;
            ++up;
        }

        if (auto s = src.reals[1];
            ImGui::InputDouble("stddev", &s) and 0.0 < s) {
            src.reals[1] = s;
            ++up;
        }
        break;

    case distribution_type::chi_squared:
        if (old_current != current_item)
            src.reals[0] = 1.0;

        if (auto n = src.reals[0]; ImGui::InputDouble("n", &n) and 0 < n) {
            src.reals[0] = n;
            ++up;
        }
        break;

    case distribution_type::cauchy:
        if (old_current != current_item) {
            src.reals[0] = 0.0;
            src.reals[1] = 1.0;
        }

        if (auto a = src.reals[0]; ImGui::InputDouble("a", &a)) {
            src.reals[0] = a;
            ++up;
        }

        if (auto b = src.reals[1]; ImGui::InputDouble("b", &b) and 0.0 < b) {
            src.reals[1] = b;
            ++up;
        }
        break;

    case distribution_type::fisher_f:
        if (old_current != current_item) {
            src.reals[0] = 1.0;
            src.reals[1] = 1.0;
        }

        if (auto m = src.reals[0]; ImGui::InputDouble("m", &m) and 0 < m) {
            src.reals[0] = m;
            ++up;
        }

        if (auto n = src.reals[1]; ImGui::InputDouble("s", &n) and 0 < n) {
            src.reals[1] = n;
            ++up;
        }
        break;

    case distribution_type::student_t:
        if (old_current != current_item)
            src.reals[0] = 1.0;

        if (auto n = src.reals[0]; ImGui::InputDouble("n", &n) and 0 < n) {
            src.reals[0] = n;
            ++up;
        }
        break;
    }

    return up > 0;
}

bool show_random_distribution_input(random_source& src) noexcept
{
    int up           = 0;
    int current_item = ordinal(src.distribution);
    int old_current  = ordinal(src.distribution);

    if (ImGui::Combo("Distribution",
                     &current_item,
                     irt::distribution_type_string,
                     IM_ARRAYSIZE(irt::distribution_type_string))) {
        if (current_item != old_current) {
            src.distribution = enum_cast<distribution_type>(current_item);
            up++;
        }
    }

    switch (src.distribution) {
    case distribution_type::uniform_int: {
        if (old_current != current_item) {
            src.ints[0] = 0;
            src.ints[1] = 100;
        }

        int a = src.ints[0];
        int b = src.ints[1];

        if (ImGui::InputInt("a", &a)) {
            up++;

            if (a < b)
                src.ints[0] = a;
            else
                src.ints[1] = a + 1;
        }

        if (ImGui::InputInt("b", &b)) {
            up++;

            if (a < b)
                src.ints[1] = b;
            else
                src.ints[0] = b - 1;
        }
    } break;

    case distribution_type::uniform_real: {
        if (old_current != current_item) {
            src.reals[0] = 0.0;
            src.reals[1] = 1.0;
        }

        auto a = src.reals[0];
        auto b = src.reals[1];

        if (ImGui::InputDouble("a", &a)) {
            ++up;
            if (a < b)
                src.reals[0] = a;
            else
                src.reals[1] = a + 1;
        }

        if (ImGui::InputDouble("b", &b)) {
            ++up;
            if (a < b)
                src.reals[0] = a;
            else
                src.reals[1] = a + 1;
        }
    } break;

    case distribution_type::bernouilli:
        if (old_current != current_item) {
            src.reals[0] = 0.5;
        }
        if (ImGui::InputDouble("p", &src.reals[0]))
            ++up;
        break;

    case distribution_type::binomial:
        if (old_current != current_item) {
            src.reals[0] = 0.5;
            src.ints[0]  = 1;
        }
        if (ImGui::InputDouble("p", &src.reals[0]))
            ++up;
        if (ImGui::InputInt("t", &src.ints[0]))
            ++up;
        break;

    case distribution_type::negative_binomial:
        if (old_current != current_item) {
            src.reals[0] = 0.5;
            src.ints[0]  = 1;
        }
        if (ImGui::InputDouble("p", &src.reals[0]))
            ++up;
        if (ImGui::InputInt("t", &src.ints[0]))
            ++up;
        break;

    case distribution_type::geometric:
        if (old_current != current_item) {
            src.reals[0] = 0.5;
        }
        if (ImGui::InputDouble("p", &src.reals[0]))
            ++up;
        break;

    case distribution_type::poisson:
        if (old_current != current_item) {
            src.reals[0] = 0.5;
        }
        if (ImGui::InputDouble("mean", &src.reals[0]))
            ++up;
        break;

    case distribution_type::exponential:
        if (old_current != current_item) {
            src.reals[0] = 1.0;
        }
        if (ImGui::InputDouble("lambda", &src.reals[0]))
            ++up;
        break;

    case distribution_type::gamma:
        if (old_current != current_item) {
            src.reals[0] = 1.0;
            src.reals[1] = 1.0;
        }
        if (ImGui::InputDouble("alpha", &src.reals[0]))
            ++up;
        if (ImGui::InputDouble("beta", &src.reals[1]))
            ++up;
        break;

    case distribution_type::weibull:
        if (old_current != current_item) {
            src.reals[0] = 1.0;
            src.reals[1] = 1.0;
        }
        if (ImGui::InputDouble("a", &src.reals[0]))
            ++up;
        if (ImGui::InputDouble("b", &src.reals[1]))
            ++up;
        break;

    case distribution_type::exterme_value:
        if (old_current != current_item) {
            src.reals[0] = 1.0;
            src.reals[1] = 0.0;
        }
        if (ImGui::InputDouble("a", &src.reals[0]))
            ++up;
        if (ImGui::InputDouble("b", &src.reals[1]))
            ++up;
        break;

    case distribution_type::normal:
        if (old_current != current_item) {
            src.reals[0] = 0.0;
            src.reals[1] = 1.0;
        }
        if (ImGui::InputDouble("mean", &src.reals[0]))
            ++up;
        if (ImGui::InputDouble("stddev", &src.reals[1]))
            ++up;
        break;

    case distribution_type::lognormal:
        if (old_current != current_item) {
            src.reals[0] = 0.0;
            src.reals[1] = 1.0;
        }
        if (ImGui::InputDouble("m", &src.reals[0]))
            ++up;
        if (ImGui::InputDouble("s", &src.reals[1]))
            ++up;
        break;

    case distribution_type::chi_squared:
        if (old_current != current_item) {
            src.reals[0] = 1.0;
        }
        if (ImGui::InputDouble("n", &src.reals[0]))
            ++up;
        break;

    case distribution_type::cauchy:
        if (old_current != current_item) {
            src.reals[0] = 1.0;
            src.reals[1] = 0.0;
        }
        if (ImGui::InputDouble("a", &src.reals[0]))
            ++up;
        if (ImGui::InputDouble("b", &src.reals[1]))
            ++up;
        break;

    case distribution_type::fisher_f:
        if (old_current != current_item) {
            src.reals[0] = 1.0;
            src.reals[1] = 1.0;
        }
        if (ImGui::InputDouble("m", &src.reals[0]))
            ++up;
        if (ImGui::InputDouble("s", &src.reals[1]))
            ++up;
        break;

    case distribution_type::student_t:
        if (old_current != current_item) {
            src.reals[0] = 1.0;
        }
        if (ImGui::InputDouble("n", &src.reals[0]))
            ++up;
        break;
    }

    return up > 0;
}

project_external_source_editor::project_external_source_editor() noexcept
  : context{ ImPlot::CreateContext() }
{}

project_external_source_editor::~project_external_source_editor() noexcept
{
    if (context)
        ImPlot::DestroyContext(context);
}

void project_external_source_editor::show(application&    app,
                                          project_editor& pj) noexcept
{
    auto old = sel;

    if (ImGui::BeginTable("All sources",
                          4,
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthFixed, 60.f);
        ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("type", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        small_string<32> label;

        {
            constant_source* cst_src     = nullptr;
            constant_source* cst_src_del = nullptr;
            while (pj.pj.sim.srcs.constant_sources.next(cst_src)) {
                ImGui::PushID(cst_src);
                const auto id = pj.pj.sim.srcs.constant_sources.get_id(cst_src);
                const auto index            = get_index(id);
                const bool item_is_selected = sel.is(id);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                format(label, "{}-{}", ordinal(source_type::constant), index);
                if (ImGui::Selectable(label.c_str(),
                                      item_is_selected,
                                      ImGuiSelectableFlags_SpanAllColumns)) {
                    sel.select(id);
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(cst_src->name.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(
                  external_source_str(source_type::constant));
                ImGui::TableNextColumn();
                if (cst_src->buffer.empty()) {
                    ImGui::TextUnformatted("-");
                } else {
                    size_t min = std::min(cst_src->buffer.size(), size_t(3));
                    if (min == 1u)
                        ImGui::Text("%f", cst_src->buffer[0]);
                    else if (min == 2u)
                        ImGui::Text(
                          "%f %f", cst_src->buffer[0], cst_src->buffer[1]);
                    else
                        ImGui::Text("%f %f %f ...",
                                    cst_src->buffer[0],
                                    cst_src->buffer[1],
                                    cst_src->buffer[2]);
                }

                ImGui::PopID();
            }

            if (cst_src_del)
                pj.pj.sim.srcs.constant_sources.free(*cst_src_del);
        }

        {
            text_file_source* txt_src     = nullptr;
            text_file_source* txt_src_del = nullptr;
            while (pj.pj.sim.srcs.text_file_sources.next(txt_src)) {
                ImGui::PushID(txt_src);

                const auto id =
                  pj.pj.sim.srcs.text_file_sources.get_id(txt_src);
                const auto index            = get_index(id);
                const bool item_is_selected = sel.is(id);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                format(label, "{}-{}", ordinal(source_type::text_file), index);
                if (ImGui::Selectable(label.c_str(),
                                      item_is_selected,
                                      ImGuiSelectableFlags_SpanAllColumns)) {
                    sel.select(id);
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(txt_src->name.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(
                  external_source_str(source_type::text_file));
                ImGui::TableNextColumn();
                // ImGui::Text("%s",
                // txt_src->file_path.string().c_str());

                ImGui::PopID();
            }

            if (txt_src_del)
                pj.pj.sim.srcs.text_file_sources.free(*txt_src_del);
        }

        {
            binary_file_source* bin_src     = nullptr;
            binary_file_source* bin_src_del = nullptr;
            while (pj.pj.sim.srcs.binary_file_sources.next(bin_src)) {
                ImGui::PushID(bin_src);
                const auto id =
                  pj.pj.sim.srcs.binary_file_sources.get_id(bin_src);
                const auto index            = get_index(id);
                const bool item_is_selected = sel.is(id);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                format(
                  label, "{}-{}", ordinal(source_type::binary_file), index);
                if (ImGui::Selectable(label.c_str(),
                                      item_is_selected,
                                      ImGuiSelectableFlags_SpanAllColumns)) {
                    sel.select(id);
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(bin_src->name.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(
                  external_source_str(source_type::binary_file));
                ImGui::TableNextColumn();
                // ImGui::Text("%s",
                // bin_src->file_path.string().c_str());

                ImGui::PopID();
            }

            if (bin_src_del)
                pj.pj.sim.srcs.binary_file_sources.free(*bin_src_del);
        }

        {
            random_source* rnd_src     = nullptr;
            random_source* rnd_src_del = nullptr;
            while (pj.pj.sim.srcs.random_sources.next(rnd_src)) {
                ImGui::PushID(rnd_src);
                const auto id = pj.pj.sim.srcs.random_sources.get_id(rnd_src);
                const auto index            = get_index(id);
                const bool item_is_selected = sel.is(id);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                format(label, "{}-{}", ordinal(source_type::random), index);
                if (ImGui::Selectable(label.c_str(),
                                      item_is_selected,
                                      ImGuiSelectableFlags_SpanAllColumns)) {
                    sel.select(id);
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(rnd_src->name.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(
                  external_source_str(source_type::random));
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(distribution_str(rnd_src->distribution));

                ImGui::PopID();
            }

            if (rnd_src_del)
                pj.pj.sim.srcs.random_sources.free(*rnd_src_del);
        }

        ImGui::EndTable();

        ImGui::Spacing();
        ImGui::InputScalarN("seed",
                            ImGuiDataType_U64,
                            &pj.pj.sim.srcs.seed,
                            2,
                            nullptr,
                            nullptr,
                            nullptr,
                            ImGuiInputTextFlags_CharsHexadecimal);

        display_allocate_external_source(app, pj);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (sel.type_sel.has_value()) {
        auto up = 0;

        if (ImGui::CollapsingHeader("Source editor",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            switch (*sel.type_sel) {
            case source_type::constant: {
                const auto id  = enum_cast<constant_source_id>(sel.id_sel);
                const auto idx = get_index(id);
                if (auto* ptr = pj.pj.sim.srcs.constant_sources.try_to_get(id);
                    ptr) {
                    auto new_size = ptr->length;

                    up += ImGui::InputScalar("id",
                                             ImGuiDataType_U16,
                                             const_cast<uint16_t*>(&idx),
                                             nullptr,
                                             nullptr,
                                             nullptr,
                                             ImGuiInputTextFlags_ReadOnly);

                    up += ImGui::InputSmallString("name", ptr->name);

                    if (ImGui::InputScalar(
                          "length", ImGuiDataType_U32, &new_size) &&
                        new_size != ptr->length &&
                        new_size < external_source_chunk_size) {
                        ptr->length = new_size;
                        ++up;
                    }

                    for (u32 i = 0; i < ptr->length; ++i) {
                        ImGui::PushID(static_cast<int>(i));
                        up += ImGui::InputDouble("##name", &ptr->buffer[i]);
                        ImGui::PopID();
                    }
                }
            } break;

            case source_type::text_file: {
                const auto id  = enum_cast<text_file_source_id>(sel.id_sel);
                const auto idx = get_index(id);
                if (auto* ptr = pj.pj.sim.srcs.text_file_sources.try_to_get(id);
                    ptr) {

                    ImGui::InputScalar("id",
                                       ImGuiDataType_U16,
                                       const_cast<uint16_t*>(&idx),
                                       nullptr,
                                       nullptr,
                                       nullptr,
                                       ImGuiInputTextFlags_ReadOnly);

                    ImGui::InputSmallString("name", ptr->name);

                    // ImGui::Text("%s",
                    // text_file_ptr->file_path.string().c_str());
                    if (ImGui::Button("...")) {
                        show_file_dialog = true;
                    }
                }
            } break;

            case source_type::binary_file: {
                const auto id  = enum_cast<binary_file_source_id>(sel.id_sel);
                const auto idx = get_index(id);
                if (auto* ptr =
                      pj.pj.sim.srcs.binary_file_sources.try_to_get(id);
                    ptr) {
                    ImGui::InputScalar("id",
                                       ImGuiDataType_U16,
                                       const_cast<uint16_t*>(&idx),
                                       nullptr,
                                       nullptr,
                                       nullptr,
                                       ImGuiInputTextFlags_ReadOnly);

                    ImGui::InputSmallString("name", ptr->name);

                    if (ImGui::InputScalar(
                          "max source",
                          ImGuiDataType_U32,
                          reinterpret_cast<void*>(&ptr->max_clients))) {
                        if (auto ret = ptr->init(); !ret) {
                            app.jn.push(log_level::error, [](auto& t, auto&) {
                                t = "Fail to initialize binary file source";
                            });
                        }
                    }

                    // ImGui::Text("%s",
                    // binary_file_ptr->file_path.string().c_str());
                    if (ImGui::Button("...")) {
                        show_file_dialog = true;
                    }
                }
            } break;

            case source_type::random: {
                const auto id  = enum_cast<random_source_id>(sel.id_sel);
                const auto idx = get_index(id);
                if (auto* ptr = pj.pj.sim.srcs.random_sources.try_to_get(id);
                    ptr) {
                    up += ImGui::InputScalar("id",
                                             ImGuiDataType_U16,
                                             const_cast<uint16_t*>(&idx),
                                             nullptr,
                                             nullptr,
                                             nullptr,
                                             ImGuiInputTextFlags_ReadOnly);

                    up += ImGui::InputSmallString("name", ptr->name);

                    up += show_random_distribution_input(*ptr);
                }
            } break;
            }
        }

        if (sel.type_sel.has_value() and show_file_dialog) {
            if (*sel.type_sel == source_type::binary_file) {
                const auto id = enum_cast<binary_file_source_id>(sel.id_sel);
                if (auto* ptr =
                      pj.pj.sim.srcs.binary_file_sources.try_to_get(id);
                    ptr) {
                    const char*    title = "Select binary file path to load";
                    const char8_t* filters[] = { u8".dat", nullptr };

                    ImGui::OpenPopup(title);
                    if (app.f_dialog.show_load_file(title, filters)) {
                        if (app.f_dialog.state == file_dialog::status::ok) {
                            ptr->file_path = app.f_dialog.result;

                            app.start_init_source(app.pjs.get_id(pj),
                                                  sel.id_sel,
                                                  source_type::binary_file);
                        }
                        app.f_dialog.clear();
                        show_file_dialog = false;
                    }
                }
            } else if (*sel.type_sel == source_type::text_file) {
                const auto id = enum_cast<text_file_source_id>(sel.id_sel);
                if (auto* ptr = pj.pj.sim.srcs.text_file_sources.try_to_get(id);
                    ptr) {
                    const char*    title     = "Select text file path to load";
                    const char8_t* filters[] = { u8".txt", nullptr };

                    ImGui::OpenPopup(title);
                    if (app.f_dialog.show_load_file(title, filters)) {
                        if (app.f_dialog.state == file_dialog::status::ok) {
                            ptr->file_path = app.f_dialog.result;

                            app.start_init_source(app.pjs.get_id(pj),
                                                  sel.id_sel,
                                                  source_type::text_file);
                        }
                        app.f_dialog.clear();
                        show_file_dialog = false;
                    }
                }
            }
        }

        plot.read([&](const auto& vec, const auto version) noexcept {
            if (vec.empty())
                return;

            ImPlot::SetNextAxesToFit();
            if (ImPlot::BeginPlot("External source preview", ImVec2(-1, -1))) {
                ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
                ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

                ImPlot::PlotScatter("value", vec.data(), vec.size(), 1.0, 0.0);

                ImPlot::PopStyleVar(2);
                ImPlot::EndPlot();
            }
        });

        if ((up or old.id_sel != sel.id_sel) and
            any_equal(
              *sel.type_sel, source_type::constant, source_type::random))
            app.start_init_source(
              app.pjs.get_id(pj), sel.id_sel, *sel.type_sel);
    }
}

static void build_source_element_name(
  const external_source_definition::source_element& src,
  const name_str&                                   str,
  small_string<63>&                                 out) noexcept
{
    static const char* names[] = { "(cst)", "(bin)", "(txt)", "(rnd)" };

    format(out, "{} {}", str.sv(), names[src.index()]);
}

static void get_source_element(const external_source_definition&    srcs,
                               const external_source_definition::id id,
                               small_string<63>& out) noexcept
{
    if (srcs.data.exists(id))
        build_source_element_name(
          srcs.data.get<external_source_definition::source_element>(id),
          srcs.data.get<name_str>(id),
          out);
    else
        out = "-";
}

void show_combobox_external_sources(external_source_definition&     srcs,
                                    external_source_definition::id& elem_id,
                                    const char* name) noexcept
{
    small_string<63> buf;

    if (ImGui::BeginCombo(name, name)) {
        if (ImGui::Selectable("-", not srcs.data.exists(elem_id)))
            elem_id = undefined<external_source_definition::id>();

        for (const auto id : srcs.data) {
            get_source_element(srcs, id, buf);
            ImGui::PushID(ordinal(id));
            if (ImGui::Selectable(buf.c_str(), id == elem_id))
                elem_id = id;
            ImGui::PopID();
        }

        ImGui::EndCombo();
    }
}

template<typename Data, typename ID>
bool show_external_source_id_combo(const Data&       data,
                                   const ID&         id,
                                   const source_type type,
                                   i64&              out) noexcept
{
    const auto* elem     = data.try_to_get(id);
    const auto* preview  = elem ? elem->name.c_str() : "-";
    const auto  copy_out = out;

    if (ImGui::BeginCombo("Source Element", preview)) {
        if (ImGui::Selectable("-", elem == nullptr))
            out = u32s_to_u64(ordinal(type), 0);

        for (const auto& d : data) {
            ImGui::PushID(ordinal(data.get_id(d)));
            if (ImGui::Selectable(d.name.c_str(), data.get_id(d) == id))
                out = u32s_to_u64(ordinal(type), ordinal(data.get_id(d)));
            ImGui::PopID();
        }

        ImGui::EndCombo();
    }

    return copy_out != out;
}

bool show_external_source_id_combo(external_source&    srcs,
                                   const source_type   type,
                                   const source_any_id id,
                                   i64&                src) noexcept
{
    switch (type) {
    case source_type::constant:
        return show_external_source_id_combo(
          srcs.constant_sources, id.constant_id, type, src);

    case source_type::binary_file:
        return show_external_source_id_combo(
          srcs.binary_file_sources, id.binary_file_id, type, src);

    case source_type::text_file:
        return show_external_source_id_combo(
          srcs.text_file_sources, id.text_file_id, type, src);

    case source_type::random:
        return show_external_source_id_combo(
          srcs.random_sources, id.random_id, type, src);

    default:
        return false;
    }
}

static auto get_and_clean_source(const external_source& srcs,
                                 const i64              src) noexcept
  -> std::pair<source_type, source_any_id>
{
    const auto [type, id] = get_source(static_cast<u64>(src));

    switch (type) {
    case source_type::constant:
        if (srcs.constant_sources.try_to_get(id.constant_id))
            return { type, id };
        break;
    case source_type::binary_file:
        if (srcs.binary_file_sources.try_to_get(id.binary_file_id))
            return { type, id };
        break;
    case source_type::text_file:
        if (srcs.text_file_sources.try_to_get(id.text_file_id))
            return { type, id };
        break;
    case source_type::random:
        if (srcs.random_sources.try_to_get(id.random_id))
            return { type, id };
        break;
    }

    return { source_type::constant, source_any_id{ constant_source_id{} } };
}

bool show_external_sources_combo(const char*      title,
                                 external_source& srcs,
                                 i64&             src) noexcept
{
    if (ImGui::CollapsingHeader(title)) {
        auto [type, id] = get_and_clean_source(srcs, src);

        if (auto new_type = static_cast<int>(ordinal(type));
            ImGui::Combo("Source Type",
                         &new_type,
                         external_source_type_string,
                         length(external_source_type_string))) {
            if (enum_cast<source_type>(new_type) != type) {
                id.constant_id = undefined<constant_source_id>();
                type           = enum_cast<source_type>(new_type);
                src            = u32s_to_u64(ordinal(type), 0);
            }
        }

        return show_external_source_id_combo(srcs, type, id, src);
    }

    return false;
}

bool show_external_sources_combo(const char*                 title,
                                 external_source_definition& srcs,
                                 i64&                        src) noexcept
{
    auto       buf     = small_string<63>{};
    const auto id      = enum_cast<external_source_definition::id>(src);
    auto       copy_id = id;

    get_source_element(srcs, copy_id, buf);

    if (ImGui::BeginCombo(title, buf.c_str())) {
        if (ImGui::Selectable("-", not srcs.data.exists(copy_id)))
            copy_id = undefined<external_source_definition::id>();

        for (const auto i : srcs.data) {
            get_source_element(srcs, i, buf);
            ImGui::PushID(ordinal(i));
            if (ImGui::Selectable(buf.c_str(), copy_id == i))
                copy_id = i;
            ImGui::PopID();
        }

        ImGui::EndCombo();
    }

    if (id != copy_id) {
        src = ordinal(copy_id);
        return true;
    }

    return false;
}

void show_menu_external_sources(application&     app,
                                external_source& srcs,
                                const char*      title,
                                source&          src,
                                source_data&     src_data) noexcept
{
    small_string<64> tmp;

    constant_source*    constant_ptr    = nullptr;
    binary_file_source* binary_file_ptr = nullptr;
    text_file_source*   text_file_ptr   = nullptr;
    random_source*      random_ptr      = nullptr;

    if (ImGui::BeginPopup(title)) {
        if (ImGui::BeginMenu("Constant")) {
            constant_source* s = nullptr;
            while (srcs.constant_sources.next(s)) {
                const auto id    = srcs.constant_sources.get_id(s);
                const auto index = get_index(id);

                format(tmp,
                       "{}-{}-{}",
                       external_source_str(source_type::constant),
                       index,
                       s->name.c_str());
                if (ImGui::MenuItem(tmp.c_str())) {
                    constant_ptr = s;
                    break;
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Binary files")) {
            binary_file_source* s = nullptr;
            while (srcs.binary_file_sources.next(s)) {
                const auto id    = srcs.binary_file_sources.get_id(s);
                const auto index = get_index(id);

                format(tmp,
                       "{}-{}-{}",
                       external_source_str(source_type::binary_file),
                       index,
                       s->name.c_str());
                if (ImGui::MenuItem(tmp.c_str())) {
                    binary_file_ptr = s;
                    break;
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Text files")) {
            text_file_source* s = nullptr;
            while (srcs.text_file_sources.next(s)) {
                const auto id    = srcs.text_file_sources.get_id(s);
                const auto index = get_index(id);

                format(tmp,
                       "{}-{}-{}",
                       external_source_str(source_type::text_file),
                       index,
                       s->name.c_str());
                if (ImGui::MenuItem(tmp.c_str())) {
                    text_file_ptr = s;
                    break;
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Random")) {
            random_source* s = nullptr;
            while (srcs.random_sources.next(s)) {
                const auto id    = srcs.random_sources.get_id(s);
                const auto index = get_index(id);

                format(tmp,
                       "{}-{}-{}",
                       external_source_str(source_type::binary_file),
                       index,
                       s->name.c_str());
                if (ImGui::MenuItem(tmp.c_str())) {
                    random_ptr = s;
                    break;
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

    if (constant_ptr) {
        src.reset();
        if (auto ret = constant_ptr->init(src, src_data); !ret) {
            app.jn.push(log_level::error, [](auto& t, auto&) {
                t = "Fail to initalize constant source";
            });
        }
    }

    if (binary_file_ptr) {
        src.reset();
        if (auto ret = binary_file_ptr->init(src, src_data); !ret) {
            app.jn.push(log_level::error, [](auto& t, auto&) {
                t = "Fail to initalize binary file source";
            });
        }
    }

    if (text_file_ptr) {
        src.reset();
        if (auto ret = text_file_ptr->init(src, src_data); !ret) {
            app.jn.push(log_level::error, [](auto& t, auto&) {
                t = "Fail to initalize text file source";
            });
        }
    }

    if (random_ptr) {
        src.reset();
        // if (auto ret = random_ptr->init(src, src_data); !ret) {
        //     app.jn.push(log_level::error, [](auto& t, auto&) {
        //         t = "Fail to initalize random source";
        //     });
        // }
    }
}

void project_external_source_editor::selection::clear() noexcept
{
    type_sel.reset();
    id_sel = 0;
}

void project_external_source_editor::selection::select(
  constant_source_id id) noexcept
{
    type_sel = source_type::constant;
    id_sel   = ordinal(id);
}

void project_external_source_editor::selection::select(
  text_file_source_id id) noexcept
{
    type_sel = source_type::text_file;
    id_sel   = ordinal(id);
}

void project_external_source_editor::selection::select(
  binary_file_source_id id) noexcept
{
    type_sel = source_type::binary_file;
    id_sel   = ordinal(id);
}

void project_external_source_editor::selection::select(
  random_source_id id) noexcept
{
    type_sel = source_type::random;
    id_sel   = ordinal(id);
}

bool project_external_source_editor::selection::is(
  constant_source_id id) const noexcept
{
    return type_sel.has_value() and * type_sel == source_type::constant and
           id_sel == ordinal(id);
}

bool project_external_source_editor::selection::is(
  text_file_source_id id) const noexcept
{
    return type_sel.has_value() and * type_sel == source_type::text_file and
           id_sel == ordinal(id);
}

bool project_external_source_editor::selection::is(
  binary_file_source_id id) const noexcept
{
    return type_sel.has_value() and * type_sel == source_type::binary_file and
           id_sel == ordinal(id);
}

bool project_external_source_editor::selection::is(
  random_source_id id) const noexcept
{
    return type_sel.has_value() and * type_sel == source_type::random and
           id_sel == ordinal(id);
}

void project_external_source_editor::fill_plot(std::span<double> data) noexcept
{
    plot.write([&](auto& vec) noexcept {
        vec.clear();
        vec.reserve(data.size());

        std::transform(data.begin(),
                       data.end(),
                       std::back_inserter(vec),
                       [](const auto v) { return static_cast<float>(v); });
    });
}

} // namespace irt
