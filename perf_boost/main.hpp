//
// Created by pmacc on 9/21/2024.
//

#pragma once

#include <hadesmem/process.hpp>
#include <hadesmem/patcher.hpp>

#include <Windows.h>

#include <cstdint>
#include <memory>
#include <atomic>
#include <cstdarg>

#include <chrono>
#include <thread>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

namespace perf_boost {
    typedef struct C3Vector {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    } C3Vector;

    typedef enum OBJECT_TYPE_MASK {
        TYPE_MASK_OBJECT=1,
        TYPE_MASK_ITEM=2,
        TYPE_MASK_CONTAINER=4,
        TYPE_MASK_UNIT=8,
        TYPE_MASK_PLAYER=16,
        TYPE_MASK_GAMEOBJECT=32,
        TYPE_MASK_DYNAMICOBJECT=64,
        TYPE_MASK_CORPSE=128,
        TYPE_MASK_AIGROUP=256,
        TYPE_MASK_AREATRIGGER=512
    } OBJECT_TYPE_MASK;

     enum OBJECT_TYPE_ID
     {
       OBJECT_TYPE_OBJECT = 0x0,
       OBJECT_TYPE_ITEM = 0x1,
       OBJECT_TYPE_CONTAINER = 0x2,
       OBJECT_TYPE_UNIT = 0x3,
       OBJECT_TYPE_PLAYER = 0x4,
       OBJECT_TYPE_GAMEOBJECT = 0x5,
       OBJECT_TYPE_DYNAMICOBJECT = 0x6,
       OBJECT_TYPE_CORPSE = 0x7,
       NUM_CLIENT_OBJECT_TYPES = 0x8,
       OBJECT_TYPE_AIGROUP = 0x8,
       OBJECT_TYPE_AREATRIGGER = 0x9,
       NUM_OBJECT_TYPES = 0xA,
     };

    enum UnitFlags
    {
        UNIT_FLAG_NONE                  = 0x00000000,
        UNIT_FLAG_UNK_0                 = 0x00000001,           // Movement checks disabled, likely paired with loss of client control packet.
        UNIT_FLAG_SPAWNING              = 0x00000002,           // not attackable
        UNIT_FLAG_DISABLE_MOVE          = 0x00000004,
        UNIT_FLAG_PLAYER_CONTROLLED     = 0x00000008,           // players, pets, totems, guardians, companions, charms, any units associated with players
        UNIT_FLAG_PET_RENAME            = 0x00000010,           // Old pet rename: moved to UNIT_FIELD_BYTES_2,2 in TBC+
        UNIT_FLAG_PET_ABANDON           = 0x00000020,           // Old pet abandon: moved to UNIT_FIELD_BYTES_2,2 in TBC+
        UNIT_FLAG_UNK_6                 = 0x00000040,
        UNIT_FLAG_IMMUNE_TO_PLAYER      = 0x00000100,           // Target is immune to players
        UNIT_FLAG_IMMUNE_TO_NPC         = 0x00000200,           // Target is immune to creatures
        UNIT_FLAG_PVP                   = 0x00001000,
        UNIT_FLAG_SILENCED              = 0x00002000,           // silenced, 2.1.1
        UNIT_FLAG_UNK_14                = 0x00004000,
        UNIT_FLAG_USE_SWIM_ANIMATION    = 0x00008000,
        UNIT_FLAG_NON_ATTACKABLE_2      = 0x00010000,           // removes attackable icon, if on yourself, cannot assist self but can cast TARGET_UNIT_CASTER spells - added by SPELL_AURA_MOD_UNATTACKABLE
        UNIT_FLAG_PACIFIED              = 0x00020000,
        UNIT_FLAG_STUNNED               = 0x00040000,           // Unit is a subject to stun, turn and strafe movement disabled
        UNIT_FLAG_IN_COMBAT             = 0x00080000,
        UNIT_FLAG_TAXI_FLIGHT           = 0x00100000,           // Unit is on taxi, paired with a duplicate loss of client control packet (likely a legacy serverside hack). Disables any spellcasts not allowed in taxi flight client-side.
        UNIT_FLAG_CONFUSED              = 0x00400000,           // Unit is a subject to confused movement, movement checks disabled, paired with loss of client control packet.
        UNIT_FLAG_FLEEING               = 0x00800000,           // Unit is a subject to fleeing movement, movement checks disabled, paired with loss of client control packet.
        UNIT_FLAG_POSSESSED             = 0x01000000,           // Unit is under remote control by another unit, movement checks disabled, paired with loss of client control packet. New master is allowed to use melee attack and can't select this unit via mouse in the world (as if it was own character).
        UNIT_FLAG_NOT_SELECTABLE        = 0x02000000,
        UNIT_FLAG_SKINNABLE             = 0x04000000,
        UNIT_FLAG_AURAS_VISIBLE         = 0x08000000,           // magic detect
        UNIT_FLAG_SHEATHE               = 0x40000000,
        UNIT_FLAG_IMMUNE                = 0x80000000,           // Immune to damage

