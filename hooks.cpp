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

    // Original ticked functions captured dynamically
    void (*oPlayerTick)(SDK::Player* self) = nullptr;

    // Debugging properties
    uintptr_t gTickAddressResolved = 0;
    int gScannedEntitiesCount = 0;

    // Fast inline arm64 patch injection
    bool ApplyInlineHook(void* target, void* replacement, void** original) {
        if (!target || !replacement || !original) return false;

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

    // Intercepted player tick loop
    void hkPlayerTick(SDK::Player* self) {
        if (self && self->isLocalPlayer()) {
            if (storageEspEnabled) {
                // Run container traversal in an isolated thread to maintain game performance (0 FPS drop)
                std::thread scanThread(ProcessContainerScanning, self);
                scanThread.detach();
            } else {
                std::lock_guard<std::mutex> lock(containerMutex);
                detectedContainers.clear();
            }
        }

        if (oPlayerTick) {
            oPlayerTick(self);
        }
    }

    void Initialize() {
        // Dynamically resolve Player::tick signature via pattern matching (no static offsets needed)
        // Standard signature pattern matches the prologue of Player::tick function in Arm64 Bedrock
        uintptr_t tickAddress = Memory::FindSignature("FD 7B BE A9 FD 03 00 91 F3 0B 00 F9 ? ? ? ? F4 4F 01 A9");
        gTickAddressResolved = tickAddress;
        if (tickAddress) {
            std::cout << "[yt] Player::tick resolved: 0x" << std::hex << tickAddress << ". Hooking..." << std::endl;
            ApplyInlineHook((void*)tickAddress, (void*)&hkPlayerTick, (void**)&oPlayerTick);
        } else {
            std::cout << "[yt] [WARNING] Player::tick signature search failed. Hooks disabled." << std::endl;
        }
    }

    void Terminate() {
        // Safe tear down
    }
}
