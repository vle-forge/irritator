// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/io.hpp>

#include "application.hpp"
#include "dialog.hpp"
#include "internal.hpp"

namespace irt {

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

void show_random_distribution_input(random_source& src) noexcept
{
    int current_item = ordinal(src.distribution);
    int old_current  = ordinal(src.distribution);
    ImGui::Combo("Distribution",
                 &current_item,
                 irt::distribution_type_string,
                 IM_ARRAYSIZE(irt::distribution_type_string));

    src.distribution = enum_cast<distribution_type>(current_item);

    switch (src.distribution) {
    case distribution_type::uniform_int: {
        if (old_current != current_item) {
            src.a32 = 0;
            src.b32 = 100;
        }

        int a = src.a32;
        int b = src.b32;

        if (ImGui::InputInt("a", &a)) {
            if (a < b)
                src.a32 = a;
        }

        if (ImGui::InputInt("b", &b)) {
            if (a < b)
                src.b32 = b;
        }
    } break;

    case distribution_type::uniform_real:
        if (old_current != current_item) {
            src.a = 0.0;
            src.b = 1.0;
        }
        ImGui::InputDouble("a", &src.a);
        ImGui::InputDouble("b", &src.b); // a < b
        break;

    case distribution_type::bernouilli:
        if (old_current != current_item) {
            src.p = 0.5;
        }
        ImGui::InputDouble("p", &src.p);
        break;

    case distribution_type::binomial:
        if (old_current != current_item) {
            src.p   = 0.5;
            src.t32 = 1;
        }
        ImGui::InputDouble("p", &src.p);
        ImGui::InputInt("t", &src.t32);
        break;

    case distribution_type::negative_binomial:
        if (old_current != current_item) {
            src.p   = 0.5;
            src.t32 = 1;
        }
        ImGui::InputDouble("p", &src.p);
        ImGui::InputInt("t", &src.k32);
        break;

    case distribution_type::geometric:
        if (old_current != current_item) {
            src.p = 0.5;
        }
        ImGui::InputDouble("p", &src.p);
        break;

    case distribution_type::poisson:
        if (old_current != current_item) {
            src.mean = 0.5;
        }
        ImGui::InputDouble("mean", &src.mean);
        break;

    case distribution_type::exponential:
        if (old_current != current_item) {
            src.lambda = 1.0;
        }
        ImGui::InputDouble("lambda", &src.lambda);
        break;

    case distribution_type::gamma:
        if (old_current != current_item) {
            src.alpha = 1.0;
            src.beta  = 1.0;
        }
        ImGui::InputDouble("alpha", &src.alpha);
        ImGui::InputDouble("beta", &src.beta);
        break;

    case distribution_type::weibull:
        if (old_current != current_item) {
            src.a = 1.0;
            src.b = 1.0;
        }
        ImGui::InputDouble("a", &src.a);
        ImGui::InputDouble("b", &src.b);
        break;

    case distribution_type::exterme_value:
        if (old_current != current_item) {
            src.a = 1.0;
            src.b = 0.0;
        }
        ImGui::InputDouble("a", &src.a);
        ImGui::InputDouble("b", &src.b);
        break;

    case distribution_type::normal:
        if (old_current != current_item) {
            src.mean   = 0.0;
            src.stddev = 1.0;
        }
        ImGui::InputDouble("mean", &src.mean);
        ImGui::InputDouble("stddev", &src.stddev);
        break;

    case distribution_type::lognormal:
        if (old_current != current_item) {
            src.m = 0.0;
            src.s = 1.0;
        }
        ImGui::InputDouble("m", &src.m);
        ImGui::InputDouble("s", &src.s);
        break;

    case distribution_type::chi_squared:
        if (old_current != current_item) {
            src.n = 1.0;
        }
        ImGui::InputDouble("n", &src.n);
        break;

    case distribution_type::cauchy:
        if (old_current != current_item) {
            src.a = 1.0;
            src.b = 0.0;
        }
        ImGui::InputDouble("a", &src.a);
        ImGui::InputDouble("b", &src.b);
        break;

    case distribution_type::fisher_f:
        if (old_current != current_item) {
            src.m = 1.0;
            src.n = 1.0;
        }
        ImGui::InputDouble("m", &src.m);
        ImGui::InputDouble("s", &src.n);
        break;

    case distribution_type::student_t:
        if (old_current != current_item) {
            src.n = 1.0;
        }
        ImGui::InputDouble("n", &src.n);
        break;
    }
}

// static void task_try_finalize_source(application&        app,
//                                      u64                 id,
//                                      source::source_type type) noexcept
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
    auto& pj = container_of(this, &project_editor::data_ed);

