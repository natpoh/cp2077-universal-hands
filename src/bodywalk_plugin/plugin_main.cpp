#include <windows.h>
#include <string>
#include <thread>
#include <atomic>
#include <iostream>
#include <algorithm>
#include <cctype>
// Include the BodyWalk plugin API
#include "../../../../vjoy/cpp_src/src/plugin_api.h"

#define PLUGIN_VERSION "1.0.0"

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
        Sleep(50); // poll at 20Hz
    }
}

extern "C" {
    __declspec(dllexport) bool BW_Plugin_Initialize(const BW_HostCallbacks* callbacks, BW_PluginInfo* out_info) {
        g_callbacks = *callbacks;

        out_info->name = "CP2077 Bridge";
        out_info->version = PLUGIN_VERSION;
        out_info->author = "Natpoh";
        out_info->type = BW_PLUGIN_TYPE_INPUT;
        out_info->input_source_name = "CP2077 Shared Memory";

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
}
