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
static status try_allocate_external_source(application&        app,
                                           project_editor&     pj,
                                           source::source_type type,
                                           DataArray&          d) noexcept
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

static status display_allocate_external_source(
  application&        app,
  project_editor&     ed,
  source::source_type part) noexcept
{

    switch (part) {
    case source::source_type::constant:
        return try_allocate_external_source(
          app, ed, part, ed.pj.sim.srcs.constant_sources);

    case source::source_type::binary_file:
        return try_allocate_external_source(
          app, ed, part, ed.pj.sim.srcs.binary_file_sources);

    case source::source_type::text_file:
        return try_allocate_external_source(
          app, ed, part, ed.pj.sim.srcs.text_file_sources);

    case source::source_type::random:
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

    std::optional<source::source_type> part;

    if (ImGui::Button("+constant", button_sz))
        part = source::source_type::constant;
    ImGui::SameLine();
    if (ImGui::Button("+text file", button_sz))
        part = source::source_type::text_file;
    ImGui::SameLine();
    if (ImGui::Button("+binary file", button_sz))
        part = source::source_type::binary_file;
    ImGui::SameLine();
    if (ImGui::Button("+random", button_sz))
        part = source::source_type::random;

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

auto show_data_file_input(const modeling&  mod,
                          const component& compo,
                          file_path_id&    id) noexcept -> bool
{
    const auto old_id = id;

    if (auto* dir = mod.dir_paths.try_to_get(compo.dir); dir) {
        const auto preview = [](file_path* f) noexcept -> const char* {
            return f ? f->path.c_str() : "-";
        }(mod.file_paths.try_to_get(id));

        if (ImGui::BeginCombo("Select file", preview)) {
            for (auto f_id : dir->children) {
                if (auto* file = mod.file_paths.try_to_get(f_id); file) {
                    const auto str = file->path.sv();
                    const auto dot = str.find_last_of(".");
                    const auto ext = str.substr(dot);

                    if (not(ext == ".irt" or ext == ".txt")) {
                        if (ImGui::Selectable(file->path.c_str(), id == f_id)) {
                            id = f_id;
                        }
                    }
                }
            }
            ImGui::EndCombo();
        }
    } else {
        ImGui::TextFormatDisabled("This component is not saved");
    }

    return old_id != id;
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
            src.a32 = 0;
            src.b32 = 100;
        }

        int a = src.a32;
        int b = src.b32;

        if (ImGui::InputInt("a", &a)) {
            up++;

            if (a < b)
                src.a32 = a;
            else
                src.b32 = a + 1;
        }

        if (ImGui::InputInt("b", &b)) {
            up++;

            if (a < b)
                src.b32 = b;
            else
                src.a32 = b - 1;
        }
    } break;

    case distribution_type::uniform_real: {
        if (old_current != current_item) {
            src.a = 0.0;
            src.b = 1.0;
        }

        auto a = src.a;
        auto b = src.b;

        if (ImGui::InputDouble("a", &a)) {
            ++up;
            if (a < b)
                src.a = a;
            else
                src.b = a + 1;
        }

        if (ImGui::InputDouble("b", &b)) {
            ++up;
            if (a < b)
                src.a = a;
            else
                src.b = a + 1;
        }
    } break;

    case distribution_type::bernouilli:
        if (old_current != current_item) {
            src.p = 0.5;
        }
        if (ImGui::InputDouble("p", &src.p))
            ++up;
        break;

    case distribution_type::binomial:
        if (old_current != current_item) {
            src.p   = 0.5;
            src.t32 = 1;
        }
        if (ImGui::InputDouble("p", &src.p))
            ++up;
        if (ImGui::InputInt("t", &src.t32))
            ++up;
        break;

    case distribution_type::negative_binomial:
        if (old_current != current_item) {
            src.p   = 0.5;
            src.t32 = 1;
        }
        if (ImGui::InputDouble("p", &src.p))
            ++up;
        if (ImGui::InputInt("t", &src.k32))
            ++up;
        break;

    case distribution_type::geometric:
        if (old_current != current_item) {
            src.p = 0.5;
        }
        if (ImGui::InputDouble("p", &src.p))
            ++up;
        break;

    case distribution_type::poisson:
        if (old_current != current_item) {
            src.mean = 0.5;
        }
        if (ImGui::InputDouble("mean", &src.mean))
            ++up;
        break;

    case distribution_type::exponential:
        if (old_current != current_item) {
            src.lambda = 1.0;
        }
        if (ImGui::InputDouble("lambda", &src.lambda))
            ++up;
        break;

    case distribution_type::gamma:
        if (old_current != current_item) {
            src.alpha = 1.0;
            src.beta  = 1.0;
        }
        if (ImGui::InputDouble("alpha", &src.alpha))
            ++up;
        if (ImGui::InputDouble("beta", &src.beta))
            ++up;
        break;

    case distribution_type::weibull:
        if (old_current != current_item) {
            src.a = 1.0;
            src.b = 1.0;
        }
        if (ImGui::InputDouble("a", &src.a))
            ++up;
        if (ImGui::InputDouble("b", &src.b))
            ++up;
        break;

    case distribution_type::exterme_value:
        if (old_current != current_item) {
            src.a = 1.0;
            src.b = 0.0;
        }
        if (ImGui::InputDouble("a", &src.a))
            ++up;
        if (ImGui::InputDouble("b", &src.b))
            ++up;
        break;

    case distribution_type::normal:
        if (old_current != current_item) {
            src.mean   = 0.0;
            src.stddev = 1.0;
        }
        if (ImGui::InputDouble("mean", &src.mean))
            ++up;
        if (ImGui::InputDouble("stddev", &src.stddev))
            ++up;
        break;

    case distribution_type::lognormal:
        if (old_current != current_item) {
            src.m = 0.0;
            src.s = 1.0;
        }
        if (ImGui::InputDouble("m", &src.m))
            ++up;
        if (ImGui::InputDouble("s", &src.s))
            ++up;
        break;

    case distribution_type::chi_squared:
        if (old_current != current_item) {
            src.n = 1.0;
        }
        if (ImGui::InputDouble("n", &src.n))
            ++up;
        break;

    case distribution_type::cauchy:
        if (old_current != current_item) {
            src.a = 1.0;
            src.b = 0.0;
        }
        if (ImGui::InputDouble("a", &src.a))
            ++up;
        if (ImGui::InputDouble("b", &src.b))
            ++up;
        break;

    case distribution_type::fisher_f:
        if (old_current != current_item) {
            src.m = 1.0;
            src.n = 1.0;
        }
        if (ImGui::InputDouble("m", &src.m))
            ++up;
        if (ImGui::InputDouble("s", &src.n))
            ++up;
        break;

    case distribution_type::student_t:
        if (old_current != current_item) {
            src.n = 1.0;
        }
        if (ImGui::InputDouble("n", &src.n))
            ++up;
        break;
    }

    return up > 0;
}

