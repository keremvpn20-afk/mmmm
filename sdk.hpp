#pragma once
#include <vector>
#include <cmath>
#include <mach/mach.h>

namespace SDK {

    struct Vector3 {
        float x, y, z;
        Vector3() : x(0), y(0), z(0) {}
        Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
        
        float distance(const Vector3& other) const {
            float dx = x - other.x, dy = y - other.y, dz = z - other.z;
            return sqrtf(dx*dx + dy*dy + dz*dz);
        }
    };

    struct Vector2 {
        float x, y;
        Vector2() : x(0), y(0) {}
    };

    struct Matrix { float m[16]; };

    inline bool IsValidPtr(uintptr_t ptr) {
        return ptr > 0x100000000ULL && ptr < 0x7FFFFFFFFFFFULL;
    }

    template<typename T>
    inline T SafeRead(uintptr_t addr) {
        if (!IsValidPtr(addr)) return T{};
        return *(T*)addr;
    }

    inline bool WorldToScreen(const Vector3& pos, Vector2& screen, const Matrix& matrix, float width, float height) {
        float x = pos.x * matrix.m[0] + pos.y * matrix.m[4] + pos.z * matrix.m[8]  + matrix.m[12];
        float y = pos.x * matrix.m[1] + pos.y * matrix.m[5] + pos.z * matrix.m[9]  + matrix.m[13];
        float w = pos.x * matrix.m[3] + pos.y * matrix.m[7] + pos.z * matrix.m[11] + matrix.m[15];

        if (w < 0.1f) return false;
        screen.x = (width  / 2.0f) * (1.0f + x / w);
        screen.y = (height / 2.0f) * (1.0f - y / w);
        return true;
    }

    inline Vector3 GetPlayerPosition(uintptr_t player) {
        Vector3 pos;
        if (IsValidPtr(player + 0x4C0)) {
            pos.x = *(float*)(player + 0x4C0);
            pos.y = *(float*)(player + 0x4C4);
            pos.z = *(float*)(player + 0x4C8);
        }
        return pos;
    }

    class Player {};
}
