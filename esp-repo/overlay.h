// language: C++, file: overlay.h, target: Windows 10/11 x64, MSVC
#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dwmapi.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#pragma comment(lib, "dwmapi.lib")

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
        wc.lpszClassName = L"SharkESPOverlayClass";
        RegisterClassExW(&wc);

        // Tạo cửa sổ overlay tràn màn hình, trong suốt hoàn toàn, không có viền
        hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
            wc.lpszClassName, L"sharkESP Overlay",
            WS_POPUP,
            0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
            nullptr, nullptr, hInst, nullptr
        );

        if (!hwnd) return false;

        // Sử dụng DWM BlurBehind hoặc ExtendFrameIntoClientArea để tạo độ trong suốt tuyệt đối chuẩn DX11
        MARGINS margin = { -1 };
        DwmExtendFrameIntoClientArea(hwnd, &margin);

        ShowWindow(hwnd, SW_SHOW);

        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 2;
        sd.BufferDesc.Width = GetSystemMetrics(SM_CXSCREEN);
        sd.BufferDesc.Height = GetSystemMetrics(SM_CYSCREEN);
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate = { 60, 1 };
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
        D3D_FEATURE_LEVEL obtainedLevel;

        if (FAILED(D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            levels, 2, D3D11_SDK_VERSION, &sd, &swap, &dev, &obtainedLevel, &ctx)))
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
        while (PeekMessageW(&msg, hwnd, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void end() {
        ImGui::Render();
        // CỰC KỲ QUAN TRỌNG: Alpha = 0.f giúp xóa sạch khung hình cũ thành trong suốt, khắc phục triệt để màn hình đen
        const float clear_color[4] = { 0.f, 0.f, 0.f, 0.f };
        ctx->OMSetRenderTargets(1, &rtv, nullptr);
        ctx->ClearRenderTargetView(rtv, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        swap->Present(1, 0); // Vsync ON để giảm tải GPU
    }

private:
    static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
        if (ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return true;
        if (m == WM_DESTROY) {
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(h, m, w, l);
    }
};
