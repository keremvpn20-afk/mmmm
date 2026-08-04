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
    uintptr_t gDebugBlockSource = 0;
    int gDebugListSize = 0;

    static uintptr_t sCalibratedOffset = 0;

    static uintptr_t FindLocalPlayerOffset(uintptr_t clientInstance) {
        for (uintptr_t off = 0x150; off <= 0x280; off += 8) {
            uintptr_t candidate = SDK::SafeRead<uintptr_t>(clientInstance + off);
            if (!SDK::IsValidPtr(candidate)) continue;

            float posX = SDK::SafeRead<float>(candidate + 0x4C0 + 0);
            float posY = SDK::SafeRead<float>(candidate + 0x4C0 + 4);
            float posZ = SDK::SafeRead<float>(candidate + 0x4C0 + 8);

            if (posY > -64.f && posY < 320.f &&
                posX > -30000000.f && posX < 30000000.f &&
                posZ > -30000000.f && posZ < 30000000.f &&
                (posX != 0.f || posZ != 0.f)) {
                return off;
            }
        }
        return 0;
    }

    static std::thread* scannerThread = nullptr;
    static bool scannerRunning = false;

    void MemoryScannerLoop() {
        while (scannerRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            
            if (!storageEspEnabled || !gLocalPlayer) {
                continue;
            }

            SDK::Vector3 playerPos = SDK::GetPlayerPosition((uintptr_t)gLocalPlayer);
            if (playerPos.y < -64.f || playerPos.y > 320.f) {
                continue;
            }

            std::vector<MappedContainer> temp;
            mach_port_t task = mach_task_self();
            
            // APPLE IOS UYUMLU DEGISTIRILDI: mach_vm_... yerine vm_... kullanilacak
            vm_address_t address = 0;
            vm_size_t size = 0;
            vm_region_basic_info_data_64_t info;
            mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
            mach_port_t object_name;

            int totalFound = 0;

            // Tüm RAM'i gez - IOS Uyumlu vm_region_64
            while (vm_region_64(task, &address, &size, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info, &info_count, &object_name) == KERN_SUCCESS) {
                
                if ((info.protection & VM_PROT_READ) && (info.protection & VM_PROT_WRITE)) {
                    
                    size_t chunkSize = 1024 * 1024; // 1 MB Buffer
                    uint8_t* buf = (uint8_t*)malloc(chunkSize);
                    
                    if (buf) {
                        for (vm_address_t chunkStart = address; chunkStart < address + size; chunkStart += chunkSize) {
                            vm_size_t readSize = (address + size - chunkStart < chunkSize) ? (address + size - chunkStart) : chunkSize;
                            vm_size_t readCount = readSize;
                            
                            if (vm_read_overwrite(task, chunkStart, readSize, (vm_address_t)buf, &readCount) == KERN_SUCCESS) {
                                
                                for (size_t offset = 0; offset < readSize - 0x40; offset += 8) {
                                    int type = *(int*)(buf + offset + 0x24);
                                    
                                    if (type == 1 || type == 2 || type == 6 || type == 8 || type == 10 || type == 15 || type == 16) {
                                        
                                        int bx = *(int*)(buf + offset + 0x2C + 0);
                                        int by = *(int*)(buf + offset + 0x2C + 4);
                                        int bz = *(int*)(buf + offset + 0x2C + 8);
                                        
                                        // Uzaklik sinirini tamamen kaldirdik. Minecraft dunya sinirlari (Y yuksekligi) icindeyse direk ekle.
                                        if (by > -64 && by < 320) {
                                            SDK::Vector3 pos = {(float)bx + 0.5f, (float)by + 0.5f, (float)bz + 0.5f};
                                            float dist = playerPos.distance(pos);
                                            
                                            int mapped = -1;
                                            if      (type == 1  && filterChest)      mapped = 1;
                                            else if (type == 2  && filterEnderChest) mapped = 2;
                                            else if ((type == 8 || type == 16) && filterHopper) mapped = 3;
                                            else if (type == 6  && filterSpawner)    mapped = 4;
                                            else if (type == 10 && filterShulker)    mapped = 5;
                                            else if (type == 15 && filterBarrel)     mapped = 6;

                                            if (mapped != -1) {
                                                temp.push_back({ mapped, pos, dist });
                                                totalFound++;
                                            }
                                        }
                                    }
                                }
                            }
                            if (totalFound > 5000) break; // Maksimum 5000 sandik
                        }
                        free(buf);
                    }
                }
                address += size;
                if (totalFound > 5000) break;
            }
            
            std::lock_guard<std::mutex> lock(containerMutex);
            gScannedEntitiesCount = (int)temp.size();
            detectedContainers = std::move(temp);
            gDebugBlockSource = 0x8888; 
            gDebugListSize = gScannedEntitiesCount;
        }
    }

    void ProcessContainerScanning(SDK::Player* /*unused*/) {
        uintptr_t base = Memory::GetBaseAddress();

        uintptr_t clientInstance = SDK::GetClientInstance(base);
        if (!SDK::IsValidPtr(clientInstance)) {
            gTickAddressResolved = 0xDEAD;
            return;
        }

        if (sCalibratedOffset == 0)
            sCalibratedOffset = FindLocalPlayerOffset(clientInstance);

        if (sCalibratedOffset == 0) {
            gTickAddressResolved = 0xBEEF;
            return;
        }

        uintptr_t localPlayer = SDK::SafeRead<uintptr_t>(clientInstance + sCalibratedOffset);
        if (!SDK::IsValidPtr(localPlayer)) {
            sCalibratedOffset = 0; 
            gTickAddressResolved = 0xDEAD;
            return;
        }

        gLocalPlayer = (SDK::Player*)localPlayer;
        gTickAddressResolved = sCalibratedOffset; 
    }

    void Initialize() {
        scannerRunning = true;
        scannerThread = new std::thread(MemoryScannerLoop);
        scannerThread->detach(); 

        printf("[yt] Sinirsiz Full Heap Scanner Thread baslatildi!\n");
    }

    void Terminate() {
        scannerRunning = false;
    }
}
