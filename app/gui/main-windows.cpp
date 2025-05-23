// Dear ImGui: standalone example application for DirectX 12
// If you are new to Dear ImGui, read documentation from the docs/ folder + read
// the top of imgui.cpp. Read online:
// https://github.com/ocornut/imgui/tree/master/docs

// Important: to compile on 32-bit systems, the DirectX12 backend requires code
// to be compiled with '#define ImTextureID ImU64'. This is because we need
// ImTextureID to carry a 64-bit value and by default ImTextureID is defined as
// void*. This define is set in the example .vcxproj file and need to be
// replicated in your app or by adding it to your imconfig.h file.

#include "application.hpp"
#include "imnodes.h"

#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <tchar.h>

#define WIN32_LEAN_AND_MEAN
#include <wchar.h>
#include <windows.h>

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

struct FrameContext {
    ID3D12CommandAllocator* CommandAllocator;
    UINT64                  FenceValue;
};

// Data
static int const    NUM_FRAMES_IN_FLIGHT                 = 3;
static FrameContext g_frameContext[NUM_FRAMES_IN_FLIGHT] = {};
static UINT         g_frameIndex                         = 0;

static int const                  NUM_BACK_BUFFERS                  = 3;
static ID3D12Device*              g_pd3dDevice                      = NULL;
static ID3D12DescriptorHeap*      g_pd3dRtvDescHeap                 = NULL;
static ID3D12DescriptorHeap*      g_pd3dSrvDescHeap                 = NULL;
static ID3D12CommandQueue*        g_pd3dCommandQueue                = NULL;
static ID3D12GraphicsCommandList* g_pd3dCommandList                 = NULL;
static ID3D12Fence*               g_fence                           = NULL;
static HANDLE                     g_fenceEvent                      = NULL;
static UINT64                     g_fenceLastSignaledValue          = 0;
static IDXGISwapChain3*           g_pSwapChain                      = NULL;
static HANDLE                     g_hSwapChainWaitableObject        = NULL;
static ID3D12Resource* g_mainRenderTargetResource[NUM_BACK_BUFFERS] = {};
static D3D12_CPU_DESCRIPTOR_HANDLE
  g_mainRenderTargetDescriptor[NUM_BACK_BUFFERS] = {};

// Forward declarations of helper functions
bool           CreateDeviceD3D(HWND hWnd);
void           CleanupDeviceD3D();
void           CreateRenderTarget();
void           CleanupRenderTarget();
void           WaitForLastSubmittedFrame();
FrameContext*  WaitForNextFrameResources();
void           ResizeSwapChain(HWND hWnd, int width, int height);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#if defined(IRRITATOR_ENABLE_DEBUG)
//! Detect if a process is being run under a debugger.
static bool is_running_under_debugger() noexcept
{
    return ::IsDebuggerPresent() == TRUE;
}
#endif

#ifdef IRRITATOR_USE_TTF
auto GetSystemFontFile(std::wstring_view        font,
                       HKEY                     hKey,
                       std::shared_ptr<WCHAR[]> name,
                       std::shared_ptr<BYTE[]>  data,
                       const DWORD              name_size,
                       const DWORD              value_size) noexcept
  -> std::optional<std::wstring>
{
    HRESULT      result;
    std::wstring font_found;
    DWORD        index = 0;
    DWORD        type  = 0;

    do {
        font_found.clear();
        auto data_len = value_size;
        auto name_len = name_size;

        result = RegEnumValueW(hKey,
                               index++,
                               name.get(),
                               &name_len,
                               0,
                               &type,
                               data.get(),
                               &data_len);

        if (result != ERROR_SUCCESS || type != REG_SZ)
            continue;

        font_found.assign(name.get(), name_len);
        if (::_wcsnicmp(font.data(), font_found.c_str(), font.size()) == 0)
            return std::wstring(reinterpret_cast<WCHAR*>(data.get()), data_len);

    } while (result != ERROR_NO_MORE_ITEMS);

    return std::nullopt;
}

auto GetSystemFontFilePath(std::wstring_view font_name) noexcept
  -> std::optional<std::filesystem::path>
{
    std::wstring windir(MAX_PATH, L'\0');
    UINT         windir_len =
      ::GetWindowsDirectoryW(windir.data(), static_cast<UINT>(windir.size()));

    if (std::cmp_greater_equal(windir_len, static_cast<UINT>(windir.size()))) {
        windir.resize(windir_len + 1u, L'\0');
        windir_len = ::GetWindowsDirectoryW(windir.data(),
                                            static_cast<UINT>(windir.size()));
    }

    if (windir_len == 0)
        return std::nullopt;

    windir.resize(windir_len);

    try {
        std::filesystem::path ret(windir);
        ret /= L"Fonts";
        ret /= font_name;
        return ret;
    } catch (...) {
    }

    return std::nullopt;
}

