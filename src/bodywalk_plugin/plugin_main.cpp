#include <windows.h>
#include <sddl.h>
#include <string>
#include <thread>
#include <atomic>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cctype>
// Include the BodyWalk plugin API
#include "../../../../vjoy/cpp_src/src/plugin_api.h"

#define PLUGIN_VERSION "1.4"

struct CP2077StateData {
    char activeCategory[64];
};

static BW_HostCallbacks g_callbacks;
static std::thread g_workerThread;
static std::atomic<bool> g_running{false};
static HANDLE g_hStateMap = NULL;
static CP2077StateData* g_pStateData = nullptr;
static std::string g_lastCategory = "";
static bool g_categoriesRegistered = false;

// The universal tracking data struct expected by CP2077 RED4ext mod
#pragma pack(push, 1)
struct UniversalTrackingData {
    uint32_t version;          // Struct version, currently 1
    uint32_t updateCounter;    // Increments every frame
    
    struct Pose {
        float pos[3];          // X, Y, Z
        float rot[4];          // X, Y, Z, W
        
        // Input data
        float trigger;         // 0.0 to 1.0
        float grip;            // 0.0 to 1.0
        float stickX;          // -1.0 to 1.0
        float stickY;          // -1.0 to 1.0
        uint64_t buttons;      // Bitmask of pressed buttons

        uint32_t valid;        // 1 if tracking, 0 otherwise
        uint32_t padding;      // Align to 8 bytes for some languages
    };

    Pose head;
    Pose leftHand;
    Pose rightHand;
};
#pragma pack(pop)

static HANDLE g_hTrackingMap = NULL;
static UniversalTrackingData* g_pTrackingData = nullptr;

struct CP2077ActionData {
    uint64_t actionFlags;
};

static HANDLE g_hActionMap = NULL;
static CP2077ActionData* g_pActionData = nullptr;

const char* g_cp2077Actions[] = {
    "CP2077: Weapon 1",         // index 0
    "CP2077: Weapon 2",         // index 1
    "CP2077: Weapon 3",         // index 2
    "CP2077: Next Weapon",      // index 3
    "CP2077: Holster Weapon",   // index 4
    "CP2077: Use Combat Stim"   // index 5
};
const int NUM_ACTIONS = sizeof(g_cp2077Actions) / sizeof(g_cp2077Actions[0]);

