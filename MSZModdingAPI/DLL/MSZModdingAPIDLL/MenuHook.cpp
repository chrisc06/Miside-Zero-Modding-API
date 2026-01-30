#include <d3d11.h>
#include <windows.h>
#include "MinHook.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include "API.h"

// --- Typedefs for the functions we are hooking ---
typedef HRESULT(__stdcall* Present_t)(IDXGISwapChain*, UINT, UINT);
typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);

// --- Globals ---
Present_t oPresent = nullptr;
WNDPROC oWndProc = nullptr;
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dContext = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
HWND window = nullptr;
bool g_Init = false;
bool g_ShowMenu = false;

// Forward declaration of ImGui's input handler
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// --- 1. Input Hook (WndProc) ---
// This lets ImGui see your mouse clicks and keyboard presses
LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (g_ShowMenu) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
            return true;

        // If menu is open, block mouse clicks from reaching the game
        // (Optional: remove this if you want to shoot while menu is open)
        if (uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONUP || uMsg == WM_MOUSEMOVE)
            return 1;
    }

    // Toggle Menu Key (INSERT)
    if (uMsg == WM_KEYDOWN && wParam == VK_INSERT) {
        g_ShowMenu = !g_ShowMenu;

        // Toggle Mouse Cursor
        // (You might need your MSZ_API::UI::ToggleCursor here if ImGui doesn't handle it)
    }

    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

// --- 2. Graphics Hook (Present) ---
// This runs every single frame (e.g., 60 times a second)
HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (!g_Init) {
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_pd3dDevice))) {
            g_pd3dDevice->GetImmediateContext(&g_pd3dContext);

            // Get the Window Handle (needed for Input)
            DXGI_SWAP_CHAIN_DESC sd;
            pSwapChain->GetDesc(&sd);
            window = sd.OutputWindow;

            // Create the Render Target (Where we draw the menu)
            ID3D11Texture2D* pBackBuffer;
            pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
            g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
            pBackBuffer->Release();

            // Hook Input
            oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);

            // Init ImGui
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange; // Let game handle cursor style

            ImGui_ImplWin32_Init(window);
            ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);

            g_Init = true;
        }
        else {
            return oPresent(pSwapChain, SyncInterval, Flags);
        }
    }

    // --- DRAW IMGUI ---
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::Render();
    g_pd3dContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    return oPresent(pSwapChain, SyncInterval, Flags);
}

// --- 3. The "Dummy Device" Trick ---
// Finds the address of 'Present' so MinHook can hook it
uintptr_t GetD3D11PresentAddress() {
    // create temporary window
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, DefWindowProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "DX11 Dummy", NULL };
    RegisterClassEx(&wc);
    HWND hWnd = CreateWindow("DX11 Dummy", "DX11 Dummy", WS_OVERLAPPEDWINDOW, 100, 100, 300, 300, NULL, NULL, wc.hInstance, NULL);

    // create temporary device
    D3D_FEATURE_LEVEL requestedLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };
    D3D_FEATURE_LEVEL obtainedLevel;
    ID3D11Device* d3dDevice = nullptr;
    ID3D11DeviceContext* d3dContext = nullptr;
    IDXGISwapChain* d3dSwapChain = nullptr;

    DXGI_SWAP_CHAIN_DESC scd;
    ZeroMemory(&scd, sizeof(scd));
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    scd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    scd.OutputWindow = hWnd;
    scd.SampleDesc.Count = 1;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    scd.Windowed = ((GetWindowLongPtr(hWnd, GWL_STYLE) & WS_POPUP) != 0) ? false : true;

    if (FAILED(D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, requestedLevels, sizeof(requestedLevels) / sizeof(D3D_FEATURE_LEVEL),
        D3D11_SDK_VERSION, &scd, &d3dSwapChain, &d3dDevice, &obtainedLevel, &d3dContext)))
    {
        DestroyWindow(hWnd);
        UnregisterClass("DX11 Dummy", wc.hInstance);
        return 0;
    }

    // Get the address of 'Present' from the SwapChain's VTable (Index 8)
    void** vTable = *reinterpret_cast<void***>(d3dSwapChain);
    uintptr_t presentAddr = reinterpret_cast<uintptr_t>(vTable[8]);

    // Cleanup
    d3dSwapChain->Release();
    d3dDevice->Release();
    d3dContext->Release();
    DestroyWindow(hWnd);
    UnregisterClass("DX11 Dummy", wc.hInstance);

    return presentAddr;
}

// --- 4. Initialization ---
// Call this from your DllMain thread
void StartImGui() {
    uintptr_t presentAddr = GetD3D11PresentAddress();
    if (!presentAddr) {
        LogE("Failed to find D3D11 Present address!");
        return;
    }

    if (MH_CreateHook((LPVOID)presentAddr, &hkPresent, (LPVOID*)&oPresent) == MH_OK) {
        MH_EnableHook((LPVOID)presentAddr);
        LogI("ImGui Hooked Successfully via MinHook!");
    }
    else {
        LogE("MinHook failed to hook Present!");
    }
}