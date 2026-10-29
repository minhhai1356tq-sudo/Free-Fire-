#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <cstdint>
#include <cwchar>

struct Mem {
    HANDLE hProc = nullptr;
    DWORD  pid   = 0;
    uintptr_t base = 0;

    bool attach(const wchar_t* name) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32W pe{ sizeof(pe) };
        if (Process32FirstW(snap, &pe)) {
            do {
                if (!_wcsicmp(pe.szExeFile, name)) { pid = pe.th32ProcessID; break; }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
        if (!pid) return false;

        hProc = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
                            FALSE, pid);
        if (!hProc) return false;

        HANDLE ms = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        MODULEENTRY32W me{ sizeof(me) };
        if (Module32FirstW(ms, &me)) {
            do {
                if (!_wcsicmp(me.szModule, L"libil2cpp.so")) {
                    base = (uintptr_t)me.modBaseAddr;
                    break;
                }
            } while (Module32NextW(ms, &me));
        }
        CloseHandle(ms);
        return base != 0;
    }

    template<typename T>
    T rpm(uintptr_t addr) {
        T v{};
        if (!hProc) return v;
        ReadProcessMemory(hProc, (LPCVOID)addr, &v, sizeof(T), nullptr);
        return v;
    }

    std::wstring read_wstr(uintptr_t addr, size_t maxlen = 64) {
        std::wstring out;
        if (!addr) return out;
        wchar_t buf[128]{};
        ReadProcessMemory(hProc, (LPCVOID)addr, buf, sizeof(buf) - sizeof(wchar_t), nullptr);
        buf[maxlen] = L'\0';
        out.assign(buf);
        return out;
    }

    bool ok() const { return hProc && base; }
};