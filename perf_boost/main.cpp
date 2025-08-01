/*
    Copyright (c) 2017-2023, namreeb (legal@namreeb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
    ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    The views and conclusions contained in the software and documentation are those
    of the authors and should not be interpreted as representing official policies,
    either expressed or implied, of the FreeBSD Project.
*/

#include "logging.hpp"
#include "offsets.hpp"
#include "main.hpp"

#include <cstdint>
#include <memory>
#include <atomic>
#include <map>
#include <set>
#include <string>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <sstream>
#include <ctime>

BOOL WINAPI DllMain(HINSTANCE, uint32_t, void *);

namespace perf_boost {

    const char* VERSION = "1.0.0";

    // Dynamic detour storage system
    std::vector<std::unique_ptr<hadesmem::PatchDetourBase>> gDetours;

    std::unique_ptr<hadesmem::PatchDetour<SpellVisualsInitializeT >> gSpellVisualsInitDetour;

    uintptr_t *gPlayerUnit = nullptr;
    C3Vector gPlayerPosition = {0.0f, 0.0f, 0.0f};
    bool gPlayerInCombat = false;
    bool gPlayerInCity = false;

    bool pbEnabled;

    int playerRenderDist;
    int playerRenderDistInCities;
    int playerRenderDistInCombat;
    int petRenderDist;
    int petRenderDistInCombat;
    int trashUnitRenderDist;
    int trashUnitRenderDistInCombat;
    int corpseRenderDist;
    bool alwaysRenderRaidMarks;

