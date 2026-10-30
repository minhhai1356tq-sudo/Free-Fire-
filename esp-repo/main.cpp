// language: C++, file: main.cpp, target: Windows 10/11 x64, MSVC
#include <windows.h>
#include <string>
#include <thread>
#include <tlhelp32.h>
#include <imgui.h>
#include "overlay.h"
#include "mem.h"

// Hàm tìm PID của process theo tên (Unicode)
DWORD GetProcessIdByName(const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    
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

// Hàm tiêm DLL vào target process an toàn
bool InjectDLL(DWORD pid, const std::string& dllPath) {
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) return false;

    size_t pathSize = dllPath.size() + 1;
    LPVOID pRemoteMem = VirtualAllocEx(hProc, nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemoteMem) {
        CloseHandle(hProc);
        return false;
    }

    if (!WriteProcessMemory(hProc, pRemoteMem, dllPath.c_str(), pathSize, nullptr)) {
        VirtualFreeEx(hProc, pRemoteMem, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return false;
    }

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

// Luồng thực thi chính của Hack/Loader
DWORD WINAPI HackThread(LPVOID lpParam) {
    Overlay ov;
    if (!ov.init(GetModuleHandleW(nullptr))) {
        MessageBoxA(0, "Overlay init failed!", "sharkESP Loader", MB_OK);
        return 1;
    }

    bool show_menu = true;

    while (true) {
        if (ov.hwnd) {
            LONG exStyle = GetWindowLong(ov.hwnd, GWL_EXSTYLE);
            if (show_menu) {
                SetWindowLong(ov.hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_TRANSPARENT);
            } else {
                SetWindowLong(ov.hwnd, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT);
            }
        }

        ov.begin();

        if (show_menu) {
            ImGui::Begin("sharkESP Loader - BlueStacks x64", &show_menu, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("Status: Connected & Ready.");
            
            if (ImGui::Button("Inject to Game (AndroidProcess.exe)")) {
                DWORD pid = GetProcessIdByName(L"AndroidProcess.exe");
                if (pid == 0) {
                    pid = GetProcessIdByName(L"BstkVM.exe");
                }
                
                if (pid != 0) {
                    std::string dllPath = "esp.dll"; 
                    if (InjectDLL(pid, dllPath)) {
                        MessageBoxA(0, "Injected esp.dll successfully!", "Success", MB_OK);
                    } else {
                        MessageBoxA(0, "Injection failed! Check DLL architecture.", "Error", MB_OK);
                    }
                } else {
                    MessageBoxA(0, "AndroidProcess.exe not found! Start Free Fire first.", "Error", MB_OK);
                }
            }
            ImGui::End();
        }

        ov.end();
        Sleep(10);
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
