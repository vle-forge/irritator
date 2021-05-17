// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "gui.hpp"
#include "node-editor.hpp"

#include <fmt/format.h>

#include <future>
#include <random>
#include <thread>

#include <cinttypes>

namespace irt {

irt::source::constant*
sources::new_constant() noexcept
{
    try {
        auto& elem = csts[csts_next_id++];
        return &elem;
    } catch (const std::exception& /*e/*/) {
        return nullptr;
    }
}

irt::source::binary_file*
sources::new_binary_file() noexcept
{
    try {
        auto& elem = bins[bins_next_id++];
        return &elem;
    } catch (const std::exception& /*e/*/) {
        return nullptr;
    }
}

irt::source::text_file*
sources::new_text_file() noexcept
{
    try {
        auto& elem = texts[texts_next_id++];
        return &elem;
    } catch (const std::exception& /*e/*/) {
        return nullptr;
    }
}

enum class distribution
{
    uniform_int,
    uniform_real,
    bernouilli,
    binomial,
    negative_binomial,
    geometric,
    poisson,
    exponential,
    gamma,
    weibull,
    exterme_value,
    normal,
    lognormal,
    chi_squared,
    cauchy,
    fisher_f,
    student_t
};

static constexpr const char* items[] = {
    "uniform-int", "uniform-real",      "bernouilli",
    "binomial",    "negative-binomial", "geometric",
    "poisson",     "exponential",       "gamma",
    "weibull",     "exterme-value",     "normal",
    "lognormal",   "chi-squared",       "cauchy",
    "fisher-f",    "student-t"
};

template<typename RandomGenerator, typename Distribution>
static void
generate(std::ostream& os,
         RandomGenerator& gen,
         Distribution dist,
         const std::size_t size,
         const source::random_file_type type,
         double& status) noexcept
{
    try {
        status = 0.0;

        switch (type) {
        case source::random_file_type::text: {
            if (!os) {
                status = -1.0;
                return;
            }

            for (std::size_t sz = 0; sz < size; ++sz) {
                status =
                  static_cast<double>(sz) * 100.0 / static_cast<double>(size);
                if (!(os << dist(gen) << '\n')) {
                    status = -2.0;
                    return;
                }
            }
        } break;

        case source::random_file_type::binary: {
            if (!os) {
                status = -1.0;
                return;
            }

            for (std::size_t sz = 0; sz < size; ++sz) {
                status =
                  static_cast<double>(sz) * 100.0 / static_cast<double>(size);
                const double value = dist(gen);
                os.write(reinterpret_cast<const char*>(&value), sizeof(value));
            }
        } break;
        }
        status = 100.0;

    } catch (std::exception& /*e*/) {
        status = -3.0;
    }
}

struct distribution_manager
{
    std::thread job;
    std::ofstream os;
    double a, b, p, mean, lambda, alpha, beta, stddev, m, s, n;
    int a32, b32, t32, k32;
    distribution type = distribution::uniform_int;
    bool is_running;
    double status;