auto GetSystemFontFile() noexcept -> std::optional<std::filesystem::path>
{
    constexpr static LPCWSTR font_reg_path =
      L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";
    HKEY hKey;

    if (auto result =
          RegOpenKeyExW(HKEY_LOCAL_MACHINE, font_reg_path, 0, KEY_READ, &hKey);
        result != ERROR_SUCCESS)
        return std::nullopt;

    DWORD max_name_size, max_value_size;
    if (auto result = RegQueryInfoKeyW(
          hKey, 0, 0, 0, 0, 0, 0, 0, &max_name_size, &max_value_size, 0, 0);
        result != ERROR_SUCCESS)
        return std::nullopt;

    auto buffer_name = std::make_shared<WCHAR[]>(max_name_size);
    auto buffer_data = std::make_shared<BYTE[]>(max_value_size);

    if (auto ret = GetSystemFontFile(L"Calibri (TrueType)",
                                     hKey,
                                     buffer_name,
                                     buffer_data,
                                     max_name_size,
                                     max_value_size);
        ret.has_value()) {
        RegCloseKey(hKey);
        return GetSystemFontFilePath(ret.value());
    }

    if (auto ret = GetSystemFontFile(L"Arial (TrueType)",
                                     hKey,
                                     buffer_name,
                                     buffer_data,
                                     max_name_size,
                                     max_value_size);
        ret.has_value()) {
        RegCloseKey(hKey);
        return GetSystemFontFilePath(ret.value());
    }

    RegCloseKey(hKey);
    return std::nullopt;
}
#endif

