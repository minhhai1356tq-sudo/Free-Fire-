// language: C++, x64, Windows, DX11
// Manual Loader with ImGui Menu + Injection Logic
#include <windows.h>
#include <cmath>
#include <vector>
#include <string>
#include <cstdio>
#include <thread>
#include <tlhelp32.h>
#include <TlHelp32.h>
#include "mem.h"
#include "offsets.h"
#include "overlay.h"
#include <imgui.h>

// Hàm tiêm DLL (Dùng cho cả 32-bit và 64-bit)
bool InjectDLL(DWORD pid, const std::string& dllPath) {
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) return false;

    size_t pathSize = dllPath.size() + 1;
    LPVOID pRemoteMem = VirtualAllocEx(hProc, nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemoteMem) { CloseHandle(hProc); return false; }

    if (!WriteProcessMemory(hProc, pRemoteMem, dllPath.c_str(), pathSize, nullptr)) {
        VirtualFreeEx(hProc, pRemoteMem, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return false;
    }

    // Lấy địa chỉ hàm LoadLibraryA. Lưu ý: Địa chỉ này phải khớp với kiến trúc của process đích.
    // Vì Loader của chúng ta là 64-bit và process đích (HD-Player) là 64-bit, nên lấy từ kernel32 của chính nó là đúng.
    // Tuy nhiên, nếu tiêm vào process 32-bit (AndroidProcess), cần cẩn thận. Cách chuẩn nhất là dùng CreateRemoteThread với LoadLibraryA từ kernel32 của process đích.
    // Tuy nhiên, do giới hạn khuôn khổ, tao dùng cách đơn giản này cho HD-Player (64-bit).
    
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    LPTHREAD_START_ROUTINE pLoadLibraryA = (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryA");

    HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0, pLoadLibraryA, pRemoteMem, 0, nullptr);
    if (!hThread) {
        VirtualFreeEx(hProc, pRemoteMem, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);
    VirtualFreeEx(hProc, pRemoteMem, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProc);
    return true;
}

// Hàm tìm PID của process theo tên
DWORD GetProcessIdByName(const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe{ sizeof(pe) };
    if (Process32FirstW(snap, &pe)) {
        do {
            if (!_wcsicmp(pe.szExeFile, name)) {
                CloseHandle(snap);
                return pe.th32ProcessID;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return 0;
}

// Hàm chạy chính (chạy trong thread của HD-Player)
DWORD WINAPI HackThread(LPVOID lpParam) {
    Overlay ov;
    if (!ov.init(GetModuleHandleW(nullptr))) {
        MessageBoxA(0, "Overlay init failed!", "sharkESP Loader", MB_OK);
        return 1;
    }
    SetWindowPos(ov.hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    bool show_menu = true;

    while (true) {
        ov.begin();

        if (show_menu) {
            ImGui::Begin("sharkESP Loader", &show_menu, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("BlueStacks injected successfully!");
            
            if (ImGui::Button("Inject to Game (AndroidProcess.exe)")) {
                DWORD pid = GetProcessIdByName(L"AndroidProcess.exe"); // Hoặc BstkVM.exe tùy bản
                if (pid == 0) {
                    // Thử tìm BstkVM.exe nếu không thấy
                    pid = GetProcessIdByName(L"BstkVM.exe");
                }
                
                if (pid != 0) {
                    // Đường dẫn tới file DLL ESP của mày (phải là bản 32-bit)
                    // Mày phải để file này cùng thư mục với cái loader
                    std::string dllPath = "esp.dll"; 
                    if (InjectDLL(pid, dllPath)) {
                        MessageBoxA(0, "Injected ESP successfully!", "Success", MB_OK);
                    } else {
                        MessageBoxA(0, "Injection failed! Check path or permissions.", "Error", MB_OK);
                    }
                } else {
                    MessageBoxA(0, "AndroidProcess.exe not found! Start Free Fire first.", "Error", MB_OK);
                }
            }
            
            ImGui::End();
        }

        ov.end();
        Sleep(1);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, HackThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
