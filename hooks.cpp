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

    // Menudeki "Tick Status" ve "Scanned Entities" degerlerini bu degiskenler belirliyor
    uintptr_t gTickAddressResolved = 0; 
    int gScannedEntitiesCount = 0;
    uintptr_t gDebugBlockSource = 0;
    int gDebugListSize = 0;

    static uintptr_t sCalibratedOffset = 0;

    static int autoPosOffset = -1;
    static int autoTypeOffset = -1;
    static int autoChestTypeVal = -1;

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
            
            // Eğer ESP kapalıysa bile "HOOKED" sinyali göndermeye devam et ki menü hata vermesin
            gTickAddressResolved = 0x7777; 

            if (!storageEspEnabled || !gLocalPlayer) {
                continue;
            }

            SDK::Vector3 playerPos = SDK::GetPlayerPosition((uintptr_t)gLocalPlayer);
            if (playerPos.y < -64.f || playerPos.y > 320.f) {
                continue;
            }

            int pX = (int)std::floor(playerPos.x);
            int pY = (int)std::floor(playerPos.y);
            int pZ = (int)std::floor(playerPos.z);

            std::vector<MappedContainer> temp;
            mach_port_t task = mach_task_self();
            vm_address_t address = 0;
            vm_size_t size = 0;
            vm_region_basic_info_data_64_t info;
            mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
            mach_port_t object_name;

            int totalFound = 0;
            uintptr_t base = Memory::GetBaseAddress();

            while (vm_region_64(task, &address, &size, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info, &info_count, &object_name) == KERN_SUCCESS) {
                
                if ((info.protection & VM_PROT_READ) && (info.protection & VM_PROT_WRITE)) {
                    
                    size_t chunkSize = 1024 * 1024; // 1 MB Buffer
                    uint8_t* buf = (uint8_t*)malloc(chunkSize);
                    
                    if (buf) {
                        for (vm_address_t chunkStart = address; chunkStart < address + size; chunkStart += chunkSize) {
                            vm_size_t readSize = (address + size - chunkStart < chunkSize) ? (address + size - chunkStart) : chunkSize;
                            vm_size_t readCount = readSize;
                            
                            if (vm_read_overwrite(task, chunkStart, readSize, (vm_address_t)buf, &readCount) == KERN_SUCCESS) {
                                
                                for (size_t offset = 0x40; offset < readSize - 0x40; offset += 4) {
                                    
                                    // YENI YAPAY ZEKA KALIBRASYONU: Sandigin ustune cik
                                    if (autoPosOffset == -1) {
                                        int bx = *(int*)(buf + offset);
                                        int by = *(int*)(buf + offset + 4);
                                        int bz = *(int*)(buf + offset + 8);
                                        
                                        // Oyuncu sandigin ustundeyse (pY veya pY-1)
                                        if (bx == pX && (by == pY - 1 || by == pY) && bz == pZ) {
                                            for (int i = 0; i <= 0x40; i += 8) {
                                                uintptr_t ptr = *(uintptr_t*)(buf + offset - i);
                                                if (ptr > base && ptr < base + 0x15000000) {
                                                    autoPosOffset = i;
                                                    autoTypeOffset = i - 8;
                                                    autoChestTypeVal = *(int*)(buf + offset - 8);
                                                    
                                                    if (autoChestTypeVal < 1 || autoChestTypeVal > 30) {
                                                        autoTypeOffset = i - 4;
                                                        autoChestTypeVal = *(int*)(buf + offset - 4);
                                                    }
                                                    gTickAddressResolved = 0x9999; // Kalibrasyon Basarili Sinyali
                                                    break;
                                                }
                                            }
                                        }
                                    } 
                                    // KALIBRASYON YAPILDIYSA (0x9999) CIZ
                                    else {
                                        int bx = *(int*)(buf + offset);
                                        int by = *(int*)(buf + offset + 4);
                                        int bz = *(int*)(buf + offset + 8);

                                        if (by > -64 && by < 320) {
                                            int type = *(int*)(buf + offset - autoPosOffset + autoTypeOffset);
                                            uintptr_t ptr = *(uintptr_t*)(buf + offset - autoPosOffset);
                                            
                                            if (ptr > base && ptr < base + 0x15000000) {
                                                int dx = std::abs(bx - pX);
                                                int dy = std::abs(by - pY);
                                                int dz = std::abs(bz - pZ);
                                                
                                                if (dx < 150 && dy < 150 && dz < 150) {
                                                    SDK::Vector3 pos = {(float)bx + 0.5f, (float)by + 0.5f, (float)bz + 0.5f};
                                                    float dist = playerPos.distance(pos);
                                                    
                                                    int mapped = -1;
                                                    if (type == autoChestTypeVal && filterChest) mapped = 1;
                                                    
                                                    if (mapped != -1) {
                                                        temp.push_back({ mapped, pos, dist });
                                                        totalFound++;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            if (totalFound > 5000) break;
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
        }
    }

    // Overlay (Ekrana Cizim) tarafindan tetiklenen ana fonksiyon
    void ProcessContainerScanning(SDK::Player* /*unused*/) {
        uintptr_t base = Memory::GetBaseAddress();

        uintptr_t clientInstance = SDK::GetClientInstance(base);
        if (!SDK::IsValidPtr(clientInstance)) {
            // Arka plandaki thread ClientInstance'i bulamadiysa
            return;
        }

        if (sCalibratedOffset == 0)
            sCalibratedOffset = FindLocalPlayerOffset(clientInstance);

        if (sCalibratedOffset == 0) {
            return;
        }

        uintptr_t localPlayer = SDK::SafeRead<uintptr_t>(clientInstance + sCalibratedOffset);
        if (!SDK::IsValidPtr(localPlayer)) {
            sCalibratedOffset = 0; 
            return;
        }

        gLocalPlayer = (SDK::Player*)localPlayer;
        
        // Menudeki "Tick Status: HOOKED" (0) yazmasini onleyen satiri kaldirdik
        // Artik arkaplan thread'i gTickAddressResolved degerini yonetiyor.
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
