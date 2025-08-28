// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "imnodes.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include <cstdio>
#include <cstring>

#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif

#if defined(IRRITATOR_USE_TTF)
#include <fontconfig/fontconfig.h>
#endif

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to
// maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with
// legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a
// newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) &&                                 \
  !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

static void glfw_error_callback(int error, const char* description)
{
    fprintf(
      stderr,
      "Glfw Error %d: %s\nTry to use declare environment variables before "
      "running irritator. For example:\n$ MESA_GLSL_VERSION_OVERRIDE=450 env "
      "MESA_GL_VERSION_OVERRIDE=4.5COMPAT ./irritator\n",
      error,
      description);
}

#if defined(IRRITATOR_USE_TTF)
auto GetSystemFontFilePath(const char* font_name, FcConfig* config) noexcept
  -> std::optional<std::filesystem::path>
{
    using fcpattern_ptr =
      std::unique_ptr<FcPattern, decltype(FcPatternDestroy)*>;

    const auto*   str = reinterpret_cast<const FcChar8*>(font_name);
    fcpattern_ptr pattern{ FcNameParse(str), FcPatternDestroy };

    FcConfigSubstitute(config, pattern.get(), FcMatchPattern);
    FcDefaultSubstitute(pattern.get());

    FcResult res;

    if (fcpattern_ptr font{ FcFontMatch(config, pattern.get(), &res),
                            FcPatternDestroy };
        font.get()) {
        FcChar8* file = nullptr;
        if (FcPatternGetString(font.get(), FC_FILE, 0, &file) == FcResultMatch)
            return std::filesystem::path(reinterpret_cast<const char*>(file));
    }

    return std::nullopt;
}

auto GetSystemFontFile() noexcept -> std::optional<std::filesystem::path>
{
    using config_ptr = std::unique_ptr<FcConfig, decltype(FcConfigDestroy)*>;

    config_ptr config{ FcInitLoadConfigAndFonts(), FcConfigDestroy };

    if (auto ret = GetSystemFontFilePath("Roboto", config.get());
        ret.has_value()) {
        return ret.value();
    }

    if (auto ret = GetSystemFontFilePath("DejaVu Sans", config.get());
        ret.has_value()) {
        return ret.value();
    }

    return std::nullopt;
}
#endif

#if defined(IRRITATOR_ENABLE_DEBUG)
//! Detect if a process is being run under a debugger.
#if defined(__APPLE__)
static bool is_running_under_debugger() noexcept
{
    int               junk;
    int               mib[4];
    struct kinfo_proc info;
    size_t            size;

    info.kp_proc.p_flag = 0;
    mib[0]              = CTL_KERN;
    mib[1]              = KERN_PROC;
    mib[2]              = KERN_PROC_PID;
    mib[3]              = getpid();

    size = sizeof(info);
    junk = sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);

    return junk == 0 ? false : (info.kp_proc.p_flag & P_TRACED) != 0;
}
#elif defined(__linux__)
static bool is_running_under_debugger() noexcept
{
    constexpr char debug_string[] = "TracerPid:";

    char buf[4096];

    const auto status_fd = ::open("/proc/self/status", O_RDONLY);
    if (status_fd == -1)
        return false;

    const auto num_read = ::read(status_fd, buf, sizeof(buf) - 1);
    ::close(status_fd);

    if (num_read <= 0)
        return false;

    buf[num_read]             = '\0';
    const auto tracer_pid_ptr = ::strstr(buf, debug_string);
    if (!tracer_pid_ptr)
        return false;

    for (const char* it = tracer_pid_ptr + sizeof(debug_string) - 1;
         it <= buf + num_read;
         ++it) {
        if (::isspace(*it))
            continue;
        else
            return ::isdigit(*it) != 0 && *it != '0';
    }

    return false;
}
#else
static bool is_running_under_debugger() noexcept
{
    bool under_debugger = false;

    if (ptrace(PTRACE_TRACEME, 0, 1, 0) < 0) {
        std::printf("Debugger detected. Enabling breakpoint\n");
        under_debugger = true;
    } else {
        ptrace(PTRACE_DETACH, 0, 1, 0);
    }

    return under_debugger;
}
#endif
#endif

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
#if defined(IRRITATOR_ENABLE_DEBUG)
    if (is_running_under_debugger())
        irt::on_error_callback = irt::debug::breakpoint;