        // [-ZERO] TBC enumerations [?]
        UNIT_FLAG_NOT_ATTACKABLE_1      = 0x00000080,           // ?? (UNIT_FLAG_PLAYER_CONTROLLED | UNIT_FLAG_NOT_ATTACKABLE_1) is NON_PVP_ATTACKABLE
        UNIT_FLAG_LOOTING               = 0x00000400,           // loot animation
        UNIT_FLAG_PET_IN_COMBAT         = 0x00000800,           // in combat?, 2.0.8
        UNIT_FLAG_DISARMED              = 0x00200000,           // disable melee spells casting..., "Required melee weapon" added to melee spells tooltip.

        UNIT_FLAG_UNK_28                = 0x10000000,
        UNIT_FLAG_UNK_29                = 0x20000000,           // used in Feing Death spell
    };

    typedef struct UnitFields {
        uint64_t charm;                          // Size:2
        uint64_t summon;                         // Size:2
        uint64_t charmedBy;                      // Size:2
        uint64_t summonedBy;                     // Size:2
        uint64_t createdBy;                      // Size:2
        uint64_t target;                         // Size:2
        uint64_t persuaded;                      // Size:2
        uint64_t channelObject;                  // Size:2
        uint32_t health;                         // Size:1
        uint32_t power1;                         // Size:1
        uint32_t power2;                         // Size:1
        uint32_t power3;                         // Size:1
        uint32_t power4;                         // Size:1
        uint32_t power5;                         // Size:1
        uint32_t maxHealth;                      // Size:1
        uint32_t maxPower1;                      // Size:1
        uint32_t maxPower2;                      // Size:1
        uint32_t maxPower3;                      // Size:1
        uint32_t maxPower4;                      // Size:1
        uint32_t maxPower5;                      // Size:1
        uint32_t level;                          // Size:1
        uint32_t factionTemplate;                // Size:1
        uint32_t bytes0;                         // Size:1
        uint32_t virtualItemDisplay[3];          // Size:3
        uint32_t virtualItemInfo[6];             // Size:6
        uint32_t flags;                          // Size:1
        uint32_t aura[48];                       // Size:48
        uint32_t auraFlags[6];                   // Size:6
        uint32_t auraLevels[12];                 // Size:12
        uint32_t auraApplications[12];           // Size:12
        uint32_t auraState;                      // Size:1
        uint32_t baseAttackTime;                 // Size:1
        uint32_t offhandAttackTime;              // Size:1
        uint32_t rangedAttackTime;               // Size:1
        float boundingRadius;                    // Size:1
        float combatReach;                       // Size:1
        uint32_t displayId;                      // Size:1
        uint32_t nativeDisplayId;                // Size:1
        uint32_t mountDisplayId;                 // Size:1
        float minDamage;                         // Size:1
        float maxDamage;                         // Size:1
        float minOffhandDamage;                  // Size:1
        float maxOffhandDamage;                  // Size:1
        uint32_t bytes1;                         // Size:1
        uint32_t petNumber;                      // Size:1
        uint32_t petNameTimestamp;               // Size:1
        uint32_t petExperience;                  // Size:1
        uint32_t petNextLevelExp;                // Size:1
        uint32_t dynamicFlags;                   // Size:1
        uint32_t channelSpell;                   // Size:1
        float modCastSpeed;                      // Size:1 (Float in 1.12+)
        uint32_t createdBySpell;                 // Size:1
        uint32_t npcFlags;                       // Size:1
        uint32_t npcEmoteState;                  // Size:1
        uint32_t trainingPoints;                 // Size:1
        uint32_t stat0;                          // Size:1
        uint32_t stat1;                          // Size:1
        uint32_t stat2;                          // Size:1
        uint32_t stat3;                          // Size:1
        uint32_t stat4;                          // Size:1
        uint32_t resistances[7];                 // Size:7
        uint32_t baseMana;                       // Size:1
        uint32_t baseHealth;                     // Size:1
        uint32_t bytes2;                         // Size:1
        uint32_t attackPower;                    // Size:1
        uint32_t attackPowerMods;                // Size:1
        float attackPowerMultiplier;             // Size:1
        uint32_t rangedAttackPower;              // Size:1
        uint32_t rangedAttackPowerMods;          // Size:1
        float rangedAttackPowerMultiplier;       // Size:1
        float minRangedDamage;                   // Size:1
        float maxRangedDamage;                   // Size:1
        float powerCostModifier[7];              // Size:7
        float powerCostMultiplier[7];            // Size:7
    } UnitFields;

