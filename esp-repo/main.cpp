// language: C++, x86, Windows, DX11
#include <windows.h>
#include <cmath>
#include <vector>
#include <string>
#include <cstdio>
#include <thread>
#include "mem.h"
#include "offsets.h"
#include "overlay.h"

struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };
struct M4   { float m[4][4]; };

static bool w2s(const Vec3& w, Vec2& out, const M4& vm, int sw, int sh) {
    float cx = w.x*vm.m[0][0] + w.y*vm.m[1][0] + w.z*vm.m[2][0] + vm.m[3][0];
    float cy = w.x*vm.m[0][1] + w.y*vm.m[1][1] + w.z*vm.m[2][1] + vm.m[3][1];
    float cw = w.x*vm.m[0][3] + w.y*vm.m[1][3] + w.z*vm.m[2][3] + vm.m[3][3];
    if (cw < 0.1f) return false;
    out.x = (sw * 0.5f) * ((cx/cw) + 1.0f);
    out.y = (sh * 0.5f) * (1.0f - (cy/cw));
    return true;
}

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

DWORD WINAPI HackThread(LPVOID lpParam) {
    AllocConsole();
    FILE* f; freopen_s(&f, "CONOUT$", "w", stdout);
    SetConsoleTitleA("sharkESP log");

    printf("[*] Injected. Attaching...\n");
    Mem mem;
    // Vì đã nằm trong process, attach chính nó
    mem.pid = GetCurrentProcessId();
    mem.hProc = GetCurrentProcess();

    // Lấy base của libil2cpp.so
    HANDLE ms = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, mem.pid);
    MODULEENTRY32W me{ sizeof(me) };
    if (Module32FirstW(ms, &me)) {
        do {
            if (!_wcsicmp(me.szModule, L"libil2cpp.so")) {
                mem.base = (uintptr_t)me.modBaseAddr;
                break;
            }
        } while (Module32NextW(ms, &me));
    }
    CloseHandle(ms);

    printf("[+] base=0x%p\n", (void*)mem.base);
    if (!mem.base) {
        printf("[!] libil2cpp.so not found. Inject later!\n");
        Sleep(5000); return 1;
    }

    Overlay ov;
    if (!ov.init(GetModuleHandleW(nullptr))) {
        printf("[!] overlay init failed\n"); Sleep(5000); return 1;
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
            uintptr_t sc    = mem.rpm<uintptr_t>(mem.base + off::StaticClass);
            uintptr_t match = sc ? mem.rpm<uintptr_t>(sc + off::CurrentMatch) : 0;
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

                        Vec3 pos = mem.rpm<Vec3>(e + off::Player_Position);
                        Vec3 head = { pos.x, pos.y + 1.6f, pos.z };
                        Vec2 ps, hs;
                        if (!w2s(pos, ps, vm, sw, sh)) continue;
                        if (!w2s(head, hs, vm, sw, sh)) continue;

                        float h = ps.y - hs.y;
                        float w = h * 0.55f;
                        dl->AddRect(ImVec2(hs.x - w/2, hs.y), ImVec2(hs.x + w/2, ps.y), IM_COL32(255, 0, 0, 255), 0, 0, 1.5f);
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