    if (ImGui::BeginTable("All sources",
                          5,
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthFixed, 60.f);
        ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("type", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("action", ImGuiTableColumnFlags_WidthStretch);
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
                ImGui::TableNextColumn();
                if (ImGui::Button("del")) {
                    cst_src_del = cst_src;
                    if (sel.is(id))
                        sel.clear();
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
                // ImGui::Text("%s", txt_src->file_path.string().c_str());

                ImGui::TableNextColumn();
                if (ImGui::Button("del")) {
                    txt_src_del = txt_src;
                    if (sel.is(id))
                        sel.clear();
                }

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
                // ImGui::Text("%s", bin_src->file_path.string().c_str());
                ImGui::TableNextColumn();
                if (ImGui::Button("del")) {
                    bin_src_del = bin_src;
                    if (sel.is(id))
                        sel.clear();
                }

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

                ImGui::TableNextColumn();
                if (ImGui::Button("del")) {
                    rnd_src_del = rnd_src;
                    if (sel.is(id))
                        sel.clear();
                }

                ImGui::PopID();
            }

            if (rnd_src_del)
                pj.pj.sim.srcs.random_sources.free(*rnd_src_del);
        }

        ImGui::EndTable();

        ImGuiStyle& style = ImGui::GetStyle();
        const float width =
          (ImGui::GetContentRegionAvail().x - 4.f * style.ItemSpacing.x) / 4.f;
        ImVec2 button_sz(width, 20);

        ImGui::Spacing();
        ImGui::InputScalarN("seed",
                            ImGuiDataType_U64,
                            &pj.pj.sim.srcs.seed,
                            2,
                            nullptr,
                            nullptr,
                            nullptr,
                            ImGuiInputTextFlags_CharsHexadecimal);

        if (ImGui::Button("+constant", button_sz)) {
            if (pj.pj.sim.srcs.constant_sources.can_alloc(1u)) {
                auto& new_src = pj.pj.sim.srcs.constant_sources.alloc();
                attempt_all(
                  [&]() noexcept -> status {
                      irt_check(new_src.init());
                      new_src.length    = 3;
                      new_src.buffer[0] = 0.0;
                      new_src.buffer[1] = 1.0;
                      new_src.buffer[2] = 2.0;
                      return success();
                  },

                  [&](const external_source::part s) noexcept -> void {
                      auto& n = app.notifications.alloc();
                      n.title = "Fail to initialize source";
                      format(n.message, "Error: {}", ordinal(s));
                      app.notifications.enable(n);
                  },

                  [&]() noexcept -> void {
                      auto& n   = app.notifications.alloc();
                      n.title   = "Fail to initialize source";
                      n.message = "Error: unknown";
                      app.notifications.enable(n);
                  });
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("+text file", button_sz)) {
            if (pj.pj.sim.srcs.text_file_sources.can_alloc(1u)) {
                auto& new_src = pj.pj.sim.srcs.text_file_sources.alloc();
                (void)new_src;
                // attempt_all(
                //   [&]() noexcept -> status {
                //       irt_check(new_src.init());
                //       return success();
                //   },

                //  [&](const external_source::part s) noexcept -> void {
                //      auto& n = app.notifications.alloc();
                //      n.title = "Fail to initialize source";
                //      format(n.message, "Error: {}", ordinal(s));
                //      app.notifications.enable(n);
                //  },

                //  [&]() noexcept -> void {
                //      auto& n   = app.notifications.alloc();
                //      n.title   = "Fail to initialize source";
                //      n.message = "Error: unknown";
                //      app.notifications.enable(n);
                //  });
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("+binary file", button_sz)) {
            if (pj.pj.sim.srcs.binary_file_sources.can_alloc(1u)) {
                auto& new_src = pj.pj.sim.srcs.binary_file_sources.alloc();
                (void)new_src;
                // attempt_all(
                //   [&]() noexcept -> status {
                //       irt_check(new_src.init());
                //       return success();
                //   },

                //  [&](const external_source::part s) noexcept -> void {
                //      auto& n = app.notifications.alloc();
                //      n.title = "Fail to initialize source";
                //      format(n.message, "Error: {}", ordinal(s));
                //      app.notifications.enable(n);
                //  },

                //  [&]() noexcept -> void {
                //      auto& n   = app.notifications.alloc();
                //      n.title   = "Fail to initialize source";
                //      n.message = "Error: unknown";
                //      app.notifications.enable(n);
                //  });
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("+random", button_sz)) {
            if (pj.pj.sim.srcs.random_sources.can_alloc(1u)) {
                auto& new_src = pj.pj.sim.srcs.random_sources.alloc();
                attempt_all(
                  [&]() noexcept -> status {
                      irt_check(new_src.init());
                      new_src.a32          = 0;
                      new_src.b32          = 100;
                      new_src.distribution = distribution_type::uniform_int;
                      return success();
                  },

                  [&](const external_source::part s) noexcept -> void {
                      auto& n = app.notifications.alloc();
                      n.title = "Fail to initialize source";
                      format(n.message, "Error: {}", ordinal(s));
                      app.notifications.enable(n);
                  },

                  [&]() noexcept -> void {
                      auto& n   = app.notifications.alloc();
                      n.title   = "Fail to initialize source";
                      n.message = "Error: unknown";
                      app.notifications.enable(n);
                  });
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (sel.type_sel.has_value() and
        ImGui::CollapsingHeader("Source editor",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        switch (*sel.type_sel) {
        case source::source_type::constant: {
            const auto id  = enum_cast<constant_source_id>(sel.id_sel);
            const auto idx = get_index(id);
            if (auto* ptr = pj.pj.sim.srcs.constant_sources.try_to_get(id);
                ptr) {
                auto new_size = ptr->length;

                ImGui::InputScalar("id",
                                   ImGuiDataType_U32,
                                   const_cast<uint32_t*>(&idx),
                                   nullptr,
                                   nullptr,
                                   nullptr,
                                   ImGuiInputTextFlags_ReadOnly);

                ImGui::InputSmallString("name", ptr->name);

                if (ImGui::InputScalar(
                      "length", ImGuiDataType_U32, &new_size) &&
                    new_size != ptr->length &&
                    new_size < external_source_chunk_size) {
                    ptr->length = new_size;
                }

                for (u32 i = 0; i < ptr->length; ++i) {
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::InputDouble("##name", &ptr->buffer[i]);
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

                // ImGui::Text("%s", text_file_ptr->file_path.string().c_str());
                if (ImGui::Button("...")) {
                    show_file_dialog = true;
                }
            }
        } break;

        case source::source_type::binary_file: {
            const auto id  = enum_cast<binary_file_source_id>(sel.id_sel);
            const auto idx = get_index(id);
            if (auto* ptr = pj.pj.sim.srcs.binary_file_sources.try_to_get(id);
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
                        auto& n = app.notifications.alloc();
                        n.title = "Fail to initialize binary file source";
                        app.notifications.enable(n);
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
            if (auto* ptr = pj.pj.sim.srcs.random_sources.try_to_get(id); ptr) {
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
                        auto& n = app.notifications.alloc();
                        n.title = "Fail to initialize random source";
                        app.notifications.enable(n);
                    }
                }

                show_random_distribution_input(*ptr);
            }
        } break;
        }
    }

    if (sel.type_sel.has_value() and show_file_dialog) {
        if (*sel.type_sel == source::source_type::binary_file) {
            const auto id = enum_cast<binary_file_source_id>(sel.id_sel);
            if (auto* ptr = pj.pj.sim.srcs.binary_file_sources.try_to_get(id);
                ptr) {
                const char*    title     = "Select binary file path to load";
                const char8_t* filters[] = { u8".dat", nullptr };

                ImGui::OpenPopup(title);
                if (app.f_dialog.show_load_file(title, filters)) {
                    if (app.f_dialog.state == file_dialog::status::ok) {
                        ptr->file_path = app.f_dialog.result;

                        app.start_init_source(app.pjs.get_id(pj),
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

                        app.start_init_source(app.pjs.get_id(pj),
                                              sel.id_sel,
                                              source::source_type::text_file);
                    }
                    app.f_dialog.clear();
                    show_file_dialog = false;
                }
            }
        }
    }

    if (plot_available) {
        debug::ensure(plot.size() > 0);
        if (ImPlot::BeginPlot("Plot", ImVec2(-1, -1))) {
            ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
            ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

            ImPlot::PlotScatter(
              "value", &plot[0].x, &plot[0].y, plot.Size, 0, sizeof(ImVec2));

            ImPlot::PopStyleVar(2);
            ImPlot::EndPlot();
        }
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
            auto& n = app.notifications.alloc();
            n.title = "Fail to initalize constant source";
            app.notifications.enable(n);
        }
    }

    if (binary_file_ptr) {
        src.reset();
        if (auto ret = binary_file_ptr->init(src); !ret) {
            auto& n = app.notifications.alloc();
            n.title = "Fail to initalize binary file source";
            app.notifications.enable(n);
        }
    }

    if (text_file_ptr) {
        src.reset();
        if (auto ret = text_file_ptr->init(src); !ret) {
            auto& n = app.notifications.alloc();
            n.title = "Fail to initalize text file source";
            app.notifications.enable(n);
        }
    }

    if (random_ptr) {
        src.reset();
        if (auto ret = random_ptr->init(src); !ret) {
            auto& n = app.notifications.alloc();
            n.title = "Fail to initalize random source";
            app.notifications.enable(n);
        }
    }
}

void project_external_source_editor::selection::clear() noexcept
{
    type_sel.reset();
    id_sel = 0;
}

void project_external_source_editor::selection::select(constant_source_id id) noexcept
{
    type_sel = source::source_type::constant;
    id_sel   = ordinal(id);
}

void project_external_source_editor::selection::select(text_file_source_id id) noexcept
{
    type_sel = source::source_type::text_file;
    id_sel   = ordinal(id);
}

void project_external_source_editor::selection::select(binary_file_source_id id) noexcept
{
    type_sel = source::source_type::binary_file;
    id_sel   = ordinal(id);
}

void project_external_source_editor::selection::select(random_source_id id) noexcept
{
    type_sel = source::source_type::random;
    id_sel   = ordinal(id);
}

bool project_external_source_editor::selection::is(constant_source_id id) const noexcept
{
    return type_sel.has_value() and * type_sel ==
             source::source_type::constant and
           id_sel == ordinal(id);
}

bool project_external_source_editor::selection::is(text_file_source_id id) const noexcept
{
    return type_sel.has_value() and * type_sel ==
             source::source_type::text_file and
           id_sel == ordinal(id);
}

bool project_external_source_editor::selection::is(binary_file_source_id id) const noexcept
{
    return type_sel.has_value() and * type_sel ==
             source::source_type::binary_file and
           id_sel == ordinal(id);
}

bool project_external_source_editor::selection::is(random_source_id id) const noexcept
{
    return type_sel.has_value() and * type_sel ==
             source::source_type::random and
           id_sel == ordinal(id);
}

} // namespace irt
