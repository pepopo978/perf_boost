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

#pragma once

#include <cstdint>
#include <cstdarg>

namespace perf_boost {
    typedef struct C3Vector {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    } C3Vector;

    typedef enum OBJECT_TYPE_MASK {
        TYPE_MASK_OBJECT = 1,
        TYPE_MASK_ITEM = 2,
        TYPE_MASK_CONTAINER = 4,
        TYPE_MASK_UNIT = 8,
        TYPE_MASK_PLAYER = 16,
        TYPE_MASK_GAMEOBJECT = 32,
        TYPE_MASK_DYNAMICOBJECT = 64,
        TYPE_MASK_CORPSE = 128,
        TYPE_MASK_AIGROUP = 256,
        TYPE_MASK_AREATRIGGER = 512
    } OBJECT_TYPE_MASK;

    enum OBJECT_TYPE_ID {
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

    enum UnitFlags {
        UNIT_FLAG_NONE = 0x00000000,
        UNIT_FLAG_UNK_0 = 0x00000001,           // Movement checks disabled, likely paired with loss of client control packet.
        UNIT_FLAG_SPAWNING = 0x00000002,           // not attackable
        UNIT_FLAG_DISABLE_MOVE = 0x00000004,
        UNIT_FLAG_PLAYER_CONTROLLED = 0x00000008,           // players, pets, totems, guardians, companions, charms, any units associated with players
        UNIT_FLAG_PET_RENAME = 0x00000010,           // Old pet rename: moved to UNIT_FIELD_BYTES_2,2 in TBC+
        UNIT_FLAG_PET_ABANDON = 0x00000020,           // Old pet abandon: moved to UNIT_FIELD_BYTES_2,2 in TBC+
        UNIT_FLAG_UNK_6 = 0x00000040,
        UNIT_FLAG_IMMUNE_TO_PLAYER = 0x00000100,           // Target is immune to players
        UNIT_FLAG_IMMUNE_TO_NPC = 0x00000200,           // Target is immune to creatures
        UNIT_FLAG_PVP = 0x00001000,
        UNIT_FLAG_SILENCED = 0x00002000,           // silenced, 2.1.1
        UNIT_FLAG_UNK_14 = 0x00004000,
        UNIT_FLAG_USE_SWIM_ANIMATION = 0x00008000,
        UNIT_FLAG_NON_ATTACKABLE_2 = 0x00010000,           // removes attackable icon, if on yourself, cannot assist self but can cast TARGET_UNIT_CASTER spells - added by SPELL_AURA_MOD_UNATTACKABLE
        UNIT_FLAG_PACIFIED = 0x00020000,
        UNIT_FLAG_STUNNED = 0x00040000,           // Unit is a subject to stun, turn and strafe movement disabled
        UNIT_FLAG_IN_COMBAT = 0x00080000,
        UNIT_FLAG_TAXI_FLIGHT = 0x00100000,           // Unit is on taxi, paired with a duplicate loss of client control packet (likely a legacy serverside hack). Disables any spellcasts not allowed in taxi flight client-side.
        UNIT_FLAG_CONFUSED = 0x00400000,           // Unit is a subject to confused movement, movement checks disabled, paired with loss of client control packet.
        UNIT_FLAG_FLEEING = 0x00800000,           // Unit is a subject to fleeing movement, movement checks disabled, paired with loss of client control packet.
        UNIT_FLAG_POSSESSED = 0x01000000,           // Unit is under remote control by another unit, movement checks disabled, paired with loss of client control packet. New master is allowed to use melee attack and can't select this unit via mouse in the world (as if it was own character).
        UNIT_FLAG_NOT_SELECTABLE = 0x02000000,
        UNIT_FLAG_SKINNABLE = 0x04000000,
        UNIT_FLAG_AURAS_VISIBLE = 0x08000000,           // magic detect
        UNIT_FLAG_SHEATHE = 0x40000000,
        UNIT_FLAG_IMMUNE = 0x80000000,           // Immune to damage

