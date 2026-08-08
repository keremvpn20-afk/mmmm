#include "hooks.hpp"
#include "sdk.hpp"
#include "memory.hpp"
#include <mutex>
#include <thread>
#include <chrono>
#include <mach/mach.h>

namespace Hooks {

    bool storageEspEnabled = false;
    bool filterChest       = true;
    bool filterEnderChest  = true;
    bool filterShulker     = true;
    bool filterHopper      = true;
    bool filterSpawner     = true;
    bool filterBarrel      = true;
    bool drawTracers       = false;

    std::vector<MappedContainer> detectedContainers;
    std::mutex containerMutex;

    SDK::Player* gLocalPlayer = nullptr;
    uintptr_t gTickAddressResolved = 0;
    int gScannedEntitiesCount = 0;
    uintptr_t gDebugBlockSource = 0;

    static std::thread* scannerThread = nullptr;
    static bool scannerRunning = false;
    static uintptr_t cachedPlayerPtr = 0;

    static uintptr_t ScanForLocalPlayer() {
        mach_port_t task = mach_task_self();
        vm_address_t address = 0;
        vm_size_t size = 0;
        vm_region_basic_info_data_64_t info;
        mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t object_name;

        while (vm_region_64(task, &address, &size, VM_REGION_BASIC_INFO_64,
                            (vm_region_info_t)&info, &info_count, &object_name) == KERN_SUCCESS) {

            if ((info.protection & VM_PROT_READ) && (info.protection & VM_PROT_WRITE) && size > 0x1000) {

                size_t chunkSize = 4 * 1024 * 1024;
                uint8_t* buf = (uint8_t*)malloc(chunkSize);
                if (buf) {
                    for (vm_address_t cs = address; cs < address + size; cs += chunkSize) {
                        vm_size_t rs = ((address + size - cs) < chunkSize) ? (address + size - cs) : chunkSize;
                        vm_size_t rc = rs;

                        if (vm_read_overwrite(task, cs, rs, (vm_address_t)buf, &rc) == KERN_SUCCESS) {
                            for (size_t off = 0; off + 8 <= rc; off += 8) {
                                uintptr_t candidate = *(uintptr_t*)(buf + off);
                                if (!SDK::IsValidPtr(candidate)) continue;

                                float px = SDK::SafeRead<float>(candidate + 0x4C0);
                                float py = SDK::SafeRead<float>(candidate + 0x4C4);
                                float pz = SDK::SafeRead<float>(candidate + 0x4C8);

                                if (py > -64.f && py < 320.f &&
                                    px > -30000000.f && px < 30000000.f &&
                                    pz > -30000000.f && pz < 30000000.f &&
                                    (px != 0.f || pz != 0.f) && py != 0.f) {
                                    
                                    uintptr_t regionCheck = SDK::SafeRead<uintptr_t>(candidate + 0x358);
                                    if (SDK::IsValidPtr(regionCheck)) {
                                        free(buf);
                                        return candidate;
                                    }
                                }
                            }
                        }
                    }
                    free(buf);
                }
            }
            address += size;
        }
        return 0;
    }

    void EntityScannerLoop() {
        while (scannerRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            if (!storageEspEnabled) {
                std::lock_guard<std::mutex> lock(containerMutex);
                detectedContainers.clear();
                gScannedEntitiesCount = 0;
                gTickAddressResolved = 0;
                continue;
            }

            if (!SDK::IsValidPtr(cachedPlayerPtr)) {
                cachedPlayerPtr = ScanForLocalPlayer();
            }
            
            if (!SDK::IsValidPtr(cachedPlayerPtr)) {
                gTickAddressResolved = 0xDEAD;
                continue;
            }

            float checkY = SDK::SafeRead<float>(cachedPlayerPtr + 0x4C4);
            if (checkY < -64.f || checkY > 320.f) {
                cachedPlayerPtr = 0;
                gTickAddressResolved = 0xDEAD;
                continue;
            }

            gLocalPlayer = (SDK::Player*)cachedPlayerPtr;

            uintptr_t regionPtr = SDK::SafeRead<uintptr_t>(cachedPlayerPtr + 0x358);
            gDebugBlockSource = regionPtr;

            if (!SDK::IsValidPtr(regionPtr)) {
                cachedPlayerPtr = 0;
                gTickAddressResolved = 0xBEEF;
                continue;
            }

            uintptr_t listStart = SDK::SafeRead<uintptr_t>(regionPtr + 0x48);
            uintptr_t listEnd   = SDK::SafeRead<uintptr_t>(regionPtr + 0x50);

            std::vector<MappedContainer> tempContainers;

            if (SDK::IsValidPtr(listStart) && SDK::IsValidPtr(listEnd) && listEnd > listStart) {
                size_t count = (listEnd - listStart) / sizeof(void*);
                if (count > 2000) count = 2000;

                SDK::Vector3 playerPos = SDK::GetPlayerPosition(cachedPlayerPtr);

                for (size_t i = 0; i < count; i++) {
                    uintptr_t entityPtr = SDK::SafeRead<uintptr_t>(listStart + i * sizeof(void*));
                    if (!SDK::IsValidPtr(entityPtr)) continue;

                    int rawType = SDK::SafeRead<int>(entityPtr + 0x24);

                    int mappedType = 0;
                    if      (rawType == 1  && filterChest)      mappedType = 1;
                    else if (rawType == 2  && filterEnderChest) mappedType = 2;
                    else if (rawType == 8  && filterHopper)     mappedType = 3;
                    else if (rawType == 6  && filterSpawner)    mappedType = 4;
                    else if (rawType == 15 && filterBarrel)     mappedType = 6;

                    if (mappedType != 0) {
                        int bx = SDK::SafeRead<int>(entityPtr + 0x2C);
                        int by = SDK::SafeRead<int>(entityPtr + 0x30);
                        int bz = SDK::SafeRead<int>(entityPtr + 0x34);

                        if (bx == 0 && by == 0 && bz == 0) continue;
                        if (by < -64 || by > 320) continue;

                        MappedContainer c;
                        c.type = mappedType;
                        c.worldPos = { (float)bx + 0.5f, (float)by + 0.5f, (float)bz + 0.5f };
                        c.distance = playerPos.distance(c.worldPos);
                        tempContainers.push_back(c);
                    }
                }
            }

            {
                std::lock_guard<std::mutex> lock(containerMutex);
                detectedContainers = tempContainers;
                gScannedEntitiesCount = (int)tempContainers.size();
                gTickAddressResolved = 0x1337;
            }
        }
    }

    void ProcessContainerScanning(SDK::Player* /*unused*/) {}

    void Initialize() {
        scannerRunning = true;
        scannerThread = new std::thread(EntityScannerLoop);
        scannerThread->detach();
    }

    void Terminate() {
        scannerRunning = false;
    }
}