    uint32_t GetTime() {
        return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count()) - gStartTime;
    }

    uint64_t GetWowTimeMs() {
        auto const osGetAsyncTimeMs = reinterpret_cast<GetTimeMsT>(Offsets::OsGetAsyncTimeMs);
        return osGetAsyncTimeMs();
    }

    uintptr_t *GetLuaStatePtr() {
        typedef uintptr_t *(__fastcall *GETCONTEXT)(void);
        static auto p_GetContext = reinterpret_cast<GETCONTEXT>(Offsets::lua_getcontext);
        return p_GetContext();
    }

    float fastApproxDistance(C3Vector &vec) {
        // Manhattan distance (fastest, ~2-3x error)
        // return std::abs(vec.x) + std::abs(vec.y) + std::abs(vec.z);
        
        // Octagonal approximation (fast, ~8% max error)
        float ax = std::abs(vec.x);
        float ay = std::abs(vec.y);
        float az = std::abs(vec.z);
        
        // Sort components so ax >= ay >= az
        if (ax < ay) std::swap(ax, ay);
        if (ay < az) std::swap(ay, az);
        if (ax < ay) std::swap(ax, ay);
        
        // Approximation: largest + 0.5*middle + 0.25*smallest
        return ax + 0.5f * ay + 0.25f * az;
    }

    int ApproximateDistanceBetween(const C3Vector &pos0, const C3Vector &pos1) {
        C3Vector v = {};
        v.x = pos0.x - pos1.x;
        v.y = pos0.y - pos1.y;
        v.z = pos0.z - pos1.z;
        return (int) fastApproxDistance(v);
    }

    bool IsPlayerInCity() {
        // Get current area ID from memory
        uint32_t areaId = *reinterpret_cast<uint32_t*>(Offsets::ZoneAreaIds);
        
        // Major city area IDs for WoW 1.12.1
        switch (areaId) {
            case 1537: // Ironforge
            case 1519: // Stormwind City
            case 1657: // Darnassus
            case 1637: // Orgrimmar
            case 1638: // Thunder Bluff
            case 1497: // Undercity
            case 2040: // Alah'Thalas
            case 75:   // Stonard
                return true;
            default:
                return false;
        }
    }

    int GetRaidMarkForGuid(uint64_t targetGUID) {
        if (targetGUID == 0) {
            return -1;
        }

        for (int result = 0; result < 8; ++result) {
            if (*reinterpret_cast<uint64_t*>(0xb71368 + result * 8) == targetGUID) {
                return result + 1;
            }
        }

        return -1;
    }

    C3Vector UnitGetPosition(uintptr_t *unit) {
        C3Vector result = {};

        if (unit == 0 || ((int) unit & 1) != 0) {
            return result;
        }

        uint32_t memberFunctions = *reinterpret_cast<uint32_t *>(unit);
        if (memberFunctions == 0 || (memberFunctions & 1) != 0) {
            return result;
        }

        uint32_t getPositionPtr = *reinterpret_cast<uint32_t *>(memberFunctions + 0x14);
        if (getPositionPtr == 0 || (getPositionPtr & 1) != 0) {
            return result;
        }

        auto getPositionFn = reinterpret_cast<CGUnitGetPositionT>(getPositionPtr);
        getPositionFn(unit, &result);

        return result;
    }

    OBJECT_TYPE_ID UnitGetType(uintptr_t *unit) {
        return *reinterpret_cast<OBJECT_TYPE_ID *>(unit + 5);
    }

    uint32_t UnitGetLevel(uintptr_t *unit) {
        if (!unit) {
            return false;
        }

        uint32_t attr = *reinterpret_cast<uintptr_t *>(unit + 68);
        if (attr == 0 || (attr & 1) != 0) {
            // we don't have attribute info.
            return -1;
        }

        uint32_t level = *reinterpret_cast<uint32_t *>(attr + 0x70);
        return level;
    }

    uint64_t UnitGetGuid(uintptr_t *unit) {
        if (!unit) {
            return 0;
        }

        uint64_t guid = *reinterpret_cast<uint64_t *>(unit + 12);
        return guid;
    }

    bool UnitIsDead(uintptr_t *unit) {
        if (!unit) {
            return false;
        }

        uint32_t attr = *reinterpret_cast<uintptr_t *>(unit + 68);
        if (attr == 0 || (attr & 1) != 0) {
            // we don't have attribute info.
            return -1;
        }

        uint32_t flag0 = *reinterpret_cast<uint32_t *>(attr + 0x40);
        uint32_t flag1 = *reinterpret_cast<uint32_t *>(attr + 0x224);

        if (flag0 < 1 || (flag1 & 0x20) != 0) {
            return true;
        } else {
            return false;
        }
    }

    bool UnitIsControlledByPlayer(uintptr_t *unit) {
        if (!unit) {
            return false;
        }

        uint32_t attr = *reinterpret_cast<uintptr_t *>(unit + 68);
        if (attr == 0 || (attr & 1) != 0) {
            // we don't have attribute info.
            return -1;
        }

        uint32_t data = *reinterpret_cast<uint32_t*>(attr + 0xa0);
        if ((data & 8) != 0) {
            return true;
        } else {
            return false;
        }
    }

    bool UnitIsInCombat(uintptr_t *unit) {
        if (!unit) {
            return false;
        }

        uint32_t attr = *reinterpret_cast<uintptr_t *>(unit + 68);
        if (attr == 0 || (attr & 1) != 0) {
            // we don't have attribute info.
            return -1;
        }

        uint32_t flags = *reinterpret_cast<uint32_t *>(attr + 0xa0);
        if ((flags & 0x80000) != 0) {
            return true;
        } else {
            return false;
        }
    }

    bool UnitCanAttackUnit(uintptr_t *unit1, uintptr_t *unit2) {
        if (!unit1 || !unit2) {
            return false;
        }

        auto canAttackFn = reinterpret_cast<CGUnitCanAttackT>(Offsets::CGUnitCanAttack);
        return canAttackFn(unit1, unit2);
    }

    std::uint64_t ClntObjMgrGetActivePlayerGuid() {
        auto const getActivePlayer = hadesmem::detail::AliasCast<decltype(&ClntObjMgrGetActivePlayerGuid)>(
                Offsets::GetActivePlayer);

        return getActivePlayer();
    }

    uintptr_t *GetObjectPtr(std::uint64_t guid) {
        uintptr_t *(__stdcall *getObjectPtr)(std::uint64_t) = hadesmem::detail::AliasCast<decltype(getObjectPtr)>(
                Offsets::GetObjectPtr);

        return getObjectPtr(guid);
    }

    uintptr_t *ClntObjMgrObjectPtr(OBJECT_TYPE_MASK typeMask, std::uint64_t guid) {
        auto const clntObjMgrObjectPtr = reinterpret_cast<ClntObjMgrObjectPtrT>(Offsets::ClntObjMgrObjectPtr);
        return clntObjMgrObjectPtr(typeMask, nullptr, guid, 0);
    }


    void OnWorldRenderHook(hadesmem::PatchDetourBase *detour, uintptr_t *worldFrame) {
        // store player data once before ShouldRender is called
        auto playerGuid = ClntObjMgrGetActivePlayerGuid();
        if (playerGuid != 0) {
            gPlayerUnit = GetObjectPtr(ClntObjMgrGetActivePlayerGuid());
            if (gPlayerUnit) {
                gPlayerPosition = UnitGetPosition(gPlayerUnit);
                gPlayerInCombat = UnitIsInCombat(gPlayerUnit);
                gPlayerInCity = IsPlayerInCity();
            }
        }

        auto const OnWorldRender = detour->GetTrampolineT<FastcallFrameT>();
        OnWorldRender(worldFrame);
    }

    void CGUnitPreAnimateHook(hadesmem::PatchDetourBase *detour, uintptr_t *this_ptr, void *dummy_edx, void *param_1) {
        auto const CGUnitPreAnimate = detour->GetTrampolineT<CGUnitPreAnimateT>();
        CGUnitPreAnimate(this_ptr, dummy_edx, param_1);
    }

    void CGUnitAnimateHook(hadesmem::PatchDetourBase *detour, uintptr_t *this_ptr, void *dummy_edx, int *param_3) {
        auto const CGUnitAnimate = detour->GetTrampolineT<CGUnitAnimateT>();
        CGUnitAnimate(this_ptr, dummy_edx, param_3);
    }

    bool ShouldRenderBasedOnDistance(uintptr_t *this_ptr, int renderDist) {
        if (renderDist == 0) {
            return false; // if dist 0 immediately return false
        } else if (renderDist > 0) {
            auto distance = ApproximateDistanceBetween(UnitGetPosition(this_ptr), gPlayerPosition);
            return distance < renderDist;
        }
        return true; // default to true if renderDist is negative
    }

    uint32_t
    CGUnitShouldRenderHook(hadesmem::PatchDetourBase *detour, uintptr_t *unitPtr, void *dummy_edx, uint32_t param_1) {
        auto const CGUnitShouldRender = detour->GetTrampolineT<CGUnitShouldRenderT>();
        uint32_t result = CGUnitShouldRender(unitPtr, dummy_edx, param_1);
        if (!pbEnabled) {
            return result;
        }
        if (result == 1 && gPlayerUnit) {
            if (unitPtr != gPlayerUnit) {
                auto unitType = UnitGetType(unitPtr);
                if (unitType == OBJECT_TYPE_PLAYER) {
                    if (alwaysRenderRaidMarks){
                        auto raidMark = GetRaidMarkForGuid(UnitGetGuid(unitPtr));

                        if (raidMark > 0) {
                            // always render players with raid marks
                            return result;
                        }
                    }
                    // always show pvp/mc players
                    if (UnitCanAttackUnit(gPlayerUnit, unitPtr)) {
                        return result;
                    }

                    int renderDist;
                    if (gPlayerInCombat && playerRenderDistInCombat != -1) {
                        renderDist = playerRenderDistInCombat;
                    } else if (gPlayerInCity && playerRenderDistInCities != -1) {
                        renderDist = playerRenderDistInCities;
                    } else {
                        renderDist = playerRenderDist;
                    }
                    return ShouldRenderBasedOnDistance(unitPtr, renderDist);
                } else if (unitType == OBJECT_TYPE_UNIT) {
                    if (alwaysRenderRaidMarks){
                        auto raidMark = GetRaidMarkForGuid(UnitGetGuid(unitPtr));

                        if (raidMark > 0) {
                            // always render raid marks
                            return result;
                        }
                    }

                    auto isDead = UnitIsDead(unitPtr);
                    if (isDead && corpseRenderDist != -1) {
                        // some corpses (lootable ones?) are dead units
                        int renderDist = corpseRenderDist;
                        return ShouldRenderBasedOnDistance(unitPtr, renderDist);
                    } else {
                        // Check if it's a pet (controlled by player)
                        if (UnitIsControlledByPlayer(unitPtr)) {
                            int renderDist = (gPlayerInCombat && petRenderDistInCombat != -1) ? petRenderDistInCombat : petRenderDist;
                            return ShouldRenderBasedOnDistance(unitPtr, renderDist);
                        }
                        
                        auto unitLevel = UnitGetLevel(unitPtr);
                        if (unitLevel < 63) {
                            int renderDist = (gPlayerInCombat && trashUnitRenderDistInCombat != -1) ? trashUnitRenderDistInCombat : trashUnitRenderDist;
                            return ShouldRenderBasedOnDistance(unitPtr, renderDist);
                        }
                    }
                } else if (unitType == OBJECT_TYPE_CORPSE && corpseRenderDist != -1) {
                    int renderDist = corpseRenderDist;
                    return ShouldRenderBasedOnDistance(unitPtr, renderDist);
                }
            }
        }
        return result;
    }

    void updateFromCvar(const char *cvar, const char *value) {
        if (strcmp(cvar, "PB_PlayerRenderDist") == 0) {
            playerRenderDist = atoi(value);
            DEBUG_LOG("Set PB_PlayerRenderDist to " << playerRenderDist);
        } else if (strcmp(cvar, "PB_PlayerRenderDistInCities") == 0) {
            playerRenderDistInCities = atoi(value);
            DEBUG_LOG("Set PB_PlayerRenderDistInCities to " << playerRenderDistInCities);
        } else if (strcmp(cvar, "PB_PlayerRenderDistInCombat") == 0) {
            playerRenderDistInCombat = atoi(value);
            DEBUG_LOG("Set PB_PlayerRenderDistInCombat to " << playerRenderDistInCombat);
        } else if (strcmp(cvar, "PB_PetRenderDist") == 0) {
            petRenderDist = atoi(value);
            DEBUG_LOG("Set PB_PetRenderDist to " << petRenderDist);
        } else if (strcmp(cvar, "PB_PetRenderDistInCombat") == 0) {
            petRenderDistInCombat = atoi(value);
            DEBUG_LOG("Set PB_PetRenderDistInCombat to " << petRenderDistInCombat);
        } else if (strcmp(cvar, "PB_TrashUnitRenderDist") == 0) {
            trashUnitRenderDist = atoi(value);
            DEBUG_LOG("Set PB_TrashUnitRenderDist to " << trashUnitRenderDist);
        } else if (strcmp(cvar, "PB_TrashUnitRenderDistInCombat") == 0) {
            trashUnitRenderDistInCombat = atoi(value);
            DEBUG_LOG("Set PB_TrashUnitRenderDistInCombat to " << trashUnitRenderDistInCombat);
        } else if (strcmp(cvar, "PB_CorpseRenderDist") == 0) {
            corpseRenderDist = atoi(value);
            DEBUG_LOG("Set PB_CorpseRenderDist to " << corpseRenderDist);
        } else if (strcmp(cvar, "PB_Enabled") == 0) {
            pbEnabled = atoi(value) != 0;
            DEBUG_LOG("Set PB_Enabled to " << pbEnabled);
        } else if (strcmp(cvar, "PB_AlwaysRenderRaidMarks") == 0) {
            alwaysRenderRaidMarks = atoi(value) != 0;
            DEBUG_LOG("Set PB_AlwaysRenderRaidMarks to " << alwaysRenderRaidMarks);
        }
    }

    int Script_SetCVarHook(hadesmem::PatchDetourBase *detour, uintptr_t *luaPtr) {
        auto const cvarSetOrig = detour->GetTrampolineT<SetCVarT>();

        auto const lua_isstring = reinterpret_cast<lua_isstringT>(Offsets::lua_isstring);
        if (lua_isstring(luaPtr, 1)) {
            auto const lua_tostring = reinterpret_cast<lua_tostringT>(Offsets::lua_tostring);
            auto const cVarName = lua_tostring(luaPtr, 1);
            auto const cVarValue = lua_tostring(luaPtr, 2);
            // if cvar starts with "PB_", then we need to handle it
            if (strncmp(cVarName, "PB_", 3) == 0) {
                updateFromCvar(cVarName, cVarValue);
            }
        } // original function handles errors

        return cvarSetOrig(luaPtr);
    }

    int *getCvar(const char *cvar) {
        auto const cvarLookup = hadesmem::detail::AliasCast<CVarLookupT>(Offsets::CVarLookup);
        uintptr_t *cvarPtr = cvarLookup(cvar);

        if (cvarPtr) {
            return reinterpret_cast<int *>(cvarPtr +
                                           10); // get intValue from CVar which is consistent, strValue more complicated
        }
        return nullptr;
    }

    void loadUserVar(const char *cvar) {
        int *value = getCvar(cvar);
        if (value) {
            updateFromCvar(cvar, std::to_string(*value).c_str());
        } else {
            DEBUG_LOG("Using default value for " << cvar);
        }
    }

    // Template function to simplify hook initialization with dynamic storage
    template<typename FuncT, typename HookT>
    void initializeHook(const hadesmem::Process &process, Offsets offset, HookT hookFunc) {
        auto const originalFunc = hadesmem::detail::AliasCast<FuncT>(offset);
        auto detour = std::make_unique<hadesmem::PatchDetour<FuncT>>(process, originalFunc, hookFunc);
        detour->Apply();
        gDetours.push_back(std::move(detour));
    }

    void initHooks() {
        const hadesmem::Process process(::GetCurrentProcessId());

        initializeHook<FastcallFrameT>(process, Offsets::OnWorldRender, &OnWorldRenderHook);
        initializeHook<SetCVarT>(process, Offsets::Script_SetCVar, &Script_SetCVarHook);

        // Hook CGUnit functions
        initializeHook<CGUnitPreAnimateT>(process, Offsets::CGUnitPreAnimate, &CGUnitPreAnimateHook);
        initializeHook<CGUnitAnimateT>(process, Offsets::CGUnitAnimate, &CGUnitAnimateHook);
        initializeHook<CGUnitShouldRenderT>(process, Offsets::CGUnitShouldRender, &CGUnitShouldRenderHook);
    }

    void loadConfig() {
        gStartTime = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count());

        // remove/rename previous logs
        remove("perf_boost.log.3");
        rename("perf_boost.log.2", "perf_boost.log.3");
        rename("perf_boost.log.1", "perf_boost.log.2");
        rename("perf_boost.log", "perf_boost.log.1");

        // open new log file
        debugLogFile.open("perf_boost.log");

        DEBUG_LOG("Loading perf_boost v" << VERSION);

        DEBUG_LOG("Registering/Loading CVars");

        char defaultUnset[] = "-1";

        // register cvars
        auto const CVarRegister = hadesmem::detail::AliasCast<CVarRegisterT>(Offsets::RegisterCVar);


        char defaultEnabled[] = "1";

        // Enable/disable all performance boost features
        char PB_Enabled[] = "PB_Enabled";
        CVarRegister(PB_Enabled, // name
                     nullptr, // help
                     0,  // unk1
                     defaultEnabled, // default value address
                     nullptr, // callback
                     5, // category
                     0,  // unk2
                     0); // unk3

        // Max distance to render players when not in combat
        char PB_PlayerRenderDistance[] = "PB_PlayerRenderDist";
        CVarRegister(PB_PlayerRenderDistance, // name
                     nullptr, // help
                     0,  // unk1
                     defaultUnset, // default value address
                     nullptr, // callback
                     5, // category
                     0,  // unk2
                     0); // unk3

        // Max distance to render players when in cities
        char PB_PlayerRenderDistInCities[] = "PB_PlayerRenderDistInCities";
        CVarRegister(PB_PlayerRenderDistInCities, // name
                     nullptr, // help
                     0,  // unk1
                     defaultUnset, // default value address
                     nullptr, // callback
                     5, // category
                     0,  // unk2
                     0); // unk3

        // Max distance to render players when in combat
        char PB_PlayerRenderDistInCombat[] = "PB_PlayerRenderDistInCombat";
        CVarRegister(PB_PlayerRenderDistInCombat, // name
                     nullptr, // help
                     0,  // unk1
                     defaultUnset, // default value address
                     nullptr, // callback
                     5, // category
                     0,  // unk2
                     0); // unk3

        // Max distance to render pets when not in combat
        char PB_PetRenderDist[] = "PB_PetRenderDist";
        CVarRegister(PB_PetRenderDist, // name
                     nullptr, // help
                     0,  // unk1
                     defaultUnset, // default value address
                     nullptr, // callback
                     5, // category
                     0,  // unk2
                     0); // unk3

        // Max distance to render pets when in combat
        char PB_PetRenderDistInCombat[] = "PB_PetRenderDistInCombat";
        CVarRegister(PB_PetRenderDistInCombat, // name
                     nullptr, // help
                     0,  // unk1
                     defaultUnset, // default value address
                     nullptr, // callback
                     5, // category
                     0,  // unk2
                     0); // unk3

        // Max distance to render trash units when not in combat
        char PB_TrashUnitRenderDist[] = "PB_TrashUnitRenderDist";
        CVarRegister(PB_TrashUnitRenderDist, // name
                     nullptr, // help
                     0,  // unk1
                     defaultUnset, // default value address
                     nullptr, // callback
                     5, // category
                     0,  // unk2
                     0); // unk3

        // Max distance to render trash units when in combat
        char PB_TrashUnitRenderDistInCombat[] = "PB_TrashUnitRenderDistInCombat";
        CVarRegister(PB_TrashUnitRenderDistInCombat, // name
                     nullptr, // help
                     0,  // unk1
                     defaultUnset, // default value address
                     nullptr, // callback
                     5, // category
                     0,  // unk2
                     0); // unk3

        // Max distance to render corpses
        char PB_CorpseRenderDist[] = "PB_CorpseRenderDist";
        CVarRegister(PB_CorpseRenderDist, // name
                     nullptr, // help
                     0,  // unk1
                     defaultUnset, // default value address
                     nullptr, // callback
                     5, // category
                     0,  // unk2
                     0); // unk3

        // Whether to always render
        char PB_AlwaysRenderRaidMarks[] = "PB_AlwaysRenderRaidMarks";
        CVarRegister(PB_AlwaysRenderRaidMarks, // name
                     nullptr, // help
                     0,  // unk1
                     defaultEnabled, // default value address
                     nullptr, // callback
                     5, // category
                     0,  // unk2
                     0); // unk3


        loadUserVar("PB_PlayerRenderDist");
        loadUserVar("PB_PlayerRenderDistInCities");
        loadUserVar("PB_PlayerRenderDistInCombat");
        loadUserVar("PB_PetRenderDist");
        loadUserVar("PB_PetRenderDistInCombat");
        loadUserVar("PB_TrashUnitRenderDist");
        loadUserVar("PB_TrashUnitRenderDistInCombat");
        loadUserVar("PB_CorpseRenderDist");
        loadUserVar("PB_Enabled");
        loadUserVar("PB_AlwaysRenderRaidMarks");
    }

    void SpellVisualsInitializeHook(hadesmem::PatchDetourBase *detour) {
        auto const spellVisualsInitialize = detour->GetTrampolineT<SpellVisualsInitializeT>();
        spellVisualsInitialize();
        loadConfig();
        initHooks();
    }

    std::once_flag loadFlag;

    void load() {
        std::call_once(loadFlag, []() {
                           // hook spell visuals initialize
                           const hadesmem::Process process(::GetCurrentProcessId());

                           auto const spellVisualsInitOrig = hadesmem::detail::AliasCast<SpellVisualsInitializeT>(
                                   Offsets::SpellVisualsInitialize);
                           gSpellVisualsInitDetour = std::make_unique<hadesmem::PatchDetour<SpellVisualsInitializeT>>(process,
                                                                                                                      spellVisualsInitOrig,
                                                                                                                      &SpellVisualsInitializeHook);
                           gSpellVisualsInitDetour->Apply();
                       }
        );
    }
}

extern "C" __declspec(dllexport) uint32_t Load() {
    perf_boost::load();
    return EXIT_SUCCESS;
}