        // [-ZERO] TBC enumerations [?]
        UNIT_FLAG_NOT_ATTACKABLE_1 = 0x00000080,           // ?? (UNIT_FLAG_PLAYER_CONTROLLED | UNIT_FLAG_NOT_ATTACKABLE_1) is NON_PVP_ATTACKABLE
        UNIT_FLAG_LOOTING = 0x00000400,           // loot animation
        UNIT_FLAG_PET_IN_COMBAT = 0x00000800,           // in combat?, 2.0.8
        UNIT_FLAG_DISARMED = 0x00200000,           // disable melee spells casting..., "Required melee weapon" added to melee spells tooltip.

        UNIT_FLAG_UNK_28 = 0x10000000,
        UNIT_FLAG_UNK_29 = 0x20000000,           // used in Feing Death spell
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


    typedef enum GameObjectType {
        GAMEOBJECT_TYPE_DOOR = 0,
        GAMEOBJECT_TYPE_BUTTON = 1,
        GAMEOBJECT_TYPE_QUESTGIVER = 2,
        GAMEOBJECT_TYPE_CHEST = 3,
        GAMEOBJECT_TYPE_BINDER = 4,
        GAMEOBJECT_TYPE_GENERIC = 5,
        GAMEOBJECT_TYPE_TRAP = 6,
        GAMEOBJECT_TYPE_CHAIR = 7,
        GAMEOBJECT_TYPE_SPELL_FOCUS = 8,
        GAMEOBJECT_TYPE_TEXT = 9,
        GAMEOBJECT_TYPE_GOOBER = 10,
        GAMEOBJECT_TYPE_TRANSPORT = 11,
        GAMEOBJECT_TYPE_AREADAMAGE = 12,
        GAMEOBJECT_TYPE_CAMERA = 13,
        GAMEOBJECT_TYPE_MAP_OBJECT = 14,
        GAMEOBJECT_TYPE_MO_TRANSPORT = 15,
        GAMEOBJECT_TYPE_DUEL_ARBITER = 16,
        GAMEOBJECT_TYPE_FISHINGNODE = 17,
        GAMEOBJECT_TYPE_SUMMONING_RITUAL = 18,
        GAMEOBJECT_TYPE_MAILBOX = 19,
        GAMEOBJECT_TYPE_AUCTIONHOUSE = 20,
        GAMEOBJECT_TYPE_GUARDPOST = 21,
        GAMEOBJECT_TYPE_SPELLCASTER = 22,
        GAMEOBJECT_TYPE_MEETINGSTONE = 23,
        GAMEOBJECT_TYPE_FLAGSTAND = 24,
        GAMEOBJECT_TYPE_FISHINGHOLE = 25,
        GAMEOBJECT_TYPE_FLAGDROP = 26,
        GAMEOBJECT_TYPE_MINI_GAME = 27,
        GAMEOBJECT_TYPE_LOTTERY_KIOSK = 28,
        GAMEOBJECT_TYPE_CAPTURE_POINT = 29,
        GAMEOBJECT_TYPE_AURA_GENERATOR = 30,
        GAMEOBJECT_TYPE_MAX = 31
    } GameObjectType;

    struct DynamicObjectFields {
        uint64_t m_caster;
        uint32_t m_bytes;
        int32_t m_spellID;
        float m_radius;
        C3Vector m_position;
        float m_facing;
        int32_t m_padding;
    };


