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
    
    // Eski menü hata vermesin diye boş tanımlamalar
    uintptr_t gDebugBlockSource = 0;
    bool triggerMemoryDump = false;

    static std::thread* scannerThread = nullptr;
    static bool scannerRunning = false;

    // WURST TARZI BLOCK SOURCE TARAYICISI (FPS DONMASINA SON!)
    void EntityScannerLoop() {
        while (scannerRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Kasmaması için bekleme
            
            if (!storageEspEnabled || !gLocalPlayer) {
                std::lock_guard<std::mutex> lock(containerMutex);
                detectedContainers.clear();
                gScannedEntitiesCount = 0;
                continue; 
            }

            // CRASH KORUMASI BAŞLANGICI
            if (!SDK::IsValidPtr((uintptr_t)gLocalPlayer)) continue;
            
            SDK::BlockSource* region = nullptr;
            @try { 
                region = gLocalPlayer->getRegion();
            } @catch (...) {
                continue;
            }
            if (!SDK::IsValidPtr((uintptr_t)region)) continue;
            // CRASH KORUMASI BİTİŞİ

            std::vector<SDK::BlockEntity*> entityList = region->getBlockEntities();
            std::vector<MappedContainer> tempContainers;

            for (SDK::BlockEntity* entity : entityList) {
                if (!SDK::IsValidPtr((uintptr_t)entity)) continue;
                
                int rawType = 0;
                @try { rawType = entity->getType(); } @catch (...) { continue; }

                int mappedType = 0;
                
                // TYPE EŞLEŞTİRMESİ
                if (rawType == 1 && filterChest) mappedType = 1;           
                else if (rawType == 2 && filterEnderChest) mappedType = 2; 
                else if (rawType == 8 && filterHopper) mappedType = 3;     
                else if (rawType == 6 && filterSpawner) mappedType = 4;    
                else if (rawType == 15 && filterBarrel) mappedType = 6;    
                
                if (mappedType != 0) {
                    SDK::Vector3 pos = entity->getPosition();
                    
                    MappedContainer c;
                    c.type = mappedType;
                    c.worldPos = { pos.x + 0.5f, pos.y + 0.5f, pos.z + 0.5f }; // Kutuyu merkeze hizala
                    c.distance = 0.0f; // Mesafe şimdilik sabit
                    
                    tempContainers.push_back(c);
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