    void start(std::mt19937_64& gen,
               sz size,
               irt::source::random_file_type file_type)
    {
        is_running = true;

        switch (type) {
        case distribution::uniform_int:
            job = std::thread(
              generate<std::mt19937_64, std::uniform_int_distribution<int>>,
              std::ref(os),
              std::ref(gen),
              std::uniform_int_distribution(a32, b32),
              size,
              file_type,
              std::ref(status));
            break;

        case distribution::uniform_real:
            job = std::thread(
              generate<std::mt19937_64, std::uniform_real_distribution<>>,
              std::ref(os),
              std::ref(gen),
              std::uniform_real_distribution(a, b),
              size,
              file_type,
              std::ref(status));
            break;

        case distribution::bernouilli:
            job = std::thread(
              generate<std::mt19937_64, std::bernoulli_distribution>,
              std::ref(os),
              std::ref(gen),
              std::bernoulli_distribution(p),
              size,
              file_type,
              std::ref(status));
            break;

        case distribution::binomial:
            job = std::thread(
              generate<std::mt19937_64, std::binomial_distribution<>>,
              std::ref(os),
              std::ref(gen),
              std::binomial_distribution(t32, p),
              size,
              file_type,
              std::ref(status));
            break;

        case distribution::negative_binomial:
            job = std::thread(
              generate<std::mt19937_64, std::negative_binomial_distribution<>>,
              std::ref(os),
              std::ref(gen),
              std::negative_binomial_distribution(t32, p),
              size,
              file_type,
              std::ref(status));
            break;

        case distribution::geometric:
            job = std::thread(
              generate<std::mt19937_64, std::geometric_distribution<>>,
              std::ref(os),
              std::ref(gen),
              std::geometric_distribution(p),
              size,
              file_type,
              std::ref(status));
            break;

        case distribution::poisson:
            job = std::thread(
              generate<std::mt19937_64, std::poisson_distribution<>>,
              std::ref(os),
              std::ref(gen),
              std::poisson_distribution(mean),
              size,
              file_type,
              std::ref(status));
            break;

        case distribution::exponential:
            job = std::thread(
              generate<std::mt19937_64, std::exponential_distribution<>>,
              std::ref(os),
              std::ref(gen),
              std::exponential_distribution(lambda),
              size,
              file_type,
              std::ref(status));
            break;

        case distribution::gamma:
            job =
              std::thread(generate<std::mt19937_64, std::gamma_distribution<>>,
                          std::ref(os),
                          std::ref(gen),
                          std::gamma_distribution(alpha, beta),
                          size,
                          file_type,
                          std::ref(status));
            break;

        case distribution::weibull:
            job = std::thread(
              generate<std::mt19937_64, std::weibull_distribution<>>,
              std::ref(os),
              std::ref(gen),
              std::weibull_distribution(a, b),
              size,
              file_type,
              std::ref(status));
            break;

        case distribution::exterme_value:
            job = std::thread(
              generate<std::mt19937_64, std::extreme_value_distribution<>>,
              std::ref(os),
              std::ref(gen),
              std::extreme_value_distribution(a, b),
              size,
              file_type,
              std::ref(status));
            break;

        case distribution::normal:
            job =
              std::thread(generate<std::mt19937_64, std::normal_distribution<>>,
                          std::ref(os),
                          std::ref(gen),
                          std::normal_distribution(mean, stddev),
                          size,
                          file_type,
                          std::ref(status));
            break;

        case distribution::lognormal:
            job = std::thread(
              generate<std::mt19937_64, std::lognormal_distribution<>>,
              std::ref(os),
              std::ref(gen),
              std::lognormal_distribution(m, s),
              size,
              file_type,
              std::ref(status));
            break;

        case distribution::chi_squared:
            job = std::thread(
              generate<std::mt19937_64, std::chi_squared_distribution<>>,
              std::ref(os),
              std::ref(gen),
              std::chi_squared_distribution(n),
              size,
              file_type,
              std::ref(status));
            break;

        case distribution::cauchy:
            job =
              std::thread(generate<std::mt19937_64, std::cauchy_distribution<>>,
                          std::ref(os),
                          std::ref(gen),
                          std::cauchy_distribution(a, b),
                          size,
                          file_type,
                          std::ref(status));
            break;

        case distribution::fisher_f:
            job = std::thread(
              generate<std::mt19937_64, std::fisher_f_distribution<>>,
              std::ref(os),
              std::ref(gen),
              std::fisher_f_distribution(m, n),
              size,
              file_type,
              std::ref(status));
            break;

        case distribution::student_t:
            job = std::thread(
              generate<std::mt19937_64, std::student_t_distribution<>>,
              std::ref(os),
              std::ref(gen),
              std::student_t_distribution(n),
              size,
              file_type,
              std::ref(status));
            break;

        default:
            irt_unreachable();
        }
    }
};

static void
show_random()
{
    static distribution_manager dm;
    static int current_item = -1;
    static u64 size = 1024 * 1024;
    static bool show_file_dialog = false;
    static bool use_binary = true;

    if (ImGui::CollapsingHeader("Random file source generator")) {
        ImGui::InputScalar("length", ImGuiDataType_U64, &size);
        ImGui::Checkbox("binary file", &use_binary);

        int old_current = current_item;
        ImGui::Combo("Distribution", &current_item, items, IM_ARRAYSIZE(items));

        dm.type = static_cast<distribution>(current_item);
        switch (dm.type) {
        case distribution::uniform_int:
            if (old_current != current_item) {
                dm.a32 = 0;
                dm.b32 = 100;
            }
            ImGui::InputInt("a", &dm.a32);
            ImGui::InputInt("b", &dm.b32);
            break;

        case distribution::uniform_real:
            if (old_current != current_item) {
                dm.a = 0.0;
                dm.b = 1.0;
            }
            ImGui::InputDouble("a", &dm.a);
            ImGui::InputDouble("b", &dm.b);
            break;

        case distribution::bernouilli:
            if (old_current != current_item) {
                dm.p = 0.5;
            }
            ImGui::InputDouble("p", &dm.p);
            break;

        case distribution::binomial:
            if (old_current != current_item) {
                dm.p = 0.5;
                dm.t32 = 1;
            }
            ImGui::InputDouble("p", &dm.p);
            ImGui::InputInt("t", &dm.t32);
            break;

        case distribution::negative_binomial:
            if (old_current != current_item) {
                dm.p = 0.5;
                dm.t32 = 1;
            }
            ImGui::InputDouble("p", &dm.p);
            ImGui::InputInt("t", &dm.k32);
            break;

        case distribution::geometric:
            if (old_current != current_item) {
                dm.p = 0.5;
            }
            ImGui::InputDouble("p", &dm.p);
            break;

        case distribution::poisson:
            if (old_current != current_item) {
                dm.mean = 0.5;
            }
            ImGui::InputDouble("mean", &dm.mean);
            break;

        case distribution::exponential:
            if (old_current != current_item) {
                dm.lambda = 1.0;
            }
            ImGui::InputDouble("lambda", &dm.lambda);
            break;

        case distribution::gamma:
            if (old_current != current_item) {
                dm.alpha = 1.0;
                dm.beta = 1.0;
            }
            ImGui::InputDouble("alpha", &dm.alpha);
            ImGui::InputDouble("beta", &dm.beta);
            break;

        case distribution::weibull:
            if (old_current != current_item) {
                dm.a = 1.0;
                dm.b = 1.0;
            }
            ImGui::InputDouble("a", &dm.a);
            ImGui::InputDouble("b", &dm.b);
            break;

        case distribution::exterme_value:
            if (old_current != current_item) {
                dm.a = 1.0;
                dm.b = 0.0;
            }
            ImGui::InputDouble("a", &dm.a);
            ImGui::InputDouble("b", &dm.b);
            break;

        case distribution::normal:
            if (old_current != current_item) {
                dm.mean = 0.0;
                dm.stddev = 1.0;
            }
            ImGui::InputDouble("mean", &dm.mean);
            ImGui::InputDouble("stddev", &dm.stddev);
            break;

        case distribution::lognormal:
            if (old_current != current_item) {
                dm.m = 0.0;
                dm.s = 1.0;
            }
            ImGui::InputDouble("m", &dm.m);
            ImGui::InputDouble("s", &dm.s);
            break;

        case distribution::chi_squared:
            if (old_current != current_item) {
                dm.n = 1.0;
            }
            ImGui::InputDouble("n", &dm.n);
            break;

        case distribution::cauchy:
            if (old_current != current_item) {
                dm.a = 1.0;
                dm.b = 0.0;
            }
            ImGui::InputDouble("a", &dm.a);
            ImGui::InputDouble("b", &dm.b);
            break;

        case distribution::fisher_f:
            if (old_current != current_item) {
                dm.m = 1.0;
                dm.n = 1.0;
            }
            ImGui::InputDouble("m", &dm.m);
            ImGui::InputDouble("s", &dm.n);
            break;

        case distribution::student_t:
            if (old_current != current_item) {
                dm.n = 1.0;
            }
            ImGui::InputDouble("n", &dm.n);
            break;
        }

        std::mt19937_64 dist(1024);

        if (dm.is_running) {
            ImGui::Text("File generation in progress %.2f", dm.status);

            if (dm.status < 0.0 || dm.status >= 100.0) {
                dm.job.join();
                dm.status = 0.0;
                dm.is_running = false;
                dm.os.close();
            }
        } else if (current_item >= 0) {
            if (ImGui::Button("Generate"))
                show_file_dialog = true;

            if (show_file_dialog) {
                const char* title = "Select file path to save";
                const char8_t* filters[] = { u8".dat", nullptr };

                ImGui::OpenPopup(title);
                std::filesystem::path path;
                if (save_file_dialog(path, title, filters)) {
                    show_file_dialog = false;

                    log_w.log(5,
                              "Save random generated file to %s\n",
                              (const char*)path.u8string().c_str());

                    if (dm.os = std::ofstream(path); dm.os.is_open())
                        dm.start(dist,
                                 size,
                                 use_binary
                                   ? irt::source::random_file_type::binary
                                   : irt::source::random_file_type::text);
                }
            }
        }
    }
}

static void
size_in_bytes(const sources& src) noexcept
{
    constexpr sz K = 1024u;
    constexpr sz M = K * 1024u;
    constexpr sz G = M * 1024u;

    const sz c = src.csts.size() * sizeof(irt::source::constant) +
                 src.bins.size() * sizeof(irt::source::binary_file) +
                 src.texts.size() * sizeof(irt::source::text_file);

    if (c / G > 0)
        ImGui::Text("Memory usage: %f Gb", ((double)c / (double)G));
    else if (c / M > 0)
        ImGui::Text("Memory usage: %f Mb", ((double)c / (double)M));
    else
        ImGui::Text("Memory usage: %f Kb", ((double)c / (double)K));
}

void
sources::show(bool* is_show)
{
    ImGui::SetNextWindowPos(ImVec2(70, 450), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);

    static bool show_file_dialog = false;
    static irt::source::binary_file* binary_file_ptr = nullptr;
    static irt::source::text_file* text_file_ptr = nullptr;

    if (!ImGui::Begin("External sources", is_show)) {
        ImGui::End();
        return;
    }

    show_random();

    static ImGuiTableFlags flags =
      ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg |
      ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable |
      ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;

    if (ImGui::CollapsingHeader("List of constant sources")) {
        if (ImGui::BeginTable("Constant sources", 2, flags)) {
            ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("value",
                                    ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            small_string<16> label;
            static ImVector<int> selection;

            for (auto& elem : csts) {
                const bool item_is_selected = selection.contains(elem.first);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                fmt::format_to_n(
                  label.begin(), label.capacity(), "{}", elem.first);
                if (ImGui::Selectable(label.c_str(),
                                      item_is_selected,
                                      ImGuiSelectableFlags_AllowItemOverlap |
                                        ImGuiSelectableFlags_SpanAllColumns)) {
                    if (ImGui::GetIO().KeyCtrl) {
                        if (item_is_selected)
                            selection.find_erase_unsorted(elem.first);
                        else
                            selection.push_back(elem.first);
                    } else {
                        selection.clear();
                        selection.push_back(elem.first);
                    }
                }
                ImGui::TableNextColumn();
                ImGui::PushID(elem.first);
                ImGui::InputDouble("##cell", &elem.second.value);
                ImGui::PopID();
            }

            ImGui::EndTable();

            if (ImGui::Button("New constant source"))
                new_constant();

            ImGui::SameLine();
            if (ImGui::Button("Delete##constant")) {
                for (int i = 0, e = selection.size(); i != e; ++i)
                    csts.erase(selection[i]);

                selection.clear();
            }
        }
    }

    if (ImGui::CollapsingHeader("List of binary file sources")) {
        if (ImGui::BeginTable("Binary files sources", 2, flags)) {
            ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("path", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            small_string<16> label;
            static ImVector<int> selection;

            for (auto& elem : bins) {
                const bool item_is_selected = selection.contains(elem.first);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                fmt::format_to_n(
                  label.begin(), label.capacity(), "{}", elem.first);
                if (ImGui::Selectable(label.c_str(),
                                      item_is_selected,
                                      ImGuiSelectableFlags_AllowItemOverlap |
                                        ImGuiSelectableFlags_SpanAllColumns)) {
                    if (ImGui::GetIO().KeyCtrl) {
                        if (item_is_selected)
                            selection.find_erase_unsorted(elem.first);
                        else
                            selection.push_back(elem.first);
                    } else {
                        selection.clear();
                        selection.push_back(elem.first);
                    }
                }
                ImGui::TableNextColumn();
                ImGui::PushID(elem.first);
                ImGui::Text(elem.second.file_path.string().c_str());
                ImGui::PopID();
            }
            ImGui::EndTable();

            if (ImGui::Button("New binary source")) {
                binary_file_ptr = new_binary_file();
                show_file_dialog = true;
            }

            ImGui::SameLine();
            if (ImGui::Button("Delete##binary")) {
                for (int i = 0, e = selection.size(); i != e; ++i)
                    bins.erase(selection[i]);

                selection.clear();
            }
        }
    }

    if (ImGui::CollapsingHeader("List of text file sources")) {
        if (ImGui::BeginTable("Text files sources", 2, flags)) {
            ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("path", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            small_string<16> label;
            static ImVector<int> selection;

            for (auto& elem : texts) {
                const bool item_is_selected = selection.contains(elem.first);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                fmt::format_to_n(
                  label.begin(), label.capacity(), "{}", elem.first);
                if (ImGui::Selectable(label.c_str(),
                                      item_is_selected,
                                      ImGuiSelectableFlags_AllowItemOverlap |
                                        ImGuiSelectableFlags_SpanAllColumns)) {
                    if (ImGui::GetIO().KeyCtrl) {
                        if (item_is_selected)
                            selection.find_erase_unsorted(elem.first);
                        else
                            selection.push_back(elem.first);
                    } else {
                        selection.clear();
                        selection.push_back(elem.first);
                    }
                }
                ImGui::TableNextColumn();
                ImGui::Text(elem.second.file_path.string().c_str());
                ImGui::PushID(elem.first);
                ImGui::PopID();
            }
            ImGui::EndTable();

            if (ImGui::Button("New text source")) {
                text_file_ptr = new_text_file();
                show_file_dialog = true;
            }

            ImGui::SameLine();
            if (ImGui::Button("Delete##text")) {
                for (int i = 0, e = selection.size(); i != e; ++i)
                    texts.erase(selection[i]);

                selection.clear();
            }
        }
    }

    if (show_file_dialog) {
        if (binary_file_ptr) {
            const char* title = "Select file path to load";
            const char8_t* filters[] = { u8".dat", nullptr };

            ImGui::OpenPopup(title);
            if (load_file_dialog(binary_file_ptr->file_path, title, filters)) {
                show_file_dialog = false;
                binary_file_ptr = nullptr;
            }
        }

        if (text_file_ptr) {
            const char* title = "Select file path to load";
            const char8_t* filters[] = { u8".txt", nullptr };

            ImGui::OpenPopup(title);
            if (load_file_dialog(text_file_ptr->file_path, title, filters)) {
                show_file_dialog = false;
                text_file_ptr = nullptr;
            }
        }
    }

    size_in_bytes(*this);

    ImGui::End();
}

void
sources::show_menu(const char* title, external_source& src)
{
    small_string<16> tmp;
    std::pair<const int, source::constant>* constant_ptr = nullptr;
    std::pair<const int, source::binary_file>* binary_file_ptr = nullptr;
    std::pair<const int, source::text_file>* text_file_ptr = nullptr;

    if (ImGui::BeginPopup(title)) {
        if (!csts.empty() && ImGui::BeginMenu("Constant")) {
            for (auto& elem : csts) {
                fmt::format_to_n(tmp.begin(), tmp.capacity(), "{}", elem.first);
                if (ImGui::MenuItem(tmp.c_str())) {
                    constant_ptr = &elem;
                    break;
                }
            }
            ImGui::EndMenu();
        }

        if (!bins.empty() && ImGui::BeginMenu("Binary files")) {
            for (auto& elem : bins) {
                fmt::format_to_n(tmp.begin(), tmp.capacity(), "{}", elem.first);
                if (ImGui::MenuItem(tmp.c_str())) {
                    binary_file_ptr = &elem;
                    break;
                }
            }
            ImGui::EndMenu();
        }

        if (!texts.empty() && ImGui::BeginMenu("Text files")) {
            for (auto& elem : texts) {
                fmt::format_to_n(tmp.begin(), tmp.capacity(), "{}", elem.first);
                if (ImGui::MenuItem(tmp.c_str())) {
                    text_file_ptr = &elem;
                    break;
                }
            }
            ImGui::EndMenu();
        }

        if (constant_ptr != nullptr) {
            constant_ptr->second.init(src);
            src.id = constant_ptr->first;
            constant_ptr = nullptr;
        }

        if (binary_file_ptr != nullptr) {
            binary_file_ptr->second.init(src);
            src.id = binary_file_ptr->first;
            binary_file_ptr = nullptr;
        }

        if (text_file_ptr != nullptr) {
            text_file_ptr->second.init(src);
            src.id = text_file_ptr->first;
            text_file_ptr = nullptr;
        }

        ImGui::EndPopup();
    }
}

} // namespace irt