    typedef struct SpellRec {
        unsigned int Id;
        unsigned int School;
        unsigned int Category;
        unsigned int castUI;
        unsigned int Dispel;
        unsigned int Mechanic;
        unsigned int Attributes;
        unsigned int AttributesEx;
        unsigned int AttributesEx2;
        unsigned int AttributesEx3;
        unsigned int AttributesEx4;
        unsigned int Stances;
        unsigned int StancesNot;
        unsigned int Targets;
        unsigned int TargetCreatureType;
        unsigned int RequiresSpellFocus;
        unsigned int CasterAuraState;
        unsigned int TargetAuraState;
        unsigned int CastingTimeIndex;
        unsigned int RecoveryTime;
        unsigned int CategoryRecoveryTime;
        unsigned int InterruptFlags;
        unsigned int AuraInterruptFlags;
        unsigned int ChannelInterruptFlags;
        unsigned int procFlags;
        unsigned int procChance;
        unsigned int procCharges;
        unsigned int maxLevel;
        unsigned int baseLevel;
        unsigned int spellLevel;
        unsigned int DurationIndex;
        unsigned int powerType;
        unsigned int manaCost;
        unsigned int manaCostPerlevel;
        unsigned int manaPerSecond;
        unsigned int manaPerSecondPerLevel;
        unsigned int rangeIndex;
        float speed;
        unsigned int modalNextSpell;
        unsigned int StackAmount;
        unsigned int Totem[2];
        int Reagent[8];
        unsigned int ReagentCount[8];
        int EquippedItemClass;
        int EquippedItemSubClassMask;
        int EquippedItemInventoryTypeMask;
        unsigned int Effect[3];
        int EffectDieSides[3];
        unsigned int EffectBaseDice[3];
        float EffectDicePerLevel[3];
        float EffectRealPointsPerLevel[3];
        int EffectBasePoints[3];
        unsigned int EffectMechanic[3];
        unsigned int EffectImplicitTargetA[3];
        unsigned int EffectImplicitTargetB[3];
        unsigned int EffectRadiusIndex[3];
        unsigned int EffectApplyAuraName[3];
        unsigned int EffectAmplitude[3];
        float EffectMultipleValue[3];
        unsigned int EffectChainTarget[3];
        unsigned int EffectItemType[3];
        int EffectMiscValue[3];
        unsigned int EffectTriggerSpell[3];
        float EffectPointsPerComboPoint[3];
        unsigned int SpellVisual;
        unsigned int SpellVisual2;
        unsigned int SpellIconID;
        unsigned int activeIconID;
        unsigned int spellPriority;
        const char *SpellName[8];
        unsigned int SpellNameFlag;
        unsigned int Rank[8];
        unsigned int RankFlags;
        unsigned int Description[8];
        unsigned int DescriptionFlags;
        unsigned int ToolTip[8];
        unsigned int ToolTipFlags;
        unsigned int ManaCostPercentage;
        unsigned int StartRecoveryCategory;
        unsigned int StartRecoveryTime;
        unsigned int MaxTargetLevel;
        unsigned int SpellFamilyName;
        unsigned __int64 SpellFamilyFlags;
        unsigned int MaxAffectedTargets;
        unsigned int DmgClass;
        unsigned int PreventionType;
        unsigned int StanceBarOrder;
        float DmgMultiplier[3];
        unsigned int MinFactionId;
        unsigned int MinReputation;
        unsigned int RequiredAuraVision;
    } SpellRec;

    typedef struct SpellVisualEffectNameRec {
        unsigned int Id;
        const char* Name;
        const char* FileName;
    } SpellVisualEffectNameRec;