// Main code
int main(int, char**)
{
#if defined(IRRITATOR_ENABLE_DEBUG)
    if (is_running_under_debugger())
        irt::on_error_callback = irt::debug::breakpoint;
#endif

    // Create application window
    // ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEX wc = { sizeof(WNDCLASSEX),    CS_CLASSDC, WndProc, 0L,   0L,
                      GetModuleHandle(NULL), NULL,       NULL,    NULL, NULL,
                      _T("ImGui Example"),   NULL };
    ::RegisterClassEx(&wc);
    HWND hwnd = ::CreateWindow(wc.lpszClassName,
                               _T("Irritator"),
                               WS_OVERLAPPEDWINDOW,
                               100,
                               100,
                               1280,
                               800,
                               NULL,
                               NULL,
                               wc.hInstance,
                               NULL);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io    = ImGui::GetIO();
    io.IniFilename = irt::get_imgui_filename();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable
    // Keyboard Controls io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; //
    // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX12_Init(
      g_pd3dDevice,
      NUM_FRAMES_IN_FLIGHT,
      DXGI_FORMAT_R8G8B8A8_UNORM,
      g_pd3dSrvDescHeap,
      g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
      g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can
    // also load multiple fonts and use ImGui::PushFont()/PopFont() to select
    // them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you
    // need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please
    // handle those errors in your application (e.g. use an assertion, or
    // display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and
    // stored into a texture when calling
    // ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame
    // below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string
    // literal you need to write a double backslash \\ !
    // io.Fonts->AddFontDefault();
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    // ImFont* font =
    // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f,
    // NULL, io.Fonts->GetGlyphRangesJapanese()); IM_ASSERT(font != NULL);

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

    // Our state
    bool   show_demo_window    = false;
    bool   show_another_window = false;
    ImVec4 clear_color         = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    ImNodes::CreateContext();

    {
        irt::journal_handler jn(irt::constrained_value<int, 4, INT_MAX>(256));
        irt::application     app(jn);

        if (!app.init()) {
            ImNodes::DestroyContext();

            ImGui_ImplDX12_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();

            CleanupDeviceD3D();
            ::DestroyWindow(hwnd);
            ::UnregisterClass(wc.lpszClassName, wc.hInstance);
            return 0;
        }

#ifdef IRRITATOR_USE_TTF
        if (ttf)
            ImGui::GetIO().FontDefault = ttf;
#endif

        // Main loop
        MSG msg;
        ZeroMemory(&msg, sizeof(msg));
        while (msg.message != WM_QUIT) {
            // Poll and handle messages (inputs, window resize, etc.)
            // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard
            // flags to tell if dear imgui wants to use your inputs.
            // - When io.WantCaptureMouse is true, do not dispatch mouse
            // input data to your main application.
            // - When io.WantCaptureKeyboard is true, do not dispatch
            // keyboard input data to your main application. Generally you
            // may always pass all inputs to dear imgui, and hide them from
            // your application based on those two flags.
            if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
                continue;
            }

            // Start the Dear ImGui frame
            ImGui_ImplDX12_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            if (app.show() == irt::application::show_result_t::request_to_close)
                ::PostMessage(hwnd, WM_CLOSE, 0, 0);

            // 1. Show the big demo window (Most of the sample code is in
            // ImGui::ShowDemoWindow()! You can browse its code to learn
            // more about Dear ImGui!).
            if (show_demo_window)
                ImGui::ShowDemoWindow(&show_demo_window);

            // 2. Show a simple window that we create ourselves. We use a
            // Begin/End pair to created a named window.

            //{
            //    static float f = 0.0f;
            //    static int counter = 0;

            //    ImGui::Begin("Hello, world!"); // Create a window called
            //    "Hello,
            //                                   // world!" and append into
            //                                   it.

            //    ImGui::Text(
            //      "This is some useful text."); // Display some text (you
            //      can use
            //      a
            //                                    // format strings too)
            //    ImGui::Checkbox("Demo Window",
            //                    &show_demo_window); // Edit bools storing
            //                    our window
            //                                        // open/close state
            //    ImGui::Checkbox("Another Window", &show_another_window);

            //    ImGui::SliderFloat(
            //      "float",
            //      &f,
            //      0.0f,
            //      1.0f); // Edit 1 float using a slider from 0.0f to 1.0f
            //    ImGui::ColorEdit3(
            //      "clear color",
            //      (float*)&clear_color); // Edit 3 floats representing a
            //      color

            //    if (ImGui::Button(
            //          "Button")) // Buttons return true when clicked (most
            //          widgets
            //                     // return true when edited/activated)
            //        counter++;
            //    ImGui::SameLine();
            //    ImGui::Text("counter = %d", counter);

            //    ImGui::Text("Application average %.3f ms/frame (%.1f
            //    FPS)",
            //                1000.0f / ImGui::GetIO().Framerate,
            //                ImGui::GetIO().Framerate);
            //    ImGui::End();
            //}

            // 3. Show another simple window.
            if (show_another_window) {
                ImGui::Begin(
                  "Another Window",
                  &show_another_window); // Pass a pointer to our bool
                                         // variable (the window will have a
                                         // closing button that will clear
                                         // the bool when clicked)
                ImGui::Text("Hello from another window!");
                if (ImGui::Button("Close Me"))
                    show_another_window = false;
                ImGui::End();
            }

            // Rendering
            ImGui::Render();

            FrameContext* frameCtx = WaitForNextFrameResources();
            UINT backBufferIdx     = g_pSwapChain->GetCurrentBackBufferIndex();
            frameCtx->CommandAllocator->Reset();

            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type  = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource =
              g_mainRenderTargetResource[backBufferIdx];
            barrier.Transition.Subresource =
              D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
            g_pd3dCommandList->Reset(frameCtx->CommandAllocator, NULL);
            g_pd3dCommandList->ResourceBarrier(1, &barrier);

            // Render Dear ImGui graphics
            const float clear_color_with_alpha[4] = {
                clear_color.x * clear_color.w,
                clear_color.y * clear_color.w,
                clear_color.z * clear_color.w,
                clear_color.w
            };
            g_pd3dCommandList->ClearRenderTargetView(
              g_mainRenderTargetDescriptor[backBufferIdx],
              clear_color_with_alpha,
              0,
              NULL);
            g_pd3dCommandList->OMSetRenderTargets(
              1, &g_mainRenderTargetDescriptor[backBufferIdx], FALSE, NULL);
            g_pd3dCommandList->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(),
                                          g_pd3dCommandList);
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
            g_pd3dCommandList->ResourceBarrier(1, &barrier);
            g_pd3dCommandList->Close();

            g_pd3dCommandQueue->ExecuteCommandLists(
              1, (ID3D12CommandList* const*)&g_pd3dCommandList);

            g_pSwapChain->Present(1, 0); // Present with vsync
            // g_pSwapChain->Present(0, 0); // Present without vsync

            UINT64 fenceValue = g_fenceLastSignaledValue + 1;
            g_pd3dCommandQueue->Signal(g_fence, fenceValue);
            g_fenceLastSignaledValue = fenceValue;
            frameCtx->FenceValue     = fenceValue;
        }

        WaitForLastSubmittedFrame();
    }

    ImNodes::DestroyContext();

    // Cleanup
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();

    if (io.IniFilename) {
        auto* str = const_cast<char*>(io.IniFilename);
        std::free(str);
        io.IniFilename = nullptr;
    }

    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC1 sd;
    {
        ZeroMemory(&sd, sizeof(sd));
        sd.BufferCount = NUM_BACK_BUFFERS;
        sd.Width       = 0;
        sd.Height      = 0;
        sd.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.Flags       = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.SampleDesc.Count   = 1;
        sd.SampleDesc.Quality = 0;
        sd.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.AlphaMode          = DXGI_ALPHA_MODE_UNSPECIFIED;
        sd.Scaling            = DXGI_SCALING_STRETCH;
        sd.Stereo             = FALSE;
    }

    // [DEBUG] Enable debug interface
