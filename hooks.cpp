#include "hooks.hpp"
#include "sdk.hpp"
#include "memory.hpp"
#include <iostream>
#include <mutex>
#include <thread>

namespace Hooks {
    
    // Core settings mapping to GUI toggle states
    bool storageEspEnabled = false;

    // Filters for "..." sub-settings panel
    bool filterChest = true;
    bool filterEnderChest = true;
    bool filterShulker = true;
    bool filterHopper = true;
    bool filterSpawner = true;
    bool filterBarrel = true;

    // Tracers toggle
    bool drawTracers = false;

    // Thread-safe coordinate tracking array
    std::vector<MappedContainer> detectedContainers;
    std::mutex containerMutex;

    // Active player pointer
    SDK::Player* gLocalPlayer = nullptr;

    // Original ticked functions captured dynamically
    void (*oSendComplexInventoryTransaction)(void* self, void* transaction) = nullptr;
    void (*oRemoveActor)(void* self, void* packet) = nullptr;

    // Debugging properties
    uintptr_t gTickAddressResolved = 0;
    int gScannedEntitiesCount = 0;

    #include <dlfcn.h>
    typedef void (*MSHookFunction_t)(void *symbol, void *replace, void **result);

    // Fast inline arm64 patch injection with dynamic jailbreak framework resolution
    bool ApplyInlineHook(void* target, void* replacement, void** original) {
        if (!target || !replacement || !original) return false;

        // 1. Try resolving MSHookFunction dynamically from Substrate or ElleKit (Standard jailbreak methods)
        void* handles[] = {
            dlopen("/usr/lib/libsubstrate.dylib", RTLD_LAZY),
            dlopen("/var/lib/ellekit/ellekit.dylib", RTLD_LAZY),
            dlopen("/Library/Frameworks/CydiaSubstrate.framework/CydiaSubstrate", RTLD_LAZY),
            dlopen("/usr/lib/libellekit.dylib", RTLD_LAZY),
            dlopen("libsubstrate.dylib", RTLD_LAZY)
        };
        
        for (void* handle : handles) {
            if (handle) {
                MSHookFunction_t hookFn = (MSHookFunction_t)dlsym(handle, "MSHookFunction");
                if (hookFn) {
                    hookFn(target, replacement, original);
                    printf("[yt] Hooked successfully using MSHookFunction from jailbreak environment\n");
                    return true;
                }
            }
        }

        // 2. Fallback to manual write-protection bypass byte-patching (e.g. Sideloads / TrollStore)
        printf("[yt] Jailbreak hooks not found, falling back to manual byte-patching\n");
        *original = target;

        // Assembly patch: LDR X16, #8 ; BR X16
        uint32_t patchInstructions[] = {
            0x58000050, // LDR X16, 8
            0xD61F0200  // BR X16
        };

        uint8_t payload[16];
        memcpy(payload, patchInstructions, 8);
        uintptr_t replacementAddress = (uintptr_t)replacement;
        memcpy(payload + 8, &replacementAddress, 8);

        std::vector<uint8_t> patchBytes(payload, payload + 16);
        return Memory::Patch((uintptr_t)target, patchBytes);
    }

    // Traverses block entities down to y=-64 safely on a separate thread to prevent FPS drops
    void ProcessContainerScanning(SDK::Player* localPlayer) {
        if (!localPlayer) return;

        SDK::BlockSource* region = localPlayer->getRegion();
        if (!region) return;

        std::vector<SDK::BlockEntity*> rawEntities = region->getBlockEntities();
        std::vector<MappedContainer> tempContainers;

        SDK::Vector3 localPos = localPlayer->getPosition();

        for (auto* blockEntity : rawEntities) {
            if (!blockEntity) continue;

            SDK::Vector3 pos = blockEntity->getPosition();
            
            // Absolute depth check (detect down to bedrock layer y=-64)
            if (pos.y < -64.0f || pos.y > 320.0f) continue;

            // Distance limit of 100 blocks to optimize performance and prevent visual clutter
            float dist = localPos.distance(pos);
            if (dist > 100.0f) continue;

            int rawType = blockEntity->getType();
            int mappedType = -1;

            // Map and filter types dynamically
            if (rawType == 1 && filterChest) mappedType = 1;       // Chest
            else if (rawType == 2 && filterEnderChest) mappedType = 2; // EnderChest
            else if (rawType == 8 && filterHopper) mappedType = 3;     // Hopper
            else if (rawType == 6 && filterSpawner) mappedType = 4;    // Spawner
            else if (rawType == 10 && filterShulker) mappedType = 5;   // Shulker (mapped to piston slot in old Pe engines)
            else if (rawType == 15 && filterBarrel) mappedType = 6;    // Barrel

            if (mappedType != -1) {
                tempContainers.push_back({ mappedType, pos, dist });
            }
        }

        // Thread-safe update of visual data
        std::lock_guard<std::mutex> lock(containerMutex);
        gScannedEntitiesCount = (int)tempContainers.size();
        detectedContainers = std::move(tempContainers);
    }

    // Intercepted inventory transaction to capture LocalPlayer
    void hkSendComplexInventoryTransaction(void* self, void* transaction) {
        if (self) {
            gLocalPlayer = (SDK::Player*)self;
            gTickAddressResolved = 0x4AD18D0; // Visual debug indicator
        }
        if (oSendComplexInventoryTransaction) {
            oSendComplexInventoryTransaction(self, transaction);
        }
    }

    // Intercepted actor removal to safely clear tracking
    void hkRemoveActor(void* self, void* packet) {
        gLocalPlayer = nullptr;
        gTickAddressResolved = 0;
        if (oRemoveActor) {
            oRemoveActor(self, packet);
        }
    }

    void Initialize() {
        uintptr_t base = Memory::GetBaseAddress();
        printf("[yt] Initialize: Base address is 0x%lx\n", base);
        
        // 1. Hook sendComplexInventoryTransaction (retrieves LocalPlayer*)
        uintptr_t transactionAddr = base + 0x4AD18D0;
        bool hook1 = ApplyInlineHook((void*)transactionAddr, (void*)&hkSendComplexInventoryTransaction, (void**)&oSendComplexInventoryTransaction);
        printf("[yt] Hook sendComplexInventoryTransaction result: %s at address 0x%lx\n", hook1 ? "SUCCESS" : "FAILED", transactionAddr);
        
        // 2. Hook RemoveActorPacket handler (resets LocalPlayer* on leave)
        uintptr_t removeActorAddr = base + 0x43DA384;
        bool hook2 = ApplyInlineHook((void*)removeActorAddr, (void*)&hkRemoveActor, (void**)&oRemoveActor);
        printf("[yt] Hook RemoveActorPacket handler result: %s at address 0x%lx\n", hook2 ? "SUCCESS" : "FAILED", removeActorAddr);
    }

    void Terminate() {
        // Safe tear down
    }
}