void StateMonitorLoop() {
    g_hStateMap = OpenFileMappingA(FILE_MAP_READ, FALSE, "Local\\BodyWalkVR_CP2077_State");
    if (!g_hStateMap) {
        // Retry loop in case game isn't open yet
        while (g_running) {
            g_hStateMap = OpenFileMappingA(FILE_MAP_READ, FALSE, "Local\\BodyWalkVR_CP2077_State");
            if (g_hStateMap) break;
            Sleep(1000);
        }
    }

    if (g_hStateMap) {
        g_pStateData = (CP2077StateData*)MapViewOfFile(g_hStateMap, FILE_MAP_READ, 0, 0, sizeof(CP2077StateData));
        if (g_pStateData && !g_categoriesRegistered && g_callbacks.register_mapping_category) {
            g_callbacks.register_mapping_category("main", "General gameplay and exploration.");
            g_callbacks.register_mapping_category("combat", "Active combat or stealth.");
            g_callbacks.register_mapping_category("menu", "In-game UI and menus.");
            g_callbacks.register_mapping_category("vehicle", "Driving or riding a vehicle.");
            g_callbacks.register_mapping_category("scene", "Braindance or interactive scenes.");
            g_callbacks.register_mapping_category("unarmed", "Bare fists or Gorilla Arms.");
            g_callbacks.register_mapping_category("melee", "Bladed or blunt melee weapons.");
            g_callbacks.register_mapping_category("pistols", "Pistols and revolvers.");
            g_callbacks.register_mapping_category("rifles", "Assault, sniper rifles, and SMGs.");
            g_callbacks.register_mapping_category("shotguns", "Shotguns.");
            g_callbacks.register_mapping_category("heavy", "LMGs, HMGs, and launchers.");
            g_categoriesRegistered = true;
        }
    }

    while (g_running) {
        if (g_pStateData) {
            std::string currentCat = g_pStateData->activeCategory;
            if (!currentCat.empty()) {
                // Convert to lowercase
                std::transform(currentCat.begin(), currentCat.end(), currentCat.begin(),
                    [](unsigned char c){ return std::tolower(c); });
                
                if (currentCat != g_lastCategory) {
                    g_lastCategory = currentCat;
                    if (g_callbacks.set_active_mapping_category) {
                        g_callbacks.set_active_mapping_category(currentCat.c_str());
                    }
                }
            }
        } else {
            // Re-attempt connecting if we lost it
            g_hStateMap = OpenFileMappingA(FILE_MAP_READ, FALSE, "Local\\BodyWalkVR_CP2077_State");
            if (g_hStateMap) {
                g_pStateData = (CP2077StateData*)MapViewOfFile(g_hStateMap, FILE_MAP_READ, 0, 0, sizeof(CP2077StateData));
                if (g_pStateData && !g_categoriesRegistered && g_callbacks.register_mapping_category) {
                    g_callbacks.register_mapping_category("main", "General gameplay and exploration.");
                    g_callbacks.register_mapping_category("combat", "Active combat or stealth.");
                    g_callbacks.register_mapping_category("menu", "In-game UI and menus.");
                    g_callbacks.register_mapping_category("vehicle", "Driving or riding a vehicle.");
                    g_callbacks.register_mapping_category("scene", "Braindance or interactive scenes.");
                    g_callbacks.register_mapping_category("unarmed", "Bare fists or Gorilla Arms.");
                    g_callbacks.register_mapping_category("melee", "Bladed or blunt melee weapons.");
                    g_callbacks.register_mapping_category("pistols", "Pistols and revolvers.");
                    g_callbacks.register_mapping_category("rifles", "Assault, sniper rifles, and SMGs.");
                    g_callbacks.register_mapping_category("shotguns", "Shotguns.");
                    g_callbacks.register_mapping_category("heavy", "LMGs, HMGs, and launchers.");
                    g_categoriesRegistered = true;
                }
            }
        }

        // Handle Action shared memory (Write)
        if (!g_hActionMap) {
            g_hActionMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(CP2077ActionData), "Local\\BodyWalkVR_CP2077_ActionData");
            if (g_hActionMap) {
                g_pActionData = (CP2077ActionData*)MapViewOfFile(g_hActionMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(CP2077ActionData));
                if (g_pActionData) {
                    g_pActionData->actionFlags = 0;
                    std::ofstream log("cp2077_bridge.log", std::ios::app);
                    log << "Created Action Shared Memory successfully." << std::endl;
                } else {
                    std::ofstream log("cp2077_bridge.log", std::ios::app);
                    log << "Failed to map Action Shared Memory. Error: " << GetLastError() << std::endl;
                }
            } else {
                std::ofstream log("cp2077_bridge.log", std::ios::app);
                log << "Failed to create Action Shared Memory. Error: " << GetLastError() << std::endl;
            }
        }

        Sleep(50); // poll at 20Hz
    }
}