#ifdef DX12_ENABLE_DEBUG_LAYER
    ID3D12Debug* pdx12Debug = NULL;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pdx12Debug))))
        pdx12Debug->EnableDebugLayer();
#endif

    // Create device
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    if (D3D12CreateDevice(NULL, featureLevel, IID_PPV_ARGS(&g_pd3dDevice)) !=
        S_OK)
        return false;

    // [DEBUG] Setup debug interface to break on any warnings/errors
#ifdef DX12_ENABLE_DEBUG_LAYER
    if (pdx12Debug != NULL) {
        ID3D12InfoQueue* pInfoQueue = NULL;
        g_pd3dDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue));
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
        pInfoQueue->Release();
        pdx12Debug->Release();
    }
#endif

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors             = NUM_BACK_BUFFERS;
        desc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask                   = 1;
        if (g_pd3dDevice->CreateDescriptorHeap(
              &desc, IID_PPV_ARGS(&g_pd3dRtvDescHeap)) != S_OK)
            return false;

        SIZE_T rtvDescriptorSize =
          g_pd3dDevice->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
          g_pd3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < NUM_BACK_BUFFERS; i++) {
            g_mainRenderTargetDescriptor[i] = rtvHandle;
            rtvHandle.ptr += rtvDescriptorSize;
        }
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 1;
        desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (g_pd3dDevice->CreateDescriptorHeap(
              &desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap)) != S_OK)
            return false;
    }

    {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type                     = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask                 = 1;
        if (g_pd3dDevice->CreateCommandQueue(
              &desc, IID_PPV_ARGS(&g_pd3dCommandQueue)) != S_OK)
            return false;
    }

    for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
        if (g_pd3dDevice->CreateCommandAllocator(
              D3D12_COMMAND_LIST_TYPE_DIRECT,
              IID_PPV_ARGS(&g_frameContext[i].CommandAllocator)) != S_OK)
            return false;

    if (g_pd3dDevice->CreateCommandList(0,
                                        D3D12_COMMAND_LIST_TYPE_DIRECT,
                                        g_frameContext[0].CommandAllocator,
                                        NULL,
                                        IID_PPV_ARGS(&g_pd3dCommandList)) !=
          S_OK ||
        g_pd3dCommandList->Close() != S_OK)
        return false;

    if (g_pd3dDevice->CreateFence(
          0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)) != S_OK)
        return false;

    g_fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (g_fenceEvent == NULL)
        return false;

    {
        IDXGIFactory4*   dxgiFactory = NULL;
        IDXGISwapChain1* swapChain1  = NULL;
        if (CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)) != S_OK)
            return false;
        if (dxgiFactory->CreateSwapChainForHwnd(
              g_pd3dCommandQueue, hWnd, &sd, NULL, NULL, &swapChain1) != S_OK)
            return false;
        if (swapChain1->QueryInterface(IID_PPV_ARGS(&g_pSwapChain)) != S_OK)
            return false;
        swapChain1->Release();
        dxgiFactory->Release();
        g_pSwapChain->SetMaximumFrameLatency(NUM_BACK_BUFFERS);
        g_hSwapChainWaitableObject =
          g_pSwapChain->GetFrameLatencyWaitableObject();
    }

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) {
        g_pSwapChain->Release();
        g_pSwapChain = NULL;
    }
    if (g_hSwapChainWaitableObject != NULL) {
        CloseHandle(g_hSwapChainWaitableObject);
    }
    for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
        if (g_frameContext[i].CommandAllocator) {
            g_frameContext[i].CommandAllocator->Release();
            g_frameContext[i].CommandAllocator = NULL;
        }
    if (g_pd3dCommandQueue) {
        g_pd3dCommandQueue->Release();
        g_pd3dCommandQueue = NULL;
    }
    if (g_pd3dCommandList) {
        g_pd3dCommandList->Release();
        g_pd3dCommandList = NULL;
    }
    if (g_pd3dRtvDescHeap) {
        g_pd3dRtvDescHeap->Release();
        g_pd3dRtvDescHeap = NULL;
    }
    if (g_pd3dSrvDescHeap) {
        g_pd3dSrvDescHeap->Release();
        g_pd3dSrvDescHeap = NULL;
    }
    if (g_fence) {
        g_fence->Release();
        g_fence = NULL;
    }
    if (g_fenceEvent) {
        CloseHandle(g_fenceEvent);
        g_fenceEvent = NULL;
    }
    if (g_pd3dDevice) {
        g_pd3dDevice->Release();
        g_pd3dDevice = NULL;
    }