#endif

    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+
    // only glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // 3.0+ only
#endif

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(1280, 720, "irritator", NULL, NULL);
    if (window == NULL)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io    = ImGui::GetIO();

    const auto init_filename       = irt::get_imgui_filename();
    const auto init_filename_u8str = init_filename.u8string();
    io.IniFilename = reinterpret_cast<const char*>(init_filename_u8str.c_str());

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

#ifdef IRRITATOR_USE_TTF
    io.Fonts->AddFontDefault();
    ImFont* ttf = nullptr;

    if (auto sans_serif_font = GetSystemFontFile(); sans_serif_font) {
        const auto u8str   = sans_serif_font.value().u8string();
        const auto c_u8str = u8str.c_str();
        const auto c_str   = reinterpret_cast<const char*>(c_u8str);

        ImFontConfig baseConfig;
        baseConfig.SizePixels  = 15.0f;
        baseConfig.PixelSnapH  = true;
        baseConfig.OversampleH = 2;
        baseConfig.OversampleV = 2;

        if (ttf = io.Fonts->AddFontFromFileTTF(
              c_str, baseConfig.SizePixels, &baseConfig);
            ttf)
            io.Fonts->Build();
    }
#endif

    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable
    // Keyboard Controls io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; //
    // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImNodes::CreateContext();

    {
        irt::journal_handler jn(irt::constrained_value<int, 4, INT_MAX>(256));
        irt::application     app(jn);

        if (!app.init()) {
            ImNodes::DestroyContext();

            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();

            glfwDestroyWindow(window);
            glfwTerminate();
            return EXIT_FAILURE;
        }

        // Load Fonts
        // - If no fonts are loaded, dear imgui will use the default font. You
        // can also load multiple fonts and use ImGui::PushFont()/PopFont() to
        // select them.
        // - AddFontFromFileTTF() will return the ImFont* so you can store it if
        // you need to select the font among multiple.
        // - If the file cannot be loaded, the function will return NULL. Please
        // handle those errors in your application (e.g. use an assertion, or
        // display an error and quit).
        // - The fonts will be rasterized at a given size (w/ oversampling) and
        // stored into a texture when calling
        // ImFontAtlas::Build()/GetTexDataAsXXXX(), which
        // ImGui_ImplXXXX_NewFrame below will call.
        // - Read 'misc/fonts/README.txt' for more instructions and details.
        // - Remember that in C/C++ if you want to include a backslash \ in a
        // string literal you need to write a double backslash \\ !
        // io.Fonts->AddFontDefault();
        // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
        // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
        // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
        // io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
        // ImFont* font =
        // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f,
        // NULL, io.Fonts->GetGlyphRangesJapanese()); IM_ASSERT(font != NULL);

        // bool show_demo_window = true;
        // bool show_another_window = false;
        ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

#ifdef IRRITATOR_USE_TTF
        if (ttf)
            ImGui::GetIO().FontDefault = ttf;
#endif

        // Main loop
        while (!glfwWindowShouldClose(window)) {
            // Poll and handle events (inputs, window resize, etc.)
            // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard
            // flags to tell if dear imgui wants to use your inputs.
            // - When io.WantCaptureMouse is true, do not dispatch mouse input
            // data to your main application.
            // - When io.WantCaptureKeyboard is true, do not dispatch keyboard
            // input data to your main application. Generally you may always
            // pass all inputs to dear imgui, and hide them from your
            // application based on those two flags.
            glfwPollEvents();

            // Start the Dear ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            if (app.show() == irt::application::show_result_t::request_to_close)
                glfwSetWindowShouldClose(window, GLFW_TRUE);

            // Rendering
            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(clear_color.x * clear_color.w,
                         clear_color.y * clear_color.w,
                         clear_color.z * clear_color.w,
                         clear_color.w);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
        }
    }

    ImNodes::DestroyContext();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