extern "C" {
    __declspec(dllexport) bool BW_Plugin_Initialize(const BW_HostCallbacks* callbacks, BW_PluginInfo* out_info) {
        g_callbacks = *callbacks;

        out_info->name = "CP2077 Bridge";
        out_info->version = PLUGIN_VERSION;
        out_info->author = "Natpoh";
        out_info->type = BW_PLUGIN_TYPE_BOTH;
        out_info->input_source_name = "CP2077 Shared Memory";
        out_info->output_mode_name = "CP2077 Actions";

        if (g_callbacks.register_action) {
            for (int i = 0; i < NUM_ACTIONS; i++) {
                g_callbacks.register_action(g_cp2077Actions[i]);
            }
        }

        g_running = true;
        g_workerThread = std::thread(StateMonitorLoop);
        
        return true; // Success
    }

    __declspec(dllexport) void BW_Plugin_Update() {
        // Nothing to do in update, we use a worker thread
    }

    __declspec(dllexport) void BW_Plugin_Shutdown() {
        g_running = false;
        if (g_workerThread.joinable()) {
            g_workerThread.join();
        }

        if (g_pStateData) {
            UnmapViewOfFile(g_pStateData);
            g_pStateData = nullptr;
        }
        if (g_hStateMap) {
            CloseHandle(g_hStateMap);
            g_hStateMap = NULL;
        }
        if (g_pActionData) {
            UnmapViewOfFile(g_pActionData);
            g_pActionData = nullptr;
        }
        if (g_hActionMap) {
            CloseHandle(g_hActionMap);
            g_hActionMap = NULL;
        }
        if (g_pTrackingData) {
            UnmapViewOfFile(g_pTrackingData);
            g_pTrackingData = nullptr;
        }
        if (g_hTrackingMap) {
            CloseHandle(g_hTrackingMap);
            g_hTrackingMap = NULL;
        }
    }

    __declspec(dllexport) void BW_Plugin_GetStatus(BW_PluginStatus* out_status) {
        if (!out_status) return;

        out_status->has_action = false;
        out_status->action_name[0] = '\0';

        if (g_pStateData != nullptr) {
            out_status->code = BW_STATUS_OK;
            strncpy_s(out_status->message, "CONNECTED TO GAME", sizeof(out_status->message) - 1);
        } else {
            out_status->code = BW_STATUS_WARNING;
            strncpy_s(out_status->message, "WAITING FOR GAME", sizeof(out_status->message) - 1);
        }
    }

    __declspec(dllexport) void BW_Plugin_ReceiveAction(const char* action_name, bool active) {
        std::ofstream log("cp2077_bridge.log", std::ios::app);
        log << "ReceiveAction: " << action_name << " | active: " << active << std::endl;
        
        if (!g_pActionData) {
            log << "  -> ERROR: g_pActionData is null! Shared memory not ready." << std::endl;
            return;
        }

        for (int i = 0; i < NUM_ACTIONS; i++) {
            if (strcmp(g_cp2077Actions[i], action_name) == 0) {
                if (active) {
                    g_pActionData->actionFlags |= (1ULL << i);
                } else {
                    g_pActionData->actionFlags &= ~(1ULL << i);
                }
                log << "  -> Set flag bit " << i << ". Current flags: " << g_pActionData->actionFlags << std::endl;
                break;
            }
        }
    }

    __declspec(dllexport) void BW_Plugin_ReceiveTracking(const BW_TrackingData* data) {
        if (!data) return;

        // Initialize shared memory for the game if it hasn't been created yet
        if (!g_hTrackingMap) {
            SECURITY_ATTRIBUTES sa;
            sa.nLength = sizeof(sa);
            sa.bInheritHandle = FALSE;
            const char* sddl = "D:(A;;GA;;;WD)S:(ML;;NW;;;LW)";
            PSECURITY_DESCRIPTOR pSD = NULL;
            if (ConvertStringSecurityDescriptorToSecurityDescriptorA(sddl, SDDL_REVISION_1, &pSD, NULL)) {
                sa.lpSecurityDescriptor = pSD;
            } else {
                sa.lpSecurityDescriptor = NULL;
            }

            g_hTrackingMap = CreateFileMappingA(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, sizeof(UniversalTrackingData), "Local\\BodyWalkVR_UniversalTracking");
            if (pSD) LocalFree(pSD);

            if (g_hTrackingMap) {
                g_pTrackingData = (UniversalTrackingData*)MapViewOfFile(g_hTrackingMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(UniversalTrackingData));
                if (g_pTrackingData) {
                    memset(g_pTrackingData, 0, sizeof(UniversalTrackingData));
                }
            }
        }

        if (g_pTrackingData) {
            // Memory layout of BW_TrackingData is identical to UniversalTrackingData
            memcpy(g_pTrackingData, data, sizeof(UniversalTrackingData));
        }
    }
}