    using ClntObjMgrObjectPtrT = uintptr_t* (__fastcall *)(OBJECT_TYPE_MASK typeMask, const char *debugMessage, unsigned __int64 guid, int debugCode);

    using ISceneEndT = int *(__fastcall *)(uintptr_t *unk);
    using EndSceneT = int (__fastcall *)(uintptr_t *unk);
    using GetTimeMsT = uint64_t (__stdcall *)();

    using StdcallT = void (__stdcall *)(void);
    using FastcallFrameT = void (__fastcall *)(uintptr_t *frame);
    using FrameBatchT = void (__fastcall *)(uintptr_t *frame, void *param_1, uint32_t param_2, int unk);

    using FrameOnLayerUpdateT = void (__fastcall *)(uintptr_t *frame, uint8_t unk, int unk2);

    using SignalEventT = void (__fastcall *)(int eventCode);
    using SignalEventParamT = void (__cdecl *)(int eventId, const char *formatString, ...);

    using FrameOnScriptEventT = void (__fastcall *)(int *param_1, int *param_2);
    using FrameOnScriptEventParamT = void (__cdecl *)(int *param_1, int *param_2, char *param_3, va_list args);
    using FrameScript_ExecuteT = void (__cdecl *)(int *param_1, int *param_2, char *param_3);

    using WorldUpdateT = void (__fastcall *)(float *param_1, float *param_2, float *param_3);

    using GetClientConnectionT = uintptr_t *(__stdcall *)();
    using GetNetStatsT = void (__thiscall *)(uintptr_t *connection, float *param_1, float *param_2, uint32_t *param_3);

    using LoadScriptFunctionsT = void (__stdcall *)();
    using FrameScript_RegisterFunctionT = void (__fastcall *)(char *name, uintptr_t *func);
    using FrameScript_CreateEventsT = void (__fastcall *)(int param_1, uint32_t maxEventId);

    using LuaGetContextT = uintptr_t *(__fastcall *)(void);
    using LuaGetTableT = void (__fastcall *)(uintptr_t *luaState, int globalsIndex);
    using LuaCallT = void (__fastcall *)(const char *code, const char *unused);
    using LuaScriptT = uint32_t (__fastcall *)(uintptr_t *luaState);
    using GetGUIDFromNameT = std::uint64_t (__fastcall *)(const char *);
    using GetUnitFromNameT = uintptr_t * (__fastcall *)(const char *);
    
    struct AlwaysRenderPlayer {
        char* name;
        std::uint64_t guid;
        bool resolved;
        
        AlwaysRenderPlayer(const char* playerName) : guid(0), resolved(false) {
            size_t len = strlen(playerName) + 1;
            name = new char[len];
            strcpy(name, playerName);
        }
        
        ~AlwaysRenderPlayer() {
            delete[] name;
        }
        
        AlwaysRenderPlayer(const AlwaysRenderPlayer& other) : guid(other.guid), resolved(other.resolved) {
            size_t len = strlen(other.name) + 1;
            name = new char[len];
            strcpy(name, other.name);
        }
        
        AlwaysRenderPlayer& operator=(const AlwaysRenderPlayer& other) {
            if (this != &other) {
                delete[] name;
                size_t len = strlen(other.name) + 1;
                name = new char[len];
                strcpy(name, other.name);
                guid = other.guid;
                resolved = other.resolved;
            }
            return *this;
        }
    };
    using lua_gettableT = void (__fastcall *)(uintptr_t *luaState, int globalsIndex);
    using lua_isstringT = bool (__fastcall *)(uintptr_t *, int);
    using lua_isnumberT = bool (__fastcall *)(uintptr_t *, int);
    using lua_tostringT = char *(__fastcall *)(uintptr_t *, int);
    using lua_tonumberT = double (__fastcall *)(uintptr_t *, int);
    using lua_pushnumberT = void (__fastcall *)(uintptr_t *, double);
    using lua_pushstringT = void (__fastcall *)(uintptr_t *, char *);
    using lua_pcallT = int (__fastcall *)(uintptr_t *, int nArgs, int nResults, int errFunction);
    using lua_pushnilT = void (__fastcall *)(uintptr_t *);
    using lua_errorT = void (__cdecl *)(uintptr_t *, const char *);
    using lua_settopT = void (__fastcall *)(uintptr_t *, int);
    using lua_getgccountT = int (__fastcall *)(uintptr_t *);
    using lua_getgcthresholdT = int (__fastcall *)(uintptr_t *);
    using lua_setgcthresholdT = void (__fastcall *)(uintptr_t *, int newthreshold);

