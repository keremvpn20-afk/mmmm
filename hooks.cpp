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
    uintptr_t gDebugBlockSource = 0;
    int gDebugListSize = 0;
    bool triggerMemoryDump = false;

    static std::thread* scannerThread = nullptr;
    static bool scannerRunning = false;

    void MemoryScannerLoop() {
        while (scannerRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            
            if (!storageEspEnabled) {
                // Eger ESP kapaliysa listeyi temizle
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
                                
                                for (size_t offset = 0; offset < readSize - 0x50; offset += 4) {
                                    
                                    int bx = *(int*)(buf + offset);
                                    if (bx < -30000000 || bx > 30000000) continue; // Mantikli kordinat araligi kontrolu
                                    
                                    int by = *(int*)(buf + offset + 4);
                                    if (by < -64 || by > 320) continue;
                                    
                                    int bz = *(int*)(buf + offset + 8);
                                    if (bz < -30000000 || bz > 30000000) continue;
                                    
                                    int type = *(int*)(buf + offset + 12);
                                    
                                    // Menüdeki (Toggles) ayarlara gore filtreleme
                                    if (type == 2 && !filterChest) continue;
                                    if (type == 23 && !filterEnderChest) continue;
                                    if (type == 25 && !filterShulker) continue;
                                    if (type == 15 && !filterHopper) continue;
                                    if (type == 5 && !filterSpawner) continue;
                                    if (type == 42 && !filterBarrel) continue;
                                    
                                    // Senin overlay.mm cizici koduna uygun ID'lere ceviriyoruz
                                    int mappedType = 0;
                                    if (type == 2) mappedType = 1;
                                    else if (type == 23) mappedType = 2;
                                    else if (type == 15) mappedType = 3;
                                    else if (type == 5) mappedType = 4;
                                    else if (type == 25) mappedType = 5;
                                    else if (type == 42) mappedType = 6;

                                    if (mappedType != 0) {
                                        // Sahte (Fake) sonuclari elemek icin AABB kutusu dogrulamasi
                                        float minX = *(float*)(buf + offset + 0x38);
                                        float minY = *(float*)(buf + offset + 0x3C);
                                        float minZ = *(float*)(buf + offset + 0x40);
                                        
                                        if (std::abs(minX - bx) <= 1.0f && 
                                            std::abs(minY - by) <= 1.0f && 
                                            std::abs(minZ - bz) <= 1.0f) {
                                            
                                            MappedContainer c;
                                            c.type = mappedType;
                                            c.worldPos = { (float)bx, (float)by, (float)bz };
                                            c.distance = 0.0f; // Bizim sistemde distance hesaplamaya gerek yok
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
                std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Oyunda takilma yapmamasi icin cok kisa dinlenme
            }
            
            {
                // Tarama bitti! Yeni listeyi gonder!
                std::lock_guard<std::mutex> lock(containerMutex);
                detectedContainers = tempContainers;
                gScannedEntitiesCount = tempContainers.size();
                gTickAddressResolved = 0x1337; // ESP AKTIF MESAJI ICIN
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
