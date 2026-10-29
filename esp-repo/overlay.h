#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <dxgi.h>
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx11.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

class Overlay {
public:
    HWND hwnd = nullptr;
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    IDXGISwapChain* swap = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;

    bool init(HINSTANCE hInst) {
        WNDCLASSEXW wc{ sizeof(wc) };
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"SharkESPOverlay";
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        RegisterClassExW(&wc);

        hwnd = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            L"SharkESPOverlay", L"sharkESP",
            WS_POPUP,
            0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
            nullptr, nullptr, hInst, nullptr);

        SetLayeredWindowAttributes(hwnd, RGB(0,0,0), 0, LWA_COLORKEY);
        ShowWindow(hwnd, SW_SHOW);

        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 2;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate = { 60, 1 };
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                nullptr, 0, D3D11_SDK_VERSION, &sd, &swap, &dev, nullptr, &ctx)))
            return false;

        ID3D11Texture2D* back = nullptr;
        swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back);
        if (!back) return false;
        dev->CreateRenderTargetView(back, nullptr, &rtv);
        back->Release();

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX11_Init(dev, ctx);
        return true;
    }

    void begin() {
        MSG msg;
        while (PeekMessageW(&msg, hwnd, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void end() {
        ImGui::Render();
        const float clear[4] = { 0, 0, 0, 0 };
        ctx->OMSetRenderTargets(1, &rtv, nullptr);
        ctx->ClearRenderTargetView(rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        swap->Present(0, 0);
    }

private:
    static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
        if (ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return true;
        if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
        return DefWindowProcW(h, m, w, l);
    }
};