    using GxRsSetT = void (__fastcall *)(int renderState, int enabled);

    using SpellVisualsInitializeT = void (__stdcall *)(void);

    using PlaySpellVisualT = void (__fastcall *)(uintptr_t *unit, uintptr_t *unk, uintptr_t *spellRec,
                                                 uintptr_t *visualKit, void *param_3, void *param_4);

    using UnknownOnRender1T = void (__stdcall *)(void);
    using UnknownOnRender2T = void (__stdcall *)(void);
    using UnknownOnRender3T = void (__stdcall *)(void);

    using PaintScreenT = void (__stdcall *)(uint32_t param_1, uint32_t param_2);

    using CM2SceneAdvanceTimeT = void (__fastcall *)(uintptr_t *this_ptr, void *dummy_edx, uint32_t param_1);
    using CM2SceneAnimateT = void (__fastcall *)(uintptr_t *this_ptr, void *dummy_edx, float *param_1);
    using CM2SceneDrawT = void (__fastcall *)(uintptr_t *this_ptr, void *dummy_edx, int param_1);

    using DrawBatchProjT = void (__fastcall *)(uintptr_t *this_ptr, void *dummy_edx);
    using DrawBatchT = void (__fastcall *)(uintptr_t *this_ptr, void *dummy_edx);
    using DrawBatchDoodadT = void (__fastcall *)(uintptr_t *this_ptr, void *dummy_edx, int param_1, int param_2);
    using DrawRibbonT = void (__fastcall *)(uintptr_t *this_ptr, void *dummy_edx);
    using DrawParticleT = void (__fastcall *)(uintptr_t *this_ptr, void *dummy_edx);
    using DrawCallbackT = void (__fastcall *)(uintptr_t *this_ptr, void *dummy_edx);
    using CM2SceneRenderDrawT = void (__fastcall *)(uintptr_t *this_ptr, void *dummy_edx, uint32_t param_1, int param_2,
                                                    int param_3, uint32_t param_4);

    using Script_RollOnLootT = int (__fastcall *)(uintptr_t *param_1);

    using MovementIdleMoveUnitsT = int (__stdcall *)(void);

    using WorldObjectRenderT = int (__fastcall *)(int param_1, int *matrix, int param_3, int param_4, int param_5);

    using luaC_collectgarbageT = void (__fastcall *)(int param_1);

    using CM2ModelAnimateMTT = void (__fastcall *)(uintptr_t *this_ptr, void *dummy_edx, float *param_1, float *param_2,
                                                   float *param_3, float *param_4);

    using ObjectFreeT = void (__fastcall *)(int param_1, uint32_t param_2);

    using CGUnitPreAnimateT = void (__fastcall *)(uintptr_t *this_ptr, void *dummy_edx, void *param_1);
    using CGUnitAnimateT = void (__fastcall *)(uintptr_t *this_ptr, void *dummy_edx, int *param_3);
    using CGUnitShouldRenderT = uint32_t (__fastcall *)(uintptr_t *this_ptr, void *dummy_edx, uint32_t param_1);
    using CGCorpseShouldRenderT = uint32_t (__fastcall *)(uintptr_t *this_ptr, void *dummy_edx, uint32_t param_1);
    using CGUnitGetPositionT = C3Vector *(__thiscall *)(uintptr_t *this_ptr, C3Vector* param_1);

    using CGUnitGetNameT = char * (__thiscall *)(uintptr_t *this_ptr, uint32_t flag);
    using CGUnitCanAttackT = bool * (__thiscall *)(uintptr_t *this_ptr, uintptr_t *unit);

    using CVarLookupT = uintptr_t *(__fastcall *)(const char *);
    using SetCVarT = int (__fastcall *)(uintptr_t *luaPtr);
    using CVarRegisterT = int *(__fastcall *)(char *name, char *help, int unk1, const char *defaultValuePtr,
                                              void *callbackPtr,
                                              int category, char unk2, int unk3);

    using SendUnitSignalT = void (__fastcall *)(uint64_t *guid, uint32_t eventCode);
    using GetNamesFromGUIDT = char** (__fastcall *)(uint64_t *guid, int *numNamesReturn);
    using SignalEventParamSingleStringT = int (__cdecl *)(uint32_t eventCode, char *format, char *str);

}