#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx11.h"

#include <windows.h>
#include <d3d11.h>
#include "MinHook.h" 
#include "API.h"
#include "Hook.h"
#include "CrashHandler.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "user32.lib")

// --- GLOBAL VARS ---
typedef HRESULT(__stdcall* Present_t)(IDXGISwapChain*, UINT, UINT);
typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);

Present_t oPresent = nullptr;
WNDPROC oWndProc = nullptr;
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dContext = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
HWND window = nullptr;
bool g_Init = false;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

    if (MSZ_API::UI::IsMenuOpen()) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
            return true;
    }

    if (uMsg == WM_KEYDOWN && wParam == VK_INSERT) {
        MSZ_API::UI::ToggleMenu();
    }

    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (!g_Init) {
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_pd3dDevice))) {
            g_pd3dDevice->GetImmediateContext(&g_pd3dContext);
            DXGI_SWAP_CHAIN_DESC sd;
            pSwapChain->GetDesc(&sd);
            window = sd.OutputWindow;

            ID3D11Texture2D* pBackBuffer;
            pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
            g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
            pBackBuffer->Release();

            oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);

            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;

            ImGui::StyleColorsDark();
            ImGui_ImplWin32_Init(window);
            ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);
            g_Init = true;
        }
        else return oPresent(pSwapChain, SyncInterval, Flags);
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (MSZ_API::UI::IsMenuOpen()) {
        ImGui::GetIO().MouseDrawCursor = true;
    }
    else {
        ImGui::GetIO().MouseDrawCursor = false;
    }

    MSZ_API::UI::Internal::RenderAll();

    ImGui::Render();
    g_pd3dContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    return oPresent(pSwapChain, SyncInterval, Flags);
}
uintptr_t GetD3D11PresentAddress() {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, DefWindowProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "DX11 Dummy", NULL };
    RegisterClassEx(&wc);
    HWND hWnd = CreateWindow("DX11 Dummy", "DX11 Dummy", WS_OVERLAPPEDWINDOW, 100, 100, 300, 300, NULL, NULL, wc.hInstance, NULL);

    D3D_FEATURE_LEVEL requestedLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };
    D3D_FEATURE_LEVEL obtainedLevel;
    ID3D11Device* d3dDevice = nullptr;
    ID3D11DeviceContext* d3dContext = nullptr;
    IDXGISwapChain* d3dSwapChain = nullptr;

    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hWnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, requestedLevels, 2, D3D11_SDK_VERSION, &scd, &d3dSwapChain, &d3dDevice, &obtainedLevel, &d3dContext);

    void** vTable = *reinterpret_cast<void***>(d3dSwapChain);
    uintptr_t presentAddr = reinterpret_cast<uintptr_t>(vTable[8]);

    d3dSwapChain->Release();
    d3dDevice->Release();
    d3dContext->Release();
    DestroyWindow(hWnd);
    UnregisterClass("DX11 Dummy", wc.hInstance);

    return presentAddr;
}

void InitDirectXHook() {
    uintptr_t presentAddr = GetD3D11PresentAddress();
    if (presentAddr) {
        if (MH_CreateHook((LPVOID)presentAddr, &hkPresent, (LPVOID*)&oPresent) == MH_OK) {
            MH_EnableHook((LPVOID)presentAddr);
            LogI("DirectX Present Hooked.");
        }
        else {
            LogE("MinHook failed to hook Present.");
        }
    }
    else {
        LogE("Failed to find Present Address.");
    }
}
void InitConsole() {
    if (!AllocConsole()) return;
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
    freopen_s(&f, "CONIN$", "r", stdin);
    std::cout.clear();
    SetConsoleTitleA("MSZ Modding API - Debug Console");
}

void MainThread(HMODULE hModule) {
    InitConsole();

    uintptr_t gameAssemblyBase = (uintptr_t)GetModuleHandleA("GameAssembly.dll");
    if (gameAssemblyBase) {
        LogI("GameAssembly found.");
        Hook::InitAll(gameAssemblyBase);
    }
    else {
        LogI("GameAssembly not found.");
        return;
    }

    InitDirectXHook();

    while (true) Sleep(100);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        InitCrashHandler();
        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, 0);
        break;
    case DLL_PROCESS_DETACH:
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        break;
    }
    return TRUE;
}