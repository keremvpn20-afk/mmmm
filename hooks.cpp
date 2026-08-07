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
    bool drawTracers       = false;

    std::vector<MappedContainer> detectedContainers;
    std::mutex containerMutex;

    SDK::Player* gLocalPlayer = nullptr;

    uintptr_t gTickAddressResolved = 0;
    int gScannedEntitiesCount = 0;

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
                                    
                                    // SADECE GERÇEK SANDIK ID'LERİ
                                    int type = *(int*)(buf + offset + 12);
                                    
                                    if (type == 54 || type == 130 || type == 154 || type == 52) {
                                        
                                        uintptr_t vtable = *(uintptr_t*)(buf + offset - 8);
                                        if (vtable < 0x100000000) continue; 
                                        
                                        // GHIDRA'DAN ALINAN GERÇEK KOORDİNAT OFSETLERİ (Integer - Tam Sayı)
                                        int blockX = *(int*)(buf + offset + 0x2C);
                                        int blockY = *(int*)(buf + offset + 0x30);
                                        int blockZ = *(int*)(buf + offset + 0x34);
                                        
                                        if (blockX == 0 && blockY == 0 && blockZ == 0) continue;
                                        if (blockX < -30000000 || blockX > 30000000) continue;
                                        if (blockY < -64 || blockY > 320) continue;
                                        if (blockZ < -30000000 || blockZ > 30000000) continue;

                                        // Kopya Sandıkları Eler (Deduplication)
                                        bool isDuplicate = false;
                                        for (const auto& existing : tempContainers) {
                                            if (existing.worldPos.x == (float)blockX && existing.worldPos.y == (float)blockY && existing.worldPos.z == (float)blockZ) {
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
                                            // Ekrana tam oturması için Y eksenine 0.5 ekliyoruz (Sandığın tam ortası)
                                            c.worldPos = { (float)blockX, (float)blockY + 0.5f, (float)blockZ };
                                            c.distance = 0.0f; 
                                            tempContainers.push_back(c);
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
