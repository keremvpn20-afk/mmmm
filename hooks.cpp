#include "hooks.hpp"
#include "sdk.hpp"
#include "memory.hpp"
#include <mutex>
#include <thread>
#include <chrono>

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
    
    // Eski menü değişkenleri hata vermesin diye
    uintptr_t gDebugBlockSource = 0;
    bool triggerMemoryDump = false;

    static std::thread* scannerThread = nullptr;
    static bool scannerRunning = false;

    // WURST TARZI SAF C++ BLOCK SOURCE TARAYICISI (SIFIR ÇÖKME / SIFIR KASMA)
    void EntityScannerLoop() {
        while (scannerRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            if (!storageEspEnabled || !gLocalPlayer) {
                std::lock_guard<std::mutex> lock(containerMutex);
                detectedContainers.clear();
                gScannedEntitiesCount = 0;
                continue; 
            }

            uintptr_t playerPtr = (uintptr_t)gLocalPlayer;
            if (!SDK::IsValidPtr(playerPtr)) continue;
            
            // Oyuncunun BlockSource (Region) adresini çekiyoruz
            uintptr_t regionPtr = 0;
            if (SDK::IsValidPtr(playerPtr + 0x358)) {
                regionPtr = *(uintptr_t*)(playerPtr + 0x358);
            }
            if (!SDK::IsValidPtr(regionPtr)) continue;

            std::vector<MappedContainer> tempContainers;

            // BlockEntity Listesinin Başlangıç ve Bitiş adreslerini okuyoruz (getBlockEntities mantığı)
            if (SDK::IsValidPtr(regionPtr + 0x48) && SDK::IsValidPtr(regionPtr + 0x50)) {
                uintptr_t listStart = *(uintptr_t*)(regionPtr + 0x48);
                uintptr_t listEnd = *(uintptr_t*)(regionPtr + 0x50);
                
                if (SDK::IsValidPtr(listStart) && SDK::IsValidPtr(listEnd) && listEnd > listStart) {
                    size_t count = (listEnd - listStart) / sizeof(void*);
                    if (count > 2000) count = 2000; // FPS'i korumak için max 2000 sandık

                    for (size_t i = 0; i < count; i++) {
                        if (!SDK::IsValidPtr(listStart + i * sizeof(void*))) continue;
                        
                        uintptr_t entityPtr = *(uintptr_t*)(listStart + i * sizeof(void*));
                        if (!SDK::IsValidPtr(entityPtr)) continue;
                        
                        // TYPE KONTROLÜ (0x24 Offseti)
                        int rawType = 0;
                        if (SDK::IsValidPtr(entityPtr + 0x24)) {
                            rawType = *(int*)(entityPtr + 0x24);
                        }

                        int mappedType = 0;
                        if (rawType == 1 && filterChest) mappedType = 1;           
                        else if (rawType == 2 && filterEnderChest) mappedType = 2; 
                        else if (rawType == 8 && filterHopper) mappedType = 3;     
                        else if (rawType == 6 && filterSpawner) mappedType = 4;    
                        else if (rawType == 15 && filterBarrel) mappedType = 6;    
                        
                        if (mappedType != 0) {
                            // GERÇEK KOORDİNATLAR (Tam Sayı / Integer olarak 0x2C'den okuyoruz!)
                            if (SDK::IsValidPtr(entityPtr + 0x2C) && SDK::IsValidPtr(entityPtr + 0x34)) {
                                int bx = *(int*)(entityPtr + 0x2C);
                                int by = *(int*)(entityPtr + 0x30);
                                int bz = *(int*)(entityPtr + 0x34);
                                
                                MappedContainer c;
                                c.type = mappedType;
                                c.worldPos = { (float)bx + 0.5f, (float)by + 0.5f, (float)bz + 0.5f }; 
                                c.distance = 0.0f; 
                                
                                tempContainers.push_back(c);
                            }
                        }
                    }
                }
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
        if (localPlayer && SDK::IsValidPtr((uintptr_t)localPlayer)) {
            gLocalPlayer = localPlayer;
        }
    }

    void Initialize() {
        scannerRunning = true;
        scannerThread = new std::thread(EntityScannerLoop);
        scannerThread->detach(); 
    }

    void Terminate() {
        scannerRunning = false;
    }
}
