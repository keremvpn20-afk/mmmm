#pragma once
#include <vector>
#include <cmath>

namespace SDK {

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
            return sqrtf(dx*dx + dy*dy + dz*dz);
        }
    };

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

    class Actor {
    public:
        Vector3 getPosition() {
            // Pos coordinate vector located inside Actor properties
            return *(Vector3*)((uintptr_t)this + 0x4C0);
        }
        
        bool isLocalPlayer() {
            // Evaluates class identity using dynamic virtual pointer indexing (VTable)
            typedef bool (*Func)(Actor*);
            return ((Func)(*(uintptr_t**)this)[1])(this);
        }
    };

    class BlockEntity {
    public:
        Vector3 getPosition() {
            return *(Vector3*)((uintptr_t)this + 0x2C);
        }
        
        int getType() {
            // Mapping types: 1 = Chest, 2 = EnderChest, 8 = Hopper, 6 = Spawner, 10 = Piston, 15 = Barrel
            return *(int*)((uintptr_t)this + 0x24);
        }
    };

    class BlockSource {
    public:
        std::vector<BlockEntity*> getBlockEntities() {
            std::vector<BlockEntity*> list;
            
            // Traverses BlockEntity list safely
            uintptr_t listStart = *(uintptr_t*)((uintptr_t)this + 0x48);
            uintptr_t listEnd = *(uintptr_t*)((uintptr_t)this + 0x50);
            
            if (listStart && listEnd && listEnd > listStart) {
                size_t count = (listEnd - listStart) / sizeof(void*);
                // Clamp container checks to avoid memory overflow crashes
                if (count > 2000) count = 2000; 

                for (size_t i = 0; i < count; i++) {
                    BlockEntity* entity = *(BlockEntity**)(listStart + i * sizeof(void*));
                    if (entity) {
                        list.push_back(entity);
                    }
                }
            }
            return list;
        }
    };

    class Player : public Actor {
    public:
        BlockSource* getRegion() {
            return *(BlockSource**)((uintptr_t)this + 0x358);
        }
    };
}
