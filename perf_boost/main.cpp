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

    const char *VERSION = "1.3.1";

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
    int summonRenderDist;
    int summonRenderDistInCombat;
    int trashUnitRenderDist;
    int trashUnitRenderDistInCombat;
    int corpseRenderDist;
    bool alwaysRenderRaidMarks;
    bool hideAllPlayers;
    bool filterGuidEvents;

    bool showPlayerSpellVisuals;
    bool showPlayerGroundEffects;
    bool showPlayerAuraVisuals;
    bool showUnitAuraVisuals;
    bool hideSpellsForHiddenPlayers;

    std::string hiddenSpellIdsString;
    std::vector<uint32_t> hiddenSpellIds;
    std::string alwaysShownSpellIdsString;
    std::vector<uint32_t> alwaysShownSpellIds;

    std::vector<PlayerData> unresolvedPlayers;
    std::vector<PlayerData> alwaysRenderPlayersToCheck;
    std::vector<PlayerData> resolvedPlayers;
    std::string alwaysRenderPlayersString;
    uint64_t lastOfflineCheckTime = 0;

    std::vector<PlayerData> neverRenderUnresolvedPlayers;
    std::vector<PlayerData> neverRenderPlayersToCheck;
    std::vector<PlayerData> neverRenderResolvedPlayers;
    std::string neverRenderPlayersString;
    uint64_t neverRenderLastOfflineCheckTime = 0;

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
        uint32_t areaId = *reinterpret_cast<uint32_t *>(Offsets::ZoneAreaIds);

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
            if (*reinterpret_cast<uint64_t *>(0xb71368 + result * 8) == targetGUID) {
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

    uint64_t UnitGetGuid(uintptr_t *unit) {
        if (!unit) {
            return 0;
        }

        uint64_t guid = *reinterpret_cast<uint64_t *>(unit + 12);
        return guid;
    }


    uint32_t UnitGetLevel(uintptr_t *unit) {
        if (!unit) {
            return false;
        }

        auto *unitFields = *reinterpret_cast<UnitFields **>(unit + 68);

        if (unitFields == nullptr) {
            // we don't have attribute info.
            return -1;
        }

        return unitFields->level;
    }

    bool UnitIsDead(uintptr_t *unit) {
        if (!unit) {
            return false;
        }

        auto *unitFields = *reinterpret_cast<UnitFields **>(unit + 68);

        if (unitFields == nullptr) {
            // we don't have attribute info.
            return false;
        }

        auto health = unitFields->health;
        auto dynamicFlags = unitFields->dynamicFlags;

        if (health < 1 || (dynamicFlags & 0x20) != 0) {
            return true;
        } else {
            return false;
        }
    }

    bool UnitIsControlledByPlayer(uintptr_t *unit) {
        if (!unit) {
            return false;
        }

        auto *unitFields = *reinterpret_cast<UnitFields **>(unit + 68);
        if (unitFields == nullptr) {
            // we don't have attribute info.
            return false;
        }

        auto flags = unitFields->flags;

        if ((flags & UNIT_FLAG_PLAYER_CONTROLLED) != 0) {
            return true;
        } else {
            return false;
        }
    }

    bool UnitIsInCombat(uintptr_t *unit) {
        if (!unit) {
            return false;
        }

        auto *unitFields = *reinterpret_cast<UnitFields **>(unit + 68);

        if (unitFields == nullptr) {
            // we don't have attribute info.
            return false;
        }

        auto flags = unitFields->flags;
        if ((flags & UNIT_FLAG_IN_COMBAT) != 0) {
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

    void parseHiddenSpellIds(const std::string &spellIdString) {
        hiddenSpellIds.clear();
        if (spellIdString.empty()) {
            return;
        }

        std::stringstream ss(spellIdString);
        std::string item;
        while (std::getline(ss, item, ',')) {
            item.erase(item.find_last_not_of(" \t\n\r\f\v") + 1); // rtrim
            item.erase(0, item.find_first_not_of(" \t\n\r\f\v")); // ltrim
            if (!item.empty()) {
                try {
                    uint32_t spellId = std::stoul(item);
                    hiddenSpellIds.push_back(spellId);
                } catch (const std::exception &e) {
                    DEBUG_LOG("Invalid spell ID in HiddenSpellIds: " << item);
                }
            }
        }
    }

    void parseAlwaysShownSpellIds(const std::string &spellIdString) {
        alwaysShownSpellIds.clear();
        if (spellIdString.empty()) {
            return;
        }

        std::stringstream ss(spellIdString);
        std::string item;
        while (std::getline(ss, item, ',')) {
            item.erase(item.find_last_not_of(" \t\n\r\f\v") + 1); // rtrim
            item.erase(0, item.find_first_not_of(" \t\n\r\f\v")); // ltrim
            if (!item.empty()) {
                try {
                    uint32_t spellId = std::stoul(item);
                    alwaysShownSpellIds.push_back(spellId);
                } catch (const std::exception &e) {
                    DEBUG_LOG("Invalid spell ID in AlwaysShownSpellIds: " << item);
                }
            }
        }
    }

    char *UnitGetName(uintptr_t *unit) {
        if (!unit) {
            return nullptr;
        }

        auto const GetUnitName = reinterpret_cast<CGUnitGetNameT>(Offsets::CGUnitGetUnitName);
        return GetUnitName(unit, 0);
    }

    void parseAlwaysRenderPlayers(const std::string &value) {
        unresolvedPlayers.clear();
        alwaysRenderPlayersToCheck.clear();
        resolvedPlayers.clear();
        alwaysRenderPlayersString = value;
        lastOfflineCheckTime = 0;

        if (value.empty()) {
            DEBUG_LOG("AlwaysRenderPlayers list cleared");
            return;
        }

        std::stringstream ss(value);
        std::string playerName;

        while (std::getline(ss, playerName, ',')) {
            playerName.erase(0, playerName.find_first_not_of(" \t"));
            playerName.erase(playerName.find_last_not_of(" \t") + 1);

            if (!playerName.empty()) {
                unresolvedPlayers.emplace_back(playerName.c_str());
                DEBUG_LOG("Added player to unresolvedPlayers: " << playerName.c_str());
            }
        }
    }

    void parseNeverRenderPlayers(const std::string &value) {
        neverRenderUnresolvedPlayers.clear();
        neverRenderPlayersToCheck.clear();
        neverRenderResolvedPlayers.clear();
        neverRenderPlayersString = value;
        neverRenderLastOfflineCheckTime = 0;

        if (value.empty()) {
            DEBUG_LOG("NeverRenderPlayers list cleared");
            return;
        }

        std::stringstream ss(value);
        std::string playerName;

        while (std::getline(ss, playerName, ',')) {
            playerName.erase(0, playerName.find_first_not_of(" \t"));
            playerName.erase(playerName.find_last_not_of(" \t") + 1);

            if (!playerName.empty()) {
                neverRenderUnresolvedPlayers.emplace_back(playerName.c_str());
                DEBUG_LOG("Added player to neverRenderUnresolvedPlayers: " << playerName.c_str());
            }
        }
    }

    bool shouldAlwaysRenderPlayer(uintptr_t *unitPtr, uint64_t unitGuid) {
        // Check resolved players first (fastest lookup)
        for (const auto &player: resolvedPlayers) {
            if (player.guid == unitGuid) {
                return true;
            }
        }

        // Check alwaysRenderPlayersToCheck and try to resolve them
        if (!alwaysRenderPlayersToCheck.empty()) {
            char *unitName = UnitGetName(unitPtr);
            if (unitName) {
                auto it = alwaysRenderPlayersToCheck.begin();
                while (it != alwaysRenderPlayersToCheck.end()) {
                    if (strcmp(it->name, unitName) == 0) {
                        // Found match, move to resolved players
                        PlayerData resolvedPlayer = *it;
                        resolvedPlayer.guid = unitGuid;
                        resolvedPlayer.resolved = true;
                        resolvedPlayers.push_back(resolvedPlayer);

                        // Remove from alwaysRenderPlayersToCheck and unresolvedPlayers
                        alwaysRenderPlayersToCheck.erase(it);
                        auto unresolvedIt = std::find_if(unresolvedPlayers.begin(), unresolvedPlayers.end(),
                                                         [&](const PlayerData &p) {
                                                             return strcmp(p.name, unitName) == 0;
                                                         });
                        if (unresolvedIt != unresolvedPlayers.end()) {
                            unresolvedPlayers.erase(unresolvedIt);
                        }

                        DEBUG_LOG("Resolved player " << unitName << " with GUID: " << std::hex << unitGuid);
                        return true;
                    } else {
                        ++it;
                    }
                }
            }
        }

        return false;
    }

    bool shouldNeverRenderPlayer(uintptr_t *unitPtr, uint64_t unitGuid) {
        // Check resolved players first (fastest lookup)
        for (const auto &player: neverRenderResolvedPlayers) {
            if (player.guid == unitGuid) {
                return true; // Found in blacklist, never render
            }
        }

        // Check neverRenderPlayersToCheck and try to resolve them
        if (!neverRenderPlayersToCheck.empty()) {
            char *unitName = UnitGetName(unitPtr);
            if (unitName) {
                auto it = neverRenderPlayersToCheck.begin();
                while (it != neverRenderPlayersToCheck.end()) {
                    if (strcmp(it->name, unitName) == 0) {
                        // Found match, move to resolved players
                        PlayerData resolvedPlayer = *it;
                        resolvedPlayer.guid = unitGuid;
                        resolvedPlayer.resolved = true;
                        neverRenderResolvedPlayers.push_back(resolvedPlayer);

                        // Remove from neverRenderPlayersToCheck and neverRenderUnresolvedPlayers
                        neverRenderPlayersToCheck.erase(it);
                        auto unresolvedIt = std::find_if(neverRenderUnresolvedPlayers.begin(),
                                                         neverRenderUnresolvedPlayers.end(),
                                                         [&](const PlayerData &p) {
                                                             return strcmp(p.name, unitName) == 0;
                                                         });
                        if (unresolvedIt != neverRenderUnresolvedPlayers.end()) {
                            neverRenderUnresolvedPlayers.erase(unresolvedIt);
                        }

                        DEBUG_LOG(
                                "Resolved never-render player " << unitName << " with GUID: " << std::hex << unitGuid);
                        return true; // Found in blacklist, never render
                    } else {
                        ++it;
                    }
                }
            }
        }

        return false; // Not in blacklist, allow rendering
    }

    void OnWorldRenderHook(hadesmem::PatchDetourBase *detour, uintptr_t *worldFrame) {
        // store player data once before ShouldRender is called
        auto playerGuid = ClntObjMgrGetActivePlayerGuid();
        if (playerGuid != 0) {
            gPlayerUnit = nullptr;
            auto unitPtr = GetObjectPtr(ClntObjMgrGetActivePlayerGuid());
            if (unitPtr != nullptr) {
                // check that descriptor fields are loaded
                auto *unitFields = *reinterpret_cast<UnitFields **>(unitPtr + 68);
                if (unitFields != nullptr && unitFields->level > 0) {
                    gPlayerUnit = unitPtr;
                }
            }
            if (gPlayerUnit) {
                gPlayerPosition = UnitGetPosition(gPlayerUnit);
                gPlayerInCombat = UnitIsInCombat(gPlayerUnit);
                gPlayerInCity = IsPlayerInCity();
            }
        }

        uint64_t currentTime = GetWowTimeMs();

        // Every 60 seconds: move unresolved players to alwaysRenderPlayersToCheck if there are any
        if (gPlayerUnit && !unresolvedPlayers.empty() && (currentTime - lastOfflineCheckTime) > 60000) {
            alwaysRenderPlayersToCheck.insert(alwaysRenderPlayersToCheck.end(), unresolvedPlayers.begin(),
                                              unresolvedPlayers.end());
            DEBUG_LOG("Moved " << unresolvedPlayers.size() << " unresolved players to alwaysRenderPlayersToCheck");
            unresolvedPlayers.clear();
            lastOfflineCheckTime = currentTime;
        }

        // Every 60 seconds: move neverRender unresolved players to neverRenderPlayersToCheck if there are any
        if (gPlayerUnit && !neverRenderUnresolvedPlayers.empty() &&
            (currentTime - neverRenderLastOfflineCheckTime) > 60000) {
            neverRenderPlayersToCheck.insert(neverRenderPlayersToCheck.end(), neverRenderUnresolvedPlayers.begin(),
                                             neverRenderUnresolvedPlayers.end());
            DEBUG_LOG("Moved " << neverRenderUnresolvedPlayers.size()
                               << " neverRender unresolved players to neverRenderPlayersToCheck");
            neverRenderUnresolvedPlayers.clear();
            neverRenderLastOfflineCheckTime = currentTime;
        }

        auto const OnWorldRender = detour->GetTrampolineT<FastcallFrameT>();
        OnWorldRender(worldFrame);

        if (!alwaysRenderPlayersToCheck.empty()) {
            // move any remaining players from alwaysRenderPlayersToCheck back to unresolvedPlayers
            unresolvedPlayers.insert(unresolvedPlayers.end(), alwaysRenderPlayersToCheck.begin(),
                                     alwaysRenderPlayersToCheck.end());
            DEBUG_LOG("Moved " << alwaysRenderPlayersToCheck.size() << " players back to unresolvedPlayers");
            alwaysRenderPlayersToCheck.clear();
        }

        if (!neverRenderPlayersToCheck.empty()) {
            // move any remaining neverRender players from neverRenderPlayersToCheck back to neverRenderUnresolvedPlayers
            neverRenderUnresolvedPlayers.insert(neverRenderUnresolvedPlayers.end(), neverRenderPlayersToCheck.begin(),
                                                neverRenderPlayersToCheck.end());
            DEBUG_LOG("Moved " << neverRenderPlayersToCheck.size()
                               << " neverRender players back to neverRenderUnresolvedPlayers");
            neverRenderPlayersToCheck.clear();
        }
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


    uint32_t shouldRenderPlayer(uintptr_t *unitPtr) {
        if (hideAllPlayers) {
            return 0; // Hide all players
        }
        auto unitGuid = UnitGetGuid(unitPtr);
        if (alwaysRenderRaidMarks) {
            auto raidMark = GetRaidMarkForGuid(unitGuid);

            if (raidMark > 0) {
                // always render players with raid marks
                return 1;
            }
        }

        // check if this player is in NeverRenderPlayers blacklist
        if (shouldNeverRenderPlayer(unitPtr, unitGuid)) {
            return 0; // Force hide this player
        }

        // always show pvp/mc players
        if (UnitCanAttackUnit(gPlayerUnit, unitPtr)) {
            return 1;
        }

        // check if this player is in AlwaysRenderPlayers list
        if (shouldAlwaysRenderPlayer(unitPtr, unitGuid)) {
            return 1;
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
    }

    uint32_t shouldRenderUnit(uintptr_t *unitPtr) {
        if (alwaysRenderRaidMarks) {
            auto raidMark = GetRaidMarkForGuid(UnitGetGuid(unitPtr));

            if (raidMark > 0) {
                // always render raid marks
                return 1;
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
                auto *unitFields = *reinterpret_cast<UnitFields **>(unitPtr + 68);

                // always show your own summons
                if (unitFields->summonedBy != ClntObjMgrGetActivePlayerGuid()) {
                    if (unitFields->petNameTimestamp > 0) {
                        // This is a pet with a name
                        int renderDist = (gPlayerInCombat && petRenderDistInCombat != -1)
                                         ? petRenderDistInCombat
                                         : petRenderDist;
                        return ShouldRenderBasedOnDistance(unitPtr, renderDist);
                    } else {
                        // This is a summon (player-controlled unit without name)
                        if (summonRenderDist != -1) {
                            int renderDist = (gPlayerInCombat && summonRenderDistInCombat != -1)
                                             ? summonRenderDistInCombat
                                             : summonRenderDist;
                            return ShouldRenderBasedOnDistance(unitPtr, renderDist);
                        }
                    }
                }
            }

            auto unitLevel = UnitGetLevel(unitPtr);
            if (unitLevel < 63) {
                int renderDist = (gPlayerInCombat && trashUnitRenderDistInCombat != -1)
                                 ? trashUnitRenderDistInCombat : trashUnitRenderDist;
                return ShouldRenderBasedOnDistance(unitPtr, renderDist);
            }
        }

        return 1; // Default to rendering the unit
    }

    uint32_t shouldRenderCorpse(uintptr_t *unitPtr) {
        int renderDist = corpseRenderDist;
        return ShouldRenderBasedOnDistance(unitPtr, renderDist);
    }

    const SpellRec *GetSpellInfo(uint32_t spellId) {
        auto const spellDb = reinterpret_cast<WowClientDB<SpellRec> *>(Offsets::SpellDb);

        if (spellId > spellDb->m_maxId)
            return nullptr;

        return spellDb->m_recordsById[spellId];
    }

    const char *GetSpellName(uint32_t spellId) {
        auto const spell = GetSpellInfo(spellId);

        if (!spell || spell->AttributesEx3 & SPELL_ATTR_EX3_NO_CASTING_BAR_TEXT)
            return "";

        auto const language = *reinterpret_cast<std::uint32_t *>(Offsets::Language);

        return spell->SpellName[language];
    }

    bool shouldHideSpellForUnit(uintptr_t *unitPtr, const SpellRec *spellRec) {
        if (!spellRec || !unitPtr) {
            return false;
        }

        if (unitPtr != gPlayerUnit && pbEnabled) {
            // Check if this spell ID should always be shown
            if (!alwaysShownSpellIds.empty()) {
                auto it = std::find(alwaysShownSpellIds.begin(), alwaysShownSpellIds.end(), spellRec->Id);
                if (it != alwaysShownSpellIds.end()) {
                    return false; // Always show this spell
                }
            }

            auto unitType = UnitGetType(unitPtr);
            if (unitType == OBJECT_TYPE_PLAYER) {
                // Check if we should show player spells
                if (!showPlayerSpellVisuals) {
                    // hide visuals for players other than the player
                    return true;
                }

                // Check if we should hide spells for hidden players
                if (hideSpellsForHiddenPlayers && shouldRenderPlayer(unitPtr) == 0) {
                    // hide spells for players that would be hidden
                    return true;
                }
            }

            // Check if this spell ID should be hidden
            if (!hiddenSpellIds.empty()) {
                auto it = std::find(hiddenSpellIds.begin(), hiddenSpellIds.end(), spellRec->Id);
                if (it != hiddenSpellIds.end()) {
                    return true;
                }
            }
        }

        return false; // Do not hide this spell
    }

    bool shouldHideGroundEffectForUnit(uintptr_t *unitPtr, const SpellRec *spellRec) {
        if (!spellRec || !unitPtr) {
            return false;
        }

        if (unitPtr != gPlayerUnit && pbEnabled) {
            // Check if this spell ID should always be shown
            if (!alwaysShownSpellIds.empty()) {
                auto it = std::find(alwaysShownSpellIds.begin(), alwaysShownSpellIds.end(), spellRec->Id);
                if (it != alwaysShownSpellIds.end()) {
                    return false; // Always show this spell
                }
            }

            auto unitType = UnitGetType(unitPtr);
            if (unitType == OBJECT_TYPE_PLAYER) {
                // Check if we should show player ground effects
                if (!showPlayerGroundEffects) {
                    // hide ground effects for players other than the player
                    return true;
                }

                // Check if we should hide spells for hidden players
                if (hideSpellsForHiddenPlayers && shouldRenderPlayer(unitPtr) == 0) {
                    // hide spells for players that would be hidden
                    return true;
                }
            }

            // Check if this spell ID should be hidden
            if (!hiddenSpellIds.empty()) {
                auto it = std::find(hiddenSpellIds.begin(), hiddenSpellIds.end(), spellRec->Id);
                if (it != hiddenSpellIds.end()) {
                    return true;
                }
            }
        }

        return false; // Do not hide this spell
    }

    bool shouldHideAuraEffectForUnit(uintptr_t *unitPtr, const SpellRec *spellRec) {
        if (!spellRec || !unitPtr) {
            return false;
        }

        if (unitPtr != gPlayerUnit && pbEnabled) {
            // Check if this spell ID should always be shown
            if (!alwaysShownSpellIds.empty()) {
                auto it = std::find(alwaysShownSpellIds.begin(), alwaysShownSpellIds.end(), spellRec->Id);
                if (it != alwaysShownSpellIds.end()) {
                    return false; // Always show this spell
                }
            }

            auto unitType = UnitGetType(unitPtr);
            if (unitType == OBJECT_TYPE_PLAYER) {
                // Check if we should show player aura visuals
                if (!showPlayerAuraVisuals) {
                    return true;
                }

                // Check if we should hide spells for hidden players
                if (hideSpellsForHiddenPlayers && shouldRenderPlayer(unitPtr) == 0) {
                    // hide spells for players that would be hidden
                    return true;
                }
            } else {
                // Check if we should show unit aura visuals
                if (!showUnitAuraVisuals) {
                    return true;
                }
            }

            // Check if this spell ID should be hidden
            if (!hiddenSpellIds.empty()) {
                auto it = std::find(hiddenSpellIds.begin(), hiddenSpellIds.end(), spellRec->Id);
                if (it != hiddenSpellIds.end()) {
                    return true;
                }
            }
        }

        return false; // Do not hide this spell
    }

    void
    CGUnitPlaySpellVisualHook(hadesmem::PatchDetourBase *detour, uintptr_t *unitPtr, void *dummy_edx,
                              SpellRec *spellRec,
                              uintptr_t *visualKit, void *param_3, void *param_4) {
        // get aura visual return address 0X005FF4CB
        if (reinterpret_cast<int>(detour->GetReturnAddressPtr()) == 0X005FF4CB) {
            if (shouldHideAuraEffectForUnit(unitPtr, spellRec)) {
                return;
            }
        }

        auto const CGUnitPlaySpellVisual = detour->GetTrampolineT<CGUnitPlaySpellVisualT>();
        CGUnitPlaySpellVisual(unitPtr, dummy_edx, spellRec, visualKit, param_3, param_4);
    }

    void
    CGUnitPlayChannelVisualHook(hadesmem::PatchDetourBase *detour, uintptr_t *unitPtr, void *dummy_edx) {
        auto *unitFields = *reinterpret_cast<UnitFields **>(unitPtr + 68);

        auto channelSpellId = unitFields->channelSpell;
        if (channelSpellId > 0) {
            auto spellRec = GetSpellInfo(channelSpellId);
            if (shouldHideSpellForUnit(unitPtr, spellRec)) {
                return; // Hide channel visual if the spell is hidden
            }
        }

        auto const CGUnitPlayChannelVisual = detour->GetTrampolineT<CGUnitPlayChannelVisualT>();
        CGUnitPlayChannelVisual(unitPtr, dummy_edx);
    }

    uintptr_t *
    CGUnitGetAppropriateSpellVisualHook(hadesmem::PatchDetourBase *detour, uintptr_t *unitPtr, void *dummy_edx,
                                        SpellRec *spellRec, uintptr_t *visualKit) {
        // Don't mess with visuals in GetMissileTargetLocation as it expects it never to be null
        // GetMissileTargetLocation return address 0x006EC80C
        if (reinterpret_cast<int>(detour->GetReturnAddressPtr()) != 0x006EC80C) {
            // spell effect 27 is PERSISTENT_AREA_AURA used by ground effects
            if ((spellRec->Effect[0] == 27 || spellRec->Effect[1] == 27 || spellRec->Effect[2] == 27) &&
                shouldHideGroundEffectForUnit(unitPtr, spellRec)) {
                return nullptr; // Return null to hide the visual {
            } else if (shouldHideSpellForUnit(unitPtr, spellRec)) {
                return nullptr; // Return null to hide the visual
            }
        }

        auto const CGUnitGetAppropriateSpellVisual = detour->GetTrampolineT<CGUnitGetAppropriateSpellVisualT>();
        return CGUnitGetAppropriateSpellVisual(unitPtr, dummy_edx, spellRec, visualKit);
    }

    uintptr_t *GetSpellVisualHook(hadesmem::PatchDetourBase *detour, SpellRec *param_1) {
        DEBUG_LOG(GetSpellName(param_1->Id) << " " << detour->GetReturnAddressPtr());
        auto const GetSpellVisual = detour->GetTrampolineT<GetSpellVisualT>();
        return GetSpellVisual(param_1);
    }

    SpellVisualEffectNameRec *
    CGDynamicObjectGetVisualEffectNameRecHook(hadesmem::PatchDetourBase *detour, uintptr_t *dynamicObjPtr,
                                              void *dummy_edx) {
        auto *dynamicObjectFields = *reinterpret_cast<DynamicObjectFields **>(dynamicObjPtr + 68);

        if (dynamicObjectFields != nullptr) {
            auto unitPtr = ClntObjMgrObjectPtr(TYPE_MASK_UNIT, dynamicObjectFields->m_caster);
            if (shouldHideGroundEffectForUnit(unitPtr, GetSpellInfo(dynamicObjectFields->m_spellID))) {
                return nullptr;
            }
        }

        auto const CGDynamicObjectGetVisualEffectNameRec = detour->GetTrampolineT<CGDynamicObjectGetVisualEffectNameRecT>();
        return CGDynamicObjectGetVisualEffectNameRec(dynamicObjPtr, dummy_edx);
    }

    void ObjectVisKitProcHook(hadesmem::PatchDetourBase *detour, uintptr_t *dynamicObjPtr) {
        auto *dynamicObjectFields = *reinterpret_cast<DynamicObjectFields **>(dynamicObjPtr + 68);

        if (dynamicObjectFields != nullptr) {
            auto unitPtr = ClntObjMgrObjectPtr(TYPE_MASK_UNIT, dynamicObjectFields->m_caster);
            if (shouldHideGroundEffectForUnit(unitPtr, GetSpellInfo(dynamicObjectFields->m_spellID))) {
                return;
            }
        }

        auto const ObjectVisKitProc = detour->GetTrampolineT<ObjectVisKitProcT>();
        ObjectVisKitProc(dynamicObjPtr);
    }

    void SendUnitSignalHook(hadesmem::PatchDetourBase *detour, uint64_t *guid, uint32_t eventCode) {
        if (guid && *guid != 0) {
            auto const GetNamesFromGUID = reinterpret_cast<GetNamesFromGUIDT>(Offsets::GetNamesFromGUID);
            auto const SignalEventParam = reinterpret_cast<SignalEventParamSingleStringT>(Offsets::SignalEventParam);

            int numNames = 0;
            char **names = GetNamesFromGUID(guid, &numNames);

            if (names && numNames > 0) {
                char format[] = "%s";

                for (int i = 0; i < numNames; i++) {
                    if (names[i]) {
                        // check if names starts with 0x (raw guids from super wow)
                        // don't trigger with guid for any event codes below 182 (UNIT_COMBAT)
                        // don't trigger for 183(UNIT_NAME_UPDATE), 184(UNIT_PORTRAIT_UPDATE), 186(UNIT_INVENTORY_CHANGED), 345(PLAYER_GUILD_UPDATE)
                        // this turns off UNIT_AURA, UNIT_HEALTH, UNIT_MANA spam for guids
                        if (filterGuidEvents && strncmp(names[i], "0x", 2) == 0 &&
                            (eventCode < 182 || eventCode == 183 || eventCode == 184 || eventCode == 186 ||
                             eventCode == 345)) {
                            continue;
                        }
                        SignalEventParam(eventCode, format, names[i]);
                    }
                }
            }
        }
    }

    void CGUnitPreAnimateHook(hadesmem::PatchDetourBase *detour, uintptr_t *unitPtr, void *dummy_edx, void *param_1) {
        auto const CGUnitPreAnimate = detour->GetTrampolineT<CGUnitPreAnimateT>();
        CGUnitPreAnimate(unitPtr, dummy_edx, param_1);
    }

    void CGUnitAnimateHook(hadesmem::PatchDetourBase *detour, uintptr_t *unitPtr, void *dummy_edx, int *param_3) {
        auto const CGUnitAnimate = detour->GetTrampolineT<CGUnitAnimateT>();
        CGUnitAnimate(unitPtr, dummy_edx, param_3);
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
                    return shouldRenderPlayer(unitPtr);
                } else if (unitType == OBJECT_TYPE_UNIT) {
                    return shouldRenderUnit(unitPtr);
                } else if (unitType == OBJECT_TYPE_CORPSE && corpseRenderDist != -1) {
                    return shouldRenderCorpse(unitPtr);
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
        } else if (strcmp(cvar, "PB_SummonRenderDist") == 0) {
            summonRenderDist = atoi(value);
            DEBUG_LOG("Set PB_SummonRenderDist to " << summonRenderDist);
        } else if (strcmp(cvar, "PB_SummonRenderDistInCombat") == 0) {
            summonRenderDistInCombat = atoi(value);
            DEBUG_LOG("Set PB_SummonRenderDistInCombat to " << summonRenderDistInCombat);
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
        } else if (strcmp(cvar, "PB_HideAllPlayers") == 0) {
            hideAllPlayers = atoi(value) != 0;
            DEBUG_LOG("Set PB_HideAllPlayers to " << hideAllPlayers);
        } else if (strcmp(cvar, "PB_FilterGuidEvents") == 0) {
            filterGuidEvents = atoi(value) != 0;
            DEBUG_LOG("Set PB_FilterGuidEvents to " << filterGuidEvents);
        } else if (strcmp(cvar, "PB_AlwaysRenderPlayers") == 0) {
            parseAlwaysRenderPlayers(value);
        } else if (strcmp(cvar, "PB_NeverRenderPlayers") == 0) {
            parseNeverRenderPlayers(value);
        } else if (strcmp(cvar, "PB_ShowPlayerSpellVisuals") == 0) {
            showPlayerSpellVisuals = atoi(value) != 0;
            DEBUG_LOG("Set PB_ShowPlayerSpellVisuals to " << showPlayerSpellVisuals);
        } else if (strcmp(cvar, "PB_ShowPlayerGroundEffects") == 0) {
            showPlayerGroundEffects = atoi(value) != 0;
            DEBUG_LOG("Set PB_ShowPlayerGroundEffects to " << showPlayerGroundEffects);
        } else if (strcmp(cvar, "PB_ShowPlayerAuraVisuals") == 0) {
            showPlayerAuraVisuals = atoi(value) != 0;
            DEBUG_LOG("Set PB_ShowPlayerAuraVisuals to " << showPlayerAuraVisuals);
        } else if (strcmp(cvar, "PB_ShowUnitAuraVisuals") == 0) {
            showUnitAuraVisuals = atoi(value) != 0;
            DEBUG_LOG("Set PB_ShowUnitAuraVisuals to " << showUnitAuraVisuals);
        } else if (strcmp(cvar, "PB_HideSpellsForHiddenPlayers") == 0) {
            hideSpellsForHiddenPlayers = atoi(value) != 0;
            DEBUG_LOG("Set PB_HideSpellsForHiddenPlayers to " << hideSpellsForHiddenPlayers);
        } else if (strcmp(cvar, "PB_HiddenSpellIds") == 0) {
            hiddenSpellIdsString = value;
            parseHiddenSpellIds(hiddenSpellIdsString);
            DEBUG_LOG("Set PB_HiddenSpellIds to " << hiddenSpellIdsString << " (parsed " << hiddenSpellIds.size()
                                                  << " spell IDs)");
        } else if (strcmp(cvar, "PB_AlwaysShownSpellIds") == 0) {
            alwaysShownSpellIdsString = value;
            parseAlwaysShownSpellIds(alwaysShownSpellIdsString);
            DEBUG_LOG("Set PB_AlwaysShownSpellIds to " << alwaysShownSpellIdsString << " (parsed "
                                                       << alwaysShownSpellIds.size()
                                                       << " spell IDs)");
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

    char *getCvarString(const char *cvar) {
        auto const cvarLookup = hadesmem::detail::AliasCast<CVarLookupT>(Offsets::CVarLookup);
        uintptr_t *cvarPtr = cvarLookup(cvar);

        if (cvarPtr) {
            // Get strValue from CVar
            char **strValuePtr = reinterpret_cast<char **>(cvarPtr + 8);
            return *strValuePtr;
        }
        return nullptr;
    }

    void loadUserVar(const char *cvar) {
        // Handle string cvars specially
        if (strcmp(cvar, "PB_AlwaysRenderPlayers") == 0 || strcmp(cvar, "PB_NeverRenderPlayers") == 0 ||
            strcmp(cvar, "PB_HiddenSpellIds") == 0) {
            char *stringValue = getCvarString(cvar);
            if (stringValue) {
                updateFromCvar(cvar, stringValue);
            } else {
                DEBUG_LOG("Using default empty string for " << cvar);
                updateFromCvar(cvar, "");
            }
        } else {
            // Handle integer cvars as before
            int *value = getCvar(cvar);
            if (value) {
                updateFromCvar(cvar, std::to_string(*value).c_str());
            } else {
                DEBUG_LOG("Using default value for " << cvar);
            }
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
//        initializeHook<CGUnitPreAnimateT>(process, Offsets::CGUnitPreAnimate, &CGUnitPreAnimateHook);
//        initializeHook<CGUnitAnimateT>(process, Offsets::CGUnitAnimate, &CGUnitAnimateHook);
        initializeHook<CGUnitShouldRenderT>(process, Offsets::CGUnitShouldRender, &CGUnitShouldRenderHook);

        // Hook CGUnitPlaySpellVisual
        initializeHook<CGUnitPlaySpellVisualT>(process, Offsets::CGUnitPlaySpellVisual, &CGUnitPlaySpellVisualHook);

        // Hook CGUnitPlayChannelVisual
        initializeHook<CGUnitPlayChannelVisualT>(process, Offsets::CGUnitPlayChannelVisual,
                                                 &CGUnitPlayChannelVisualHook);

        // Hook CGUnitGetAppropriateSpellVisual
        initializeHook<CGUnitGetAppropriateSpellVisualT>(process, Offsets::CGUnitGetAppropriateSpellVisual,
                                                         &CGUnitGetAppropriateSpellVisualHook);

        // Hook CGDynamicObjectGetVisualEffectNameRec
        initializeHook<CGDynamicObjectGetVisualEffectNameRecT>(process, Offsets::CGDynamicObjectGetVisualEffectNameRec,
                                                               &CGDynamicObjectGetVisualEffectNameRecHook);

        // Hook GetSpellVisual
//        initializeHook<GetSpellVisualT>(process, Offsets::GetSpellVisual, &GetSpellVisualHook);

        // Hook ObjectVisKitProc
        initializeHook<ObjectVisKitProcT>(process, Offsets::ObjectVisKitProc, &ObjectVisKitProcHook);

        // Hook SendUnitSignal
        initializeHook<SendUnitSignalT>(process, Offsets::SendUnitSignal, &SendUnitSignalHook);
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
        char defaultDisabled[] = "0";

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

        // Max distance to render summons when not in combat
        char PB_SummonRenderDist[] = "PB_SummonRenderDist";
        CVarRegister(PB_SummonRenderDist, // name
                     nullptr, // help
                     0,  // unk1
                     defaultUnset, // default value address
                     nullptr, // callback
                     5, // category
                     0,  // unk2
                     0); // unk3

        // Max distance to render summons when in combat
        char PB_SummonRenderDistInCombat[] = "PB_SummonRenderDistInCombat";
        CVarRegister(PB_SummonRenderDistInCombat, // name
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

        // Whether to hide all players
        char PB_HideAllPlayers[] = "PB_HideAllPlayers";
        CVarRegister(PB_HideAllPlayers, // name
                     nullptr, // help
                     0,  // unk1
                     defaultDisabled, // default value address
                     nullptr, // callback
                     5, // category
                     0,  // unk2
                     0); // unk3

        // Whether to filter GUID events
        char PB_FilterGuidEvents[] = "PB_FilterGuidEvents";
        CVarRegister(PB_FilterGuidEvents, // name
                     nullptr, // help
                     0,  // unk1
                     defaultEnabled, // default value address
                     nullptr, // callback
                     5, // category
                     0,  // unk2
                     0); // unk3

        // Comma separated list of player names to always render
        char defaultEmpty[] = "";
        char PB_AlwaysRenderPlayers[] = "PB_AlwaysRenderPlayers";
        CVarRegister(PB_AlwaysRenderPlayers, // name
                     nullptr, // help
                     0,  // unk1
                     defaultEmpty, // default value address
                     nullptr, // callback
                     5, // category
                     0,  // unk2
                     0); // unk3

        // Comma separated list of player names to never render (blacklist)
        char PB_NeverRenderPlayers[] = "PB_NeverRenderPlayers";
        CVarRegister(PB_NeverRenderPlayers, // name
                     nullptr, // help
                     0,  // unk1
                     defaultEmpty, // default value address
                     nullptr, // callback
                     5, // category
                     0,  // unk2
                     0); // unk3

        // Show player spell visuals
        char PB_ShowPlayerSpellVisuals[] = "PB_ShowPlayerSpellVisuals";
        CVarRegister(PB_ShowPlayerSpellVisuals, // name
                     nullptr, // help
                     0,  // unk1
                     defaultEnabled, // default value address
                     nullptr, // callback
                     5, // category
                     0,  // unk2
                     0); // unk3

        // Show player ground effects
        char PB_ShowPlayerGroundEffects[] = "PB_ShowPlayerGroundEffects";
        CVarRegister(PB_ShowPlayerGroundEffects, // name
                     nullptr, // help
                     0,  // unk1
                     defaultEnabled, // default value address
                     nullptr, // callback
                     5, // category
                     0,  // unk2
                     0); // unk3

        // Show player aura visuals
        char PB_ShowPlayerAuraVisuals[] = "PB_ShowPlayerAuraVisuals";
        CVarRegister(PB_ShowPlayerAuraVisuals, // name
                     nullptr, // help
                     0,  // unk1
                     defaultEnabled, // default value address
                     nullptr, // callback
                     5, // category
                     0,  // unk2
                     0); // unk3

        // Show unit aura visuals
        char PB_ShowUnitAuraVisuals[] = "PB_ShowUnitAuraVisuals";
        CVarRegister(PB_ShowUnitAuraVisuals, // name
                     nullptr, // help
                     0,  // unk1
                     defaultEnabled, // default value address
                     nullptr, // callback
                     5, // category
                     0,  // unk2
                     0); // unk3

        // Hide spells for hidden players
        char PB_HideSpellsForHiddenPlayers[] = "PB_HideSpellsForHiddenPlayers";
        CVarRegister(PB_HideSpellsForHiddenPlayers, // name
                     nullptr, // help
                     0,  // unk1
                     defaultEnabled, // default value address
                     nullptr, // callback
                     5, // category
                     0,  // unk2
                     0); // unk3

        // Comma separated list of spell IDs to hide visuals for
        char PB_HiddenSpellIds[] = "PB_HiddenSpellIds";
        CVarRegister(PB_HiddenSpellIds, // name
                     nullptr, // help
                     0,  // unk1
                     defaultEmpty, // default value address
                     nullptr, // callback
                     5, // category
                     0,  // unk2
                     0); // unk3

        // Comma separated list of spell IDs to always show visuals for
        char PB_AlwaysShownSpellIds[] = "PB_AlwaysShownSpellIds";
        CVarRegister(PB_AlwaysShownSpellIds, // name
                     nullptr, // help
                     0,  // unk1
                     defaultEmpty, // default value address
                     nullptr, // callback
                     5, // category
                     0,  // unk2
                     0); // unk3


        loadUserVar("PB_PlayerRenderDist");
        loadUserVar("PB_PlayerRenderDistInCities");
        loadUserVar("PB_PlayerRenderDistInCombat");
        loadUserVar("PB_PetRenderDist");
        loadUserVar("PB_PetRenderDistInCombat");
        loadUserVar("PB_SummonRenderDist");
        loadUserVar("PB_SummonRenderDistInCombat");
        loadUserVar("PB_TrashUnitRenderDist");
        loadUserVar("PB_TrashUnitRenderDistInCombat");
        loadUserVar("PB_CorpseRenderDist");
        loadUserVar("PB_Enabled");
        loadUserVar("PB_AlwaysRenderRaidMarks");
        loadUserVar("PB_HideAllPlayers");
        loadUserVar("PB_FilterGuidEvents");
        loadUserVar("PB_AlwaysRenderPlayers");
        loadUserVar("PB_NeverRenderPlayers");
        loadUserVar("PB_ShowPlayerSpellVisuals");
        loadUserVar("PB_ShowPlayerGroundEffects");
        loadUserVar("PB_ShowPlayerAuraVisuals");
        loadUserVar("PB_ShowUnitAuraVisuals");
        loadUserVar("PB_HideSpellsForHiddenPlayers");
        loadUserVar("PB_HiddenSpellIds");
        loadUserVar("PB_AlwaysShownSpellIds");
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