// static void task_try_finalize_source(application&        app,
//                                      u64                 id,
//                                      source::source_type type)
//                                      noexcept
// {
//     source src;
//     src.id   = id;
//     src.type = type;

//     if (auto ret = app.pj.sim.srcs.dispatch(src,
//     if (auto ret = ed.pj.sim.srcs.dispatch(src,
//     source::operation_type::finalize);
//         !ret) {
//         auto& n = app.notifications.alloc(log_level::error);
//         n.title = "Fail to finalize data";
//         app.notifications.enable(n);
//     }
// }

project_external_source_editor::project_external_source_editor() noexcept
  : context{ ImPlot::CreateContext() }
{}

project_external_source_editor::~project_external_source_editor() noexcept
{
    if (context)
        ImPlot::DestroyContext(context);
}

void project_external_source_editor::show(application& app) noexcept
{
    auto& pj  = container_of(this, &project_editor::data_ed);
    auto  old = sel;

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
                format(label,
                       "{}-{}",
                       ordinal(source::source_type::constant),
                       index);
                if (ImGui::Selectable(label.c_str(),
                                      item_is_selected,
                                      ImGuiSelectableFlags_SpanAllColumns)) {
                    sel.select(id);
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(cst_src->name.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(
                  external_source_str(source::source_type::constant));
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
                format(label,
                       "{}-{}",
                       ordinal(source::source_type::text_file),
                       index);
                if (ImGui::Selectable(label.c_str(),
                                      item_is_selected,
                                      ImGuiSelectableFlags_SpanAllColumns)) {
                    sel.select(id);
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(txt_src->name.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(
                  external_source_str(source::source_type::text_file));
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
                format(label,
                       "{}-{}",
                       ordinal(source::source_type::binary_file),
                       index);
                if (ImGui::Selectable(label.c_str(),
                                      item_is_selected,
                                      ImGuiSelectableFlags_SpanAllColumns)) {
                    sel.select(id);
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(bin_src->name.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(
                  external_source_str(source::source_type::binary_file));
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
                format(
                  label, "{}-{}", ordinal(source::source_type::random), index);
                if (ImGui::Selectable(label.c_str(),
                                      item_is_selected,
                                      ImGuiSelectableFlags_SpanAllColumns)) {
                    sel.select(id);
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(rnd_src->name.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(
                  external_source_str(source::source_type::random));
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
            case source::source_type::constant: {
                const auto id  = enum_cast<constant_source_id>(sel.id_sel);
                const auto idx = get_index(id);
                if (auto* ptr = pj.pj.sim.srcs.constant_sources.try_to_get(id);
                    ptr) {
                    auto new_size = ptr->length;

                    up += ImGui::InputScalar("id",
                                             ImGuiDataType_U32,
                                             const_cast<uint32_t*>(&idx),
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

            case source::source_type::text_file: {
                const auto id  = enum_cast<text_file_source_id>(sel.id_sel);
                const auto idx = get_index(id);
                if (auto* ptr = pj.pj.sim.srcs.text_file_sources.try_to_get(id);
                    ptr) {

                    ImGui::InputScalar("id",
                                       ImGuiDataType_U32,
                                       const_cast<uint32_t*>(&idx),
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

            case source::source_type::binary_file: {
                const auto id  = enum_cast<binary_file_source_id>(sel.id_sel);
                const auto idx = get_index(id);
                if (auto* ptr =
                      pj.pj.sim.srcs.binary_file_sources.try_to_get(id);
                    ptr) {
                    ImGui::InputScalar("id",
                                       ImGuiDataType_U32,
                                       const_cast<uint32_t*>(&idx),
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

            case source::source_type::random: {
                const auto id  = enum_cast<random_source_id>(sel.id_sel);
                const auto idx = get_index(id);
                if (auto* ptr = pj.pj.sim.srcs.random_sources.try_to_get(id);
                    ptr) {
                    up += ImGui::InputScalar("id",
                                             ImGuiDataType_U32,
                                             const_cast<uint32_t*>(&idx),
                                             nullptr,
                                             nullptr,
                                             nullptr,
                                             ImGuiInputTextFlags_ReadOnly);

                    up += ImGui::InputSmallString("name", ptr->name);

                    if (ImGui::InputScalar(
                          "max source",
                          ImGuiDataType_U32,
                          reinterpret_cast<void*>(&ptr->max_clients))) {
                        up++;
                        if (auto ret = ptr->init(); !ret) {
                            app.jn.push(log_level::error, [](auto& t, auto&) {
                                t = "Fail to initialize random source";
                            });
                        }
                    }

                    up += show_random_distribution_input(*ptr);
                }
            } break;
            }
        }

        if (sel.type_sel.has_value() and show_file_dialog) {
            if (*sel.type_sel == source::source_type::binary_file) {
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

                            app.start_init_source(
                              app.pjs.get_id(pj),
                              sel.id_sel,
                              source::source_type::binary_file);
                        }
                        app.f_dialog.clear();
                        show_file_dialog = false;
                    }
                }
            } else if (*sel.type_sel == source::source_type::text_file) {
                const auto id = enum_cast<text_file_source_id>(sel.id_sel);
                if (auto* ptr = pj.pj.sim.srcs.text_file_sources.try_to_get(id);
                    ptr) {
                    const char*    title     = "Select text file path to load";
                    const char8_t* filters[] = { u8".txt", nullptr };

                    ImGui::OpenPopup(title);
                    if (app.f_dialog.show_load_file(title, filters)) {
                        if (app.f_dialog.state == file_dialog::status::ok) {
                            ptr->file_path = app.f_dialog.result;

                            app.start_init_source(
                              app.pjs.get_id(pj),
                              sel.id_sel,
                              source::source_type::text_file);
                        }
                        app.f_dialog.clear();
                        show_file_dialog = false;
                    }
                }
            }
        }

        if (m_status == plot_status::data_available) {
            if (std::unique_lock lock(mutex, std::try_to_lock);
                lock.owns_lock()) {
                debug::ensure(plot.size() > 0);

                ImPlot::SetNextAxesToFit();
                if (ImPlot::BeginPlot("External source preview",
                                      ImVec2(-1, -1))) {
                    ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
                    ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

                    ImPlot::PlotScatter(
                      "value", plot.data(), plot.size(), 1.0, 0.0);

                    ImPlot::PopStyleVar(2);
                    ImPlot::EndPlot();
                }
            } else {
                ImGui::TextUnformatted("preview in progress");
            }
        } else if (m_status == plot_status::work_in_progress)
            ImGui::TextUnformatted("preview in progress");
        else
            ImGui::TextUnformatted("not data to build preview");

        if ((up or old.id_sel != sel.id_sel) and
            any_equal(*sel.type_sel,
                      source::source_type::constant,
                      source::source_type::random))
            app.start_init_source(
              app.pjs.get_id(pj), sel.id_sel, *sel.type_sel);
    }
}

void show_combobox_external_sources(external_source& srcs, source& src) noexcept
{
    const char* preview_t = external_source_type_string[ordinal(src.type)];

    if (ImGui::BeginCombo("type", preview_t)) {
        if (ImGui::Selectable(external_source_type_string[ordinal(
                                source::source_type::constant)],
                              src.type == source::source_type::constant)) {
            src.clear();
            src.type = source::source_type::constant;
        }

        if (ImGui::Selectable(external_source_type_string[ordinal(
                                source::source_type::binary_file)],
                              src.type == source::source_type::binary_file)) {
            src.clear();
            src.type = source::source_type::binary_file;
        }

        if (ImGui::Selectable(external_source_type_string[ordinal(
                                source::source_type::text_file)],
                              src.type == source::source_type::text_file)) {
            src.clear();
            src.type = source::source_type::text_file;
        }

        if (ImGui::Selectable(
              external_source_type_string[ordinal(source::source_type::random)],
              src.type == source::source_type::random)) {
            src.clear();
            src.type = source::source_type::random;
        }

        ImGui::EndCombo();
    }

    auto get_source_name = [](const external_source& srcs,
                              const source& src) noexcept -> const char* {
        switch (src.type) {
        case source::source_type::binary_file:
            if (const auto* s = srcs.binary_file_sources.try_to_get(
                  enum_cast<binary_file_source_id>(src.id));
                s)
                return s->name.c_str();
            break;

        case source::source_type::constant:
            if (const auto* s = srcs.constant_sources.try_to_get(
                  enum_cast<constant_source_id>(src.id));
                s)
                return s->name.c_str();
            break;
        case source::source_type::text_file:
            if (const auto* s = srcs.text_file_sources.try_to_get(
                  enum_cast<text_file_source_id>(src.id));
                s)
                return s->name.c_str();
            break;

        case source::source_type::random:
            if (const auto* s = srcs.random_sources.try_to_get(
                  enum_cast<random_source_id>(src.id));
                s)
                return s->name.c_str();
            break;
        }

        return "-";
    };

    const char* preview_s = get_source_name(srcs, src);

    if (ImGui::BeginCombo("source", preview_s)) {
        switch (src.type) {
        case source::source_type::binary_file:
            for (auto& s : srcs.binary_file_sources) {
                const auto id = ordinal(srcs.binary_file_sources.get_id(s));
                ImGui::PushID(&s);
                if (ImGui::Selectable(s.name.c_str(), id == src.id))
                    src.id = id;
                ImGui::PopID();
            }
            break;

        case source::source_type::constant:
            for (auto& s : srcs.constant_sources) {
                const auto id = ordinal(srcs.constant_sources.get_id(s));

                ImGui::PushID(&s);
                if (ImGui::Selectable(s.name.c_str(), id == src.id))
                    src.id = id;
                ImGui::PopID();
            }
            break;

        case source::source_type::text_file:
            for (auto& s : srcs.text_file_sources) {
                const auto id = ordinal(srcs.text_file_sources.get_id(s));

                ImGui::PushID(&s);
                if (ImGui::Selectable(s.name.c_str(), id == src.id))
                    src.id = id;
                ImGui::PopID();
            }
            break;

        case source::source_type::random:
            for (auto& s : srcs.random_sources) {
                const auto id = ordinal(srcs.random_sources.get_id(s));

                ImGui::PushID(&s);
                if (ImGui::Selectable(s.name.c_str(), id == src.id))
                    src.id = id;
                ImGui::PopID();
            }
            break;
        }

        ImGui::EndCombo();
    }
}

void show_menu_external_sources(application&     app,
                                external_source& srcs,
                                const char*      title,
                                source&          src) noexcept
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
                       external_source_str(source::source_type::constant),
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
                       external_source_str(source::source_type::binary_file),
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
                       external_source_str(source::source_type::text_file),
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
                       external_source_str(source::source_type::binary_file),
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
        if (auto ret = constant_ptr->init(src); !ret) {
            app.jn.push(log_level::error, [](auto& t, auto&) {
                t = "Fail to initalize constant source";
            });
        }
    }

    if (binary_file_ptr) {
        src.reset();
        if (auto ret = binary_file_ptr->init(src); !ret) {
            app.jn.push(log_level::error, [](auto& t, auto&) {
                t = "Fail to initalize binary file source";
            });
        }
    }

    if (text_file_ptr) {
        src.reset();
        if (auto ret = text_file_ptr->init(src); !ret) {
            app.jn.push(log_level::error, [](auto& t, auto&) {
                t = "Fail to initalize text file source";
            });
        }
    }

    if (random_ptr) {
        src.reset();
        if (auto ret = random_ptr->init(src); !ret) {
            app.jn.push(log_level::error, [](auto& t, auto&) {
                t = "Fail to initalize random source";
            });
        }
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
    type_sel = source::source_type::constant;
    id_sel   = ordinal(id);
}

void project_external_source_editor::selection::select(
  text_file_source_id id) noexcept
{
    type_sel = source::source_type::text_file;
    id_sel   = ordinal(id);
}

void project_external_source_editor::selection::select(
  binary_file_source_id id) noexcept
{
    type_sel = source::source_type::binary_file;
    id_sel   = ordinal(id);
}

void project_external_source_editor::selection::select(
  random_source_id id) noexcept
{
    type_sel = source::source_type::random;
    id_sel   = ordinal(id);
}

bool project_external_source_editor::selection::is(
  constant_source_id id) const noexcept
{
    return type_sel.has_value() and
           *type_sel == source::source_type::constant and id_sel == ordinal(id);
}

bool project_external_source_editor::selection::is(
  text_file_source_id id) const noexcept
{
    return type_sel.has_value() and
           *type_sel == source::source_type::text_file and
           id_sel == ordinal(id);
}

bool project_external_source_editor::selection::is(
  binary_file_source_id id) const noexcept
{
    return type_sel.has_value() and
           *type_sel == source::source_type::binary_file and
           id_sel == ordinal(id);
}

bool project_external_source_editor::selection::is(
  random_source_id id) const noexcept
{
    return type_sel.has_value() and *type_sel == source::source_type::random and
           id_sel == ordinal(id);
}

void project_external_source_editor::fill_plot(std::span<double> data) noexcept
{
    m_status = plot_status::work_in_progress;

    std::unique_lock lock(mutex);

    plot.clear();
    plot.reserve(data.size());

    for (sz i = 0, e = data.size(); i != e; ++i)
        plot.push_back(static_cast<float>(data[i]));

    m_status = plot_status::data_available;
}

} // namespace irt