    enum SpellAttributesEx3 {
        SPELL_ATTR_EX3_PVP_ENABLING = 0x00000001,            // 0 Spell landed counts as hostile action against enemy even if it doesn't trigger combat state, propagates PvP flags
        SPELL_ATTR_EX3_NO_PROC_EQUIP_REQUIREMENT = 0x00000002,            // 1
        SPELL_ATTR_EX3_NO_CASTING_BAR_TEXT = 0x00000004,            // 2
        SPELL_ATTR_EX3_COMPLETELY_BLOCKED = 0x00000008,            // 3 All effects prevented on block
        SPELL_ATTR_EX3_NO_RES_TIMER = 0x00000010,            // 4 Corpse reclaim delay does not apply to accepting resurrection (only Rebirth has it)
        SPELL_ATTR_EX3_NO_DURABILITY_LOSS = 0x00000020,            // 5
        SPELL_ATTR_EX3_NO_AVOIDANCE = 0x00000040,            // 6 Persistent Area Aura not removed on leaving radius
        SPELL_ATTR_EX3_DOT_STACKING_RULE = 0x00000080,            // 7 Create a separate (de)buff stack for each caster
        SPELL_ATTR_EX3_ONLY_ON_PLAYER = 0x00000100,            // 8 Can target only players
        SPELL_ATTR_EX3_NOT_A_PROC = 0x00000200,            // 9 Aura periodic trigger is not evaluated as triggered
        SPELL_ATTR_EX3_REQUIRES_MAIN_HAND_WEAPON = 0x00000400,            // 10
        SPELL_ATTR_EX3_ONLY_BATTLEGROUNDS = 0x00000800,            // 11
        SPELL_ATTR_EX3_ONLY_ON_GHOSTS = 0x00001000,            // 12
        SPELL_ATTR_EX3_HIDE_CHANNEL_BAR = 0x00002000,            // 13 Client will not display channeling bar
        SPELL_ATTR_EX3_HIDE_IN_RAID_FILTER = 0x00004000,            // 14 Only "Honorless Target" has this flag
        SPELL_ATTR_EX3_NORMAL_RANGED_ATTACK = 0x00008000,            // 15 Spells with this attribute are processed as ranged attacks in client
        SPELL_ATTR_EX3_SUPPRESS_CASTER_PROCS = 0x00010000,            // 16
        SPELL_ATTR_EX3_SUPPRESS_TARGET_PROCS = 0x00020000,            // 17
        SPELL_ATTR_EX3_ALWAYS_HIT = 0x00040000,            // 18 Spell should always hit its target
        SPELL_ATTR_EX3_INSTANT_TARGET_PROCS = 0x00080000,            // 19 Related to spell batching
        SPELL_ATTR_EX3_ALLOW_AURA_WHILE_DEAD = 0x00100000,            // 20 Death persistent spells
        SPELL_ATTR_EX3_ONLY_PROC_OUTDOORS = 0x00200000,            // 21
        SPELL_ATTR_EX3_CASTING_CANCELS_AUTOREPEAT = 0x00400000,            // 22 NYI (only Shoot with Wand has it)
        SPELL_ATTR_EX3_NO_DAMAGE_HISTORY = 0x00800000,            // 23 NYI
        SPELL_ATTR_EX3_REQUIRES_OFFHAND_WEAPON = 0x01000000,            // 24
        SPELL_ATTR_EX3_TREAT_AS_PERIODIC = 0x02000000,            // 25 Does not cause spell pushback
        SPELL_ATTR_EX3_CAN_PROC_FROM_PROCS = 0x04000000,            // 26 Auras with this attribute can proc off procced spells (periodic triggers etc)
        SPELL_ATTR_EX3_ONLY_PROC_ON_CASTER = 0x08000000,            // 27
        SPELL_ATTR_EX3_IGNORE_CASTER_AND_TARGET_RESTRICTIONS = 0x10000000,   // 28 Skips all cast checks, moved from AttributesEx after 1.10 (100% correlation)
        SPELL_ATTR_EX3_IGNORE_CASTER_MODIFIERS = 0x20000000,            // 29
        SPELL_ATTR_EX3_DO_NOT_DISPLAY_RANGE = 0x40000000,            // 30
        SPELL_ATTR_EX3_NOT_ON_AOE_IMMUNE = 0x80000000             // 31
    };

    template<typename T>
    struct WowClientDB {
        T *m_records;
        int m_numRecords;
        T **m_recordsById;
        int m_maxId;
        int m_loaded;
    };

    struct PlayerData {
        char *name;
        std::uint64_t guid;
        bool resolved;

        PlayerData(const char *playerName) : guid(0), resolved(false) {
            size_t len = strlen(playerName) + 1;
            name = new char[len];
            strcpy(name, playerName);
        }

        ~PlayerData() {
            delete[] name;
        }

        PlayerData(const PlayerData &other) : guid(other.guid), resolved(other.resolved) {
            size_t len = strlen(other.name) + 1;
            name = new char[len];
            strcpy(name, other.name);
        }

        PlayerData &operator=(const PlayerData &other) {
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
}