#ifdef DX12_ENABLE_DEBUG_LAYER
    IDXGIDebug1* pDebug = NULL;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug)))) {
        pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
        pDebug->Release();
    }
#endif
}

void CreateRenderTarget()
{
    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++) {
        ID3D12Resource* pBackBuffer = NULL;
        g_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
        g_pd3dDevice->CreateRenderTargetView(
          pBackBuffer, NULL, g_mainRenderTargetDescriptor[i]);
        g_mainRenderTargetResource[i] = pBackBuffer;
    }
}

void CleanupRenderTarget()
{
    WaitForLastSubmittedFrame();

    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
        if (g_mainRenderTargetResource[i]) {
            g_mainRenderTargetResource[i]->Release();
            g_mainRenderTargetResource[i] = NULL;
        }
}

void WaitForLastSubmittedFrame()
{
    FrameContext* frameCtx =
      &g_frameContext[g_frameIndex % NUM_FRAMES_IN_FLIGHT];

    UINT64 fenceValue = frameCtx->FenceValue;
    if (fenceValue == 0)
        return; // No fence was signaled

    frameCtx->FenceValue = 0;
    if (g_fence->GetCompletedValue() >= fenceValue)
        return;

    g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
    WaitForSingleObject(g_fenceEvent, INFINITE);
}

FrameContext* WaitForNextFrameResources()
{
    UINT nextFrameIndex = g_frameIndex + 1;
    g_frameIndex        = nextFrameIndex;

    HANDLE waitableObjects[]  = { g_hSwapChainWaitableObject, NULL };
    DWORD  numWaitableObjects = 1;

    FrameContext* frameCtx =
      &g_frameContext[nextFrameIndex % NUM_FRAMES_IN_FLIGHT];
    UINT64 fenceValue = frameCtx->FenceValue;
    if (fenceValue != 0) // means no fence was signaled
    {
        frameCtx->FenceValue = 0;
        g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
        waitableObjects[1] = g_fenceEvent;
        numWaitableObjects = 2;
    }

    WaitForMultipleObjects(numWaitableObjects, waitableObjects, TRUE, INFINITE);

    return frameCtx;
}

void ResizeSwapChain(HWND hWnd, int width, int height)
{
    DXGI_SWAP_CHAIN_DESC1 sd;
    g_pSwapChain->GetDesc1(&sd);
    sd.Width  = width;
    sd.Height = height;

    IDXGIFactory4* dxgiFactory = NULL;
    g_pSwapChain->GetParent(IID_PPV_ARGS(&dxgiFactory));

    g_pSwapChain->Release();
    CloseHandle(g_hSwapChainWaitableObject);

    IDXGISwapChain1* swapChain1 = NULL;
    dxgiFactory->CreateSwapChainForHwnd(
      g_pd3dCommandQueue, hWnd, &sd, NULL, NULL, &swapChain1);
    swapChain1->QueryInterface(IID_PPV_ARGS(&g_pSwapChain));
    swapChain1->Release();
    dxgiFactory->Release();

    g_pSwapChain->SetMaximumFrameLatency(NUM_BACK_BUFFERS);

    g_hSwapChainWaitableObject = g_pSwapChain->GetFrameLatencyWaitableObject();
    assert(g_hSwapChainWaitableObject != NULL);
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND   hWnd,
                                                             UINT   msg,
                                                             WPARAM wParam,
                                                             LPARAM lParam);

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED) {
            WaitForLastSubmittedFrame();
            ImGui_ImplDX12_InvalidateDeviceObjects();
            CleanupRenderTarget();
            ResizeSwapChain(hWnd, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam));
            CreateRenderTarget();
            ImGui_ImplDX12_CreateDeviceObjects();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
