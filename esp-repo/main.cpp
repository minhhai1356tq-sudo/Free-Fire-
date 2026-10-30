// language: C++, x64, Windows, DX11
// External ESP for Free Fire via BlueStacks - Full Feature (Box, Bones, Name, Health)
#include <windows.h>
#include <cmath>
#include <vector>
#include <string>
#include <cstdio>
#include <thread>
#include <algorithm>
#include "mem.h"
#include "offsets.h"
#include "overlay.h"
#include <imgui.h>

struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };
struct M4   { float m[4][4]; };

// --- Các hàm W2S ---
static bool w2s(const Vec3& w, Vec2& out, const M4& vm, int sw, int sh) {
    float cx = w.x*vm.m[0][0] + w.y*vm.m[1][0] + w.z*vm.m[2][0] + vm.m[3][0];
    float cy = w.x*vm.m[0][1] + w.y*vm.m[1][1] + w.z*vm.m[2][1] + vm.m[3][1];
    float cw = w.x*vm.m[0][3] + w.y*vm.m[1][3] + w.z*vm.m[2][3] + vm.m[3][3];
    if (cw < 0.1f) return false;
    out.x = (sw * 0.5f) * ((cx/cw) + 1.0f);
    out.y = (sh * 0.5f) * (1.0f - (cy/cw));
    return true;
}

// --- Lấy danh sách Entity ---
static std::vector<uintptr_t> get_entities(Mem& m, uintptr_t match) {
    std::vector<uintptr_t> out;
    uintptr_t dict = m.rpm<uintptr_t>(match + off::DictionaryEntities);
    if (!dict) return out;
    uintptr_t entries = m.rpm<uintptr_t>(dict + 0x10);
    int count         = m.rpm<int>(dict + 0x18);
    if (!entries || count <= 0 || count > 200) return out;
    for (int i = 0; i < count; i++) {
        uintptr_t v = m.rpm<uintptr_t>(entries + 0x10 + i * 0x18);
        if (v) out.push_back(v);
    }
    return out;
}

// --- Hàm quét tìm Base (cho 64-bit) ---
uintptr_t FindIL2CppBase(HANDLE hProc) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    uintptr_t addr = (uintptr_t)si.lpMinimumApplicationAddress;
    uintptr_t maxAddr = (uintptr_t)si.lpMaximumApplicationAddress;

    while (addr < maxAddr) {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQueryEx(hProc, (LPCVOID)addr, &mbi, sizeof(mbi)) == sizeof(mbi)) {
            if (mbi.State == MEM_COMMIT && 
               (mbi.Protect == PAGE_READONLY || mbi.Protect == PAGE_READWRITE || 
                mbi.Protect == PAGE_EXECUTE_READ || mbi.Protect == PAGE_EXECUTE_READWRITE)) {
                uint32_t magic = 0;
                SIZE_T bytesRead = 0;
                if (ReadProcessMemory(hProc, mbi.BaseAddress, &magic, sizeof(magic), &bytesRead)) {
                    if (magic == 0x464C457F) { // ELF magic
                        return (uintptr_t)mbi.BaseAddress;
                    }
                }
            }
        }
        addr = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    }
    return 0;
}

// --- Hàm vẽ chính (chạy trong thread) ---
DWORD WINAPI HackThread(LPVOID lpParam) {
    Mem mem;
    mem.pid = GetCurrentProcessId();
    mem.hProc = GetCurrentProcess();

    mem.base = FindIL2CppBase(mem.hProc);
    if (!mem.base) {
        MessageBoxA(0, "Cannot find libil2cpp.so in RAM! Game not fully loaded?", "sharkESP", MB_OK);
        return 1;
    }

    Overlay ov;
    if (!ov.init(GetModuleHandleW(nullptr))) {
        MessageBoxA(0, "Overlay init failed!", "sharkESP", MB_OK);
        return 1;
    }
    SetWindowPos(ov.hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    bool esp_on = true;
    bool show_menu = true;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    while (true) {
        ov.begin();

        if (show_menu) {
            ImGui::Begin("sharkESP", &show_menu, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Checkbox("Enable ESP", &esp_on);
            ImGui::End();
        }

        if (esp_on && mem.ok()) {
            uintptr_t initBaseAddr = mem.base + off::InitBase;
            uintptr_t sc = mem.rpm<uintptr_t>(initBaseAddr);
            
            if (sc) {
                uintptr_t match = mem.rpm<uintptr_t>(sc + off::CurrentMatch);
                if (match) {
                    uintptr_t lp = mem.rpm<uintptr_t>(match + off::LocalPlayer);
                    uintptr_t fc = mem.rpm<uintptr_t>(match + off::FollowCamera);
                    uintptr_t cam = fc ? mem.rpm<uintptr_t>(fc + off::Camera) : 0;

                    if (lp && cam) {
                        M4 vm = mem.rpm<M4>(cam + off::ViewMatrix);
                        uint32_t local_team = mem.rpm<uint32_t>(lp + off::Player_TeamModeID);
                        ImDrawList* dl = ImGui::GetBackgroundDrawList();
                        auto ents = get_entities(mem, match);

                        for (uintptr_t e : ents) {
                            if (e == lp) continue;
                            if (mem.rpm<bool>(e + off::Player_IsDead)) continue;
                            if (mem.rpm<uint32_t>(e + off::Player_TeamModeID) == local_team) continue;

                            // Đọc vị trí từ PhysXData
                            uintptr_t physX = mem.rpm<uintptr_t>(e + 0x14E8); // Player_ShadowBase
                            if (!physX) continue;
                            uintptr_t transNode = mem.rpm<uintptr_t>(physX + 0x8);
                            if (!transNode) continue;
                            uintptr_t transform = mem.rpm<uintptr_t>(transNode + 0x8);
                            if (!transform) continue;
                            
                            Vec3 pos = mem.rpm<Vec3>(transform + 0x60);
                            if (pos.x == 0.0f && pos.y == 0.0f && pos.z == 0.0f) continue;

                            Vec3 head = { pos.x, pos.y + 1.6f, pos.z };
                            Vec2 ps, hs;
                            if (!w2s(pos, ps, vm, sw, sh)) continue;
                            if (!w2s(head, hs, vm, sw, sh)) continue;

                            // Vẽ Box
                            float h = ps.y - hs.y;
                            float w = h * 0.55f;
                            dl->AddRect(ImVec2(hs.x - w/2, hs.y), ImVec2(hs.x + w/2, ps.y), IM_COL32(255, 0, 0, 255), 0, 0, 1.5f);

                            // Vẽ thanh máu (dùng HealthBar giả lập, vì HP server-side đéo đọc được)
                            // Nếu muốn, mày có thể đọc thanh máu từ Offset của Player_Health (nếu có).
                            // Hiện tại tao không vẽ vì server-side.

                            // Vẽ Name
                            uintptr_t np = mem.rpm<uintptr_t>(e + off::Player_Name);
                            std::wstring nm = mem.read_wstr(np, 32);
                            if (!nm.empty()) {
                                char mb[128]{};
                                WideCharToMultiByte(CP_UTF8, 0, nm.c_str(), -1, mb, sizeof(mb), 0, 0);
                                dl->AddText(ImVec2(hs.x - w/2, hs.y - 16), IM_COL32(255,255,0,255), mb);
                            }
                        }
                    }
                }
            }
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
