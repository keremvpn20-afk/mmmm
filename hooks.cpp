#include "hooks.hpp"
#include "sdk.hpp"
#include "memory.hpp"
#include <mutex>
#include <thread>
#include <chrono>
#include <cmath>

namespace Hooks {

    bool storageEspEnabled = false;
    bool filterChest       = true;
    bool filterEnderChest  = true;
    bool filterShulker     = true;
    bool filterHopper      = true;
    bool filterSpawner     = true;
    bool filterBarrel      = true;
    bool drawTracers       = false; // Menüden bunu aktif etmeyi unutma!

    std::vector<MappedContainer> detectedContainers;
    std::mutex containerMutex;

    SDK::Player* gLocalPlayer = nullptr;

    uintptr_t gTickAddressResolved = 0;
    int gScannedEntitiesCount = 0;
    uintptr_t gDebugBlockSource = 0;
    int gDebugListSize = 0;
    bool triggerMemoryDump = false;

    static std::thread* scannerThread = nullptr;
    static bool scannerRunning = false;

    void MemoryScannerLoop() {
        while (scannerRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            
            if (!storageEspEnabled) {
                std::lock_guard<std::mutex> lock(containerMutex);
                detectedContainers.clear();
                gScannedEntitiesCount = 0;
                continue; 
            }

            std::vector<MappedContainer> tempContainers;
            
            mach_port_t task = mach_task_self();
            vm_address_t address = 0;
            vm_size_t size = 0;
            vm_region_basic_info_data_64_t info;
            mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
            mach_port_t object_name;

            while (vm_region_64(task, &address, &size, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info, &info_count, &object_name) == KERN_SUCCESS) {
                
                if ((info.protection & VM_PROT_READ) && (info.protection & VM_PROT_WRITE)) {
                    
                    size_t chunkSize = 1024 * 1024;
                    uint8_t* buf = (uint8_t*)malloc(chunkSize);
                    
                    if (buf) {
                        for (vm_address_t chunkStart = address; chunkStart < address + size; chunkStart += chunkSize) {
                            vm_size_t readSize = (address + size - chunkStart < chunkSize) ? (address + size - chunkStart) : chunkSize;
                            vm_size_t readCount = readSize;
                            
                            if (vm_read_overwrite(task, chunkStart, readSize, (vm_address_t)buf, &readCount) == KERN_SUCCESS) {
                                
                                for (size_t offset = 8; offset < readSize - 0x50; offset += 4) {
                                    
                                    int bx = *(int*)(buf + offset);
                                    int by = *(int*)(buf + offset + 4);
                                    int bz = *(int*)(buf + offset + 8);
                                    
                                    if (bx == 0 && by == 0 && bz == 0) continue;
                                    
                                    if (bx < -30000000 || bx > 30000000) continue;
                                    if (by < -64 || by > 320) continue;
                                    if (bz < -30000000 || bz > 30000000) continue;
                                    
                                    // SADECE GERÇEK SANDIK ID'LERİ (Çimenler Elendi)
                                    int type = *(int*)(buf + offset + 12);
                                    
                                    if (type == 54 || type == 130 || type == 154 || type == 52) {
                                        
                                        uintptr_t vtable = *(uintptr_t*)(buf + offset - 8);
                                        if (vtable < 0x100000000) continue; 
                                        
                                        float minX = *(float*)(buf + offset + 0x38);
                                        float minY = *(float*)(buf + offset + 0x3C);
                                        float minZ = *(float*)(buf + offset + 0x40);
                                        
                                        float maxX = *(float*)(buf + offset + 0x44);
                                        float maxY = *(float*)(buf + offset + 0x48);
                                        float maxZ = *(float*)(buf + offset + 0x4C);
                                        
                                        float diffX = maxX - minX;
                                        float diffY = maxY - minY;
                                        float diffZ = maxZ - minZ;
                                        
                                        if (diffX < 0.1f || diffX > 1.5f) continue;
                                        if (diffY < 0.1f || diffY > 1.5f) continue;
                                        if (diffZ < 0.1f || diffZ > 1.5f) continue;

                                        if (std::abs(minX - bx) <= 1.0f && std::abs(minY - by) <= 1.0f && std::abs(minZ - bz) <= 1.0f) {
                                            
                                            // Kopya Depo Sandıklarını Eler (Deduplication)
                                            bool isDuplicate = false;
                                            for (const auto& existing : tempContainers) {
                                                if (existing.worldPos.x == (float)bx && existing.worldPos.y == (float)by && existing.worldPos.z == (float)bz) {
                                                    isDuplicate = true;
                                                    break;
                                                }
                                            }
                                            if (isDuplicate) continue;
                                            
                                            int mappedType = 0;
                                            if (type == 54) mappedType = 1;      // Chest
                                            else if (type == 130) mappedType = 2; // EnderChest
                                            else if (type == 154) mappedType = 3; // Hopper
                                            else if (type == 52)  mappedType = 4; // Spawner

                                            if (mappedType != 0) {
                                                MappedContainer c;
                                                c.type = mappedType;
                                                c.worldPos = { (float)bx, (float)by, (float)bz };
                                                c.distance = 0.0f; 
                                                tempContainers.push_back(c);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        free(buf);
                    }
                }
                address += size;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            
            {
                std::lock_guard<std::mutex> lock(containerMutex);
                detectedContainers = tempContainers;
                gScannedEntitiesCount = tempContainers.size();
                gTickAddressResolved = 0x1337; 
            }
        }
    }

    void ProcessContainerScanning(SDK::Player* localPlayer) {
        if (localPlayer) gLocalPlayer = localPlayer;
    }

    void Initialize() {
        scannerRunning = true;
        scannerThread = new std::thread(MemoryScannerLoop);
        scannerThread->detach(); 
    }

    void Terminate() {
        scannerRunning = false;
    }
}
