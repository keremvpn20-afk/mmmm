#include "hooks.hpp"
#include "sdk.hpp"
#include "memory.hpp"
#include <iostream>
#include <mutex>
#include <thread>
#include <dlfcn.h>

namespace Hooks {
    
    bool storageEspEnabled = false;
    bool filterChest = true;
    bool filterEnderChest = true;
    bool filterShulker = true;
    bool filterHopper = true;
    bool filterSpawner = true;
    bool filterBarrel = true;
    bool drawTracers = false;

    std::vector<MappedContainer> detectedContainers;
    std::mutex containerMutex;
    SDK::Player* gLocalPlayer = nullptr;

    void (*oClientTick)(void* self) = nullptr;
    void (*oRemoveActor)(void* self, void* packet) = nullptr;

    uintptr_t gTickAddressResolved = 0;
    int gScannedEntitiesCount = 0;

    typedef void (*MSHookFunction_t)(void* symbol, void* replace, void** result);

    bool ApplyInlineHook(void* target, void* replacement, void** original) {
        if (!target || !replacement || !original) return false;

        const char* libs[] = {
            "/usr/lib/libsubstrate.dylib",
            "/usr/lib/libellekit.dylib",
            "/var/lib/ellekit/ellekit.dylib",
            "/Library/Frameworks/CydiaSubstrate.framework/CydiaSubstrate",
            "libsubstrate.dylib"
        };
        for (const char* path : libs) {
            void* handle = dlopen(path, RTLD_LAZY | RTLD_NOLOAD);
            if (!handle) handle = dlopen(path, RTLD_LAZY);
            if (handle) {
                MSHookFunction_t fn = (MSHookFunction_t)dlsym(handle, "MSHookFunction");
                if (fn) {
                    fn(target, replacement, original);
                    printf("[yt] MSHookFunction from %s OK\n", path);
                    return true;
                }
            }
        }

        printf("[yt] Falling back to manual byte-patch\n");
        *original = target;
        uint32_t patch[] = { 0x58000050, 0xD61F0200 };
        uint8_t payload[16];
        memcpy(payload, patch, 8);
        uintptr_t dst = (uintptr_t)replacement;
        memcpy(payload + 8, &dst, 8);
        std::vector<uint8_t> patchBytes(payload, payload + 16);
        return Memory::Patch((uintptr_t)target, patchBytes);
    }

    void ProcessContainerScanning(SDK::Player* localPlayer) {
        if (!localPlayer) return;
        SDK::BlockSource* region = localPlayer->getRegion();
        if (!region) return;

        std::vector<SDK::BlockEntity*> rawEntities = region->getBlockEntities();
        std::vector<MappedContainer> tempContainers;
        SDK::Vector3 localPos = localPlayer->getPosition();

        for (auto* be : rawEntities) {
            if (!be) continue;
            SDK::Vector3 pos = be->getPosition();
            if (pos.y < -64.0f || pos.y > 320.0f) continue;
            float dist = localPos.distance(pos);
            if (dist > 100.0f) continue;

            int rawType = be->getType();
            int mappedType = -1;
            if      (rawType == 1  && filterChest)      mappedType = 1;
            else if (rawType == 2  && filterEnderChest) mappedType = 2;
            else if (rawType == 8  && filterHopper)     mappedType = 3;
            else if (rawType == 6  && filterSpawner)    mappedType = 4;
            else if (rawType == 10 && filterShulker)    mappedType = 5;
            else if (rawType == 15 && filterBarrel)     mappedType = 6;

            if (mappedType != -1)
                tempContainers.push_back({ mappedType, pos, dist });
        }

        std::lock_guard<std::mutex> lock(containerMutex);
        gScannedEntitiesCount = (int)tempContainers.size();
        detectedContainers = std::move(tempContainers);
    }

    // Her frame tetiklenen Client ECS tick fonksiyonu (0xA608634)
    void hkClientTick(void* self) {
        if (self && !gLocalPlayer) {
            SDK::Player* candidate = *(SDK::Player**)((uintptr_t)self + 0x3E8);
            if (candidate) {
                SDK::Vector3 pos = candidate->getPosition();
                if (pos.y > -200.0f && pos.y < 400.0f && pos.x != 0.0f) {
                    gLocalPlayer = candidate;
                    printf("[yt] LocalPlayer captured at 0x%p\n", candidate);
                }
            }
        }
        if (oClientTick) oClientTick(self);
    }

    void hkRemoveActor(void* self, void* packet) {
        gLocalPlayer = nullptr;
        gTickAddressResolved = 0;
        if (oRemoveActor) oRemoveActor(self, packet);
    }

    void Initialize() {
        uintptr_t base = Memory::GetBaseAddress();
        printf("[yt] Base: 0x%lx\n", base);

        uintptr_t tickAddr = base + 0xA608634;
        bool r1 = ApplyInlineHook((void*)tickAddr, (void*)&hkClientTick, (void**)&oClientTick);
        printf("[yt] ClientTick hook: %s @ 0x%lx\n", r1 ? "OK" : "FAIL", tickAddr);
        // Hook başarıyla uygulandıysa anında HOOKED göster
        if (r1) gTickAddressResolved = 0xA608634;

        uintptr_t removeAddr = base + 0x43DA384;
        bool r2 = ApplyInlineHook((void*)removeAddr, (void*)&hkRemoveActor, (void**)&oRemoveActor);
        printf("[yt] RemoveActor hook: %s @ 0x%lx\n", r2 ? "OK" : "FAIL", removeAddr);
    }

    void Terminate() {}
}
