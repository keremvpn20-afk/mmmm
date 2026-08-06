#include "hooks.hpp"
#include "sdk.hpp"
#include "memory.hpp"
#include <mutex>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>

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
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            
            gTickAddressResolved = 0x7777; // Calisiyor isareti

            if (!triggerMemoryDump) {
                continue; // Sadece DUMP butonuna basinca tarama yapar
            }

            if (!gLocalPlayer) {
                triggerMemoryDump = false;
                continue;
            }

            SDK::Vector3 playerPos = SDK::GetPlayerPosition((uintptr_t)gLocalPlayer);
            int pX = (int)std::floor(playerPos.x);
            int pY = (int)std::floor(playerPos.y);
            int pZ = (int)std::floor(playerPos.z);

            // iOS app sandbox dokumanlar klasoru
            NSString *docPath = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) firstObject];
            NSString *filePath = [docPath stringByAppendingPathComponent:@"minecraft_memory_dump.txt"];
            std::ofstream dumpFile([filePath UTF8String], std::ios_base::app);
            
            dumpFile << "\n============================================\n";
            dumpFile << "MEMORY DUMP STARTED! Player Position: X=" << pX << " Y=" << pY << " Z=" << pZ << "\n";
            dumpFile << "============================================\n";

            mach_port_t task = mach_task_self();
            vm_address_t address = 0;
            vm_size_t size = 0;
            vm_region_basic_info_data_64_t info;
            mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
            mach_port_t object_name;

            int foundCount = 0;
            uintptr_t base = Memory::GetBaseAddress();

            while (vm_region_64(task, &address, &size, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info, &info_count, &object_name) == KERN_SUCCESS) {
                
                if ((info.protection & VM_PROT_READ) && (info.protection & VM_PROT_WRITE)) {
                    
                    size_t chunkSize = 1024 * 1024;
                    uint8_t* buf = (uint8_t*)malloc(chunkSize);
                    
                    if (buf) {
                        for (vm_address_t chunkStart = address; chunkStart < address + size; chunkStart += chunkSize) {
                            vm_size_t readSize = (address + size - chunkStart < chunkSize) ? (address + size - chunkStart) : chunkSize;
                            vm_size_t readCount = readSize;
                            
                            if (vm_read_overwrite(task, chunkStart, readSize, (vm_address_t)buf, &readCount) == KERN_SUCCESS) {
                                
                                for (size_t offset = 0x100; offset < readSize - 0x100; offset += 4) {
                                    
                                    int bx = *(int*)(buf + offset);
                                    int by = *(int*)(buf + offset + 4);
                                    int bz = *(int*)(buf + offset + 8);
                                    
                                    // Oyuncu Sandigin Ustundeyse (Y veya Y-1 eslesir)
                                    if (bx == pX && bz == pZ && (by == pY || by == pY - 1 || by == pY - 2)) {
                                        
                                        dumpFile << "\n--- POTENTIAL BLOCK ENTITY FOUND AT ADDR: 0x" << std::hex << (chunkStart + offset) << std::dec << " ---\n";
                                        dumpFile << "Match Coords: X=" << bx << " Y=" << by << " Z=" << bz << "\n";
                                        dumpFile << "Dumping Memory 256 bytes BEFORE and 256 bytes AFTER:\n";
                                        
                                        for (int i = -0x100; i <= 0x100; i += 4) {
                                            uintptr_t addr = chunkStart + offset + i;
                                            int val_int = *(int*)(buf + offset + i);
                                            float val_float = *(float*)(buf + offset + i);
                                            uintptr_t val_ptr = *(uintptr_t*)(buf + offset + i);
                                            
                                            dumpFile << "Offset " << (i > 0 ? "+" : "") << std::hex << i << std::dec 
                                                     << "\t| Int: " << val_int 
                                                     << "\t| Float: " << val_float 
                                                     << "\t| Ptr: 0x" << std::hex << val_ptr << std::dec << "\n";
                                        }
                                        dumpFile << "-------------------------------------------\n";
                                        
                                        foundCount++;
                                    }
                                }
                            }
                            if (foundCount > 100) break; 
                        }
                        free(buf);
                    }
                }
                address += size;
                if (foundCount > 100) break;
            }
            
            dumpFile << "\nDump Complete! Found " << foundCount << " matching coordinate blocks.\n";
            dumpFile.close();
            
            gDebugBlockSource = 0xD000D000; 
            triggerMemoryDump = false; 
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
    }

    void Terminate() {
        scannerRunning = false;
    }
}
