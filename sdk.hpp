#pragma once
#include <vector>
#include <cmath>
#include <mach/mach.h>
#include <stdint.h>

namespace SDK {

    // --- YENI GUVENLI OKUMA (Kernel Level) ---
    template<typename T>
    inline T SafeRead(uintptr_t addr) {
        if (!addr) return T{};
        T val{};
        vm_size_t readCount = sizeof(T);
        // Eger adres gecersizse cokmek yerine bos (sifir) dondurur
        vm_read_overwrite(mach_task_self(), (vm_address_t)addr, sizeof(T), (vm_address_t)&val, &readCount);
        return val;
    }

    // Basit ve hizli gecerli RAM adresi kontrolu
    inline bool IsValidPtr(uintptr_t ptr) {
        // iOS'ta gecerli user-space pointerlar genellikle bu araliktadir
        return (ptr > 0x100000000ULL && ptr < 0x8000000000ULL);
    }

    // 1.26.33 surumu icin buldugumuz global ClientInstance offseti
    constexpr uintptr_t kClientInstancePtrOffset = 0x101D3A10;

    inline uintptr_t GetClientInstance(uintptr_t base) {
        return SafeRead<uintptr_t>(base + kClientInstancePtrOffset);
    }

    // ------------------------------------------

    struct Vector3 {
        float x, y, z;
        Vector3() : x(0), y(0), z(0) {}
        Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
        
        Vector3 operator+(const Vector3& other) const { return {x + other.x, y + other.y, z + other.z}; }
        Vector3 operator-(const Vector3& other) const { return {x - other.x, y - other.y, z - other.z}; }
        
        float distance(const Vector3& other) const {
            float dx = x - other.x;
            float dy = y - other.y;
            float dz = z - other.z;
            return std::sqrt(dx*dx + dy*dy + dz*dz);
        }
    };

    inline Vector3 GetPlayerPosition(uintptr_t localPlayer) {
        return SafeRead<Vector3>(localPlayer + 0x4C0); // 1.26.33 Actor Pos offset
    }

    struct Vector2 {
        float x, y;
        Vector2() : x(0), y(0) {}
        Vector2(float x, float y) : x(x), y(y) {}
    };

    struct Matrix {
        float m[16];
    };

    // Projects 3D block/entity coordinates into 2D coordinates on standard screens
    inline bool WorldToScreen(const Vector3& pos, Vector2& screen, const Matrix& matrix, float width, float height) {
        float x = pos.x * matrix.m[0] + pos.y * matrix.m[4] + pos.z * matrix.m[8] + matrix.m[12];
        float y = pos.x * matrix.m[1] + pos.y * matrix.m[5] + pos.z * matrix.m[9] + matrix.m[13];
        float w = pos.x * matrix.m[3] + pos.y * matrix.m[7] + pos.z * matrix.m[11] + matrix.m[15];

        if (w < 0.1f) return false;

        float ndc_x = x / w;
        float ndc_y = y / w;

        screen.x = (width / 2.0f) + (ndc_x * width / 2.0f);
        screen.y = (height / 2.0f) - (ndc_y * height / 2.0f);
        return true;
    }

    // Dummy class yapilari (artik yeni Kernel Scanner kullandigimiz icin cogu gereksiz, ama uyumluluk icin duruyor)
    class Actor {
    public:
        Vector3 getPosition() {
            return SafeRead<Vector3>((uintptr_t)this + 0x4C0);
        }
    };

    class BlockEntity {
    public:
        Vector3 getPosition() {
            return SafeRead<Vector3>((uintptr_t)this + 0x2C);
        }
        
        int getType() {
            return SafeRead<int>((uintptr_t)this + 0x24);
        }
    };

    class BlockSource {
    };

    class Player : public Actor {
    };
}
