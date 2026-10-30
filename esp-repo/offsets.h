#pragma once
#include <cstdint>

namespace off {
    constexpr uintptr_t InitBase        = 0xA342EFC;
    constexpr uintptr_t StaticClass     = 0x5C;

    constexpr uintptr_t CurrentMatch    = 0x50;
    constexpr uintptr_t LocalPlayer     = 0x94;
    constexpr uintptr_t DictionaryEntities = 0x68;

    constexpr uintptr_t Player_IsDead   = 0x50;
    constexpr uintptr_t Player_Name     = 0x31C;
    constexpr uintptr_t Player_Position = 0x18C; // Vẫn giữ offset cũ, nếu sai sẽ phải dùng PhysXData
    constexpr uintptr_t Player_TeamModeID = 0x2D4;
    constexpr uintptr_t Player_IsBot    = 0x324;

    constexpr uintptr_t FollowCamera    = 0x494;
    constexpr uintptr_t Camera          = 0x18;
    constexpr uintptr_t ViewMatrix      = 0xE8;
}
