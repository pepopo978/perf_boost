# PerfBoost

A performance optimization DLL for World of Warcraft 1.12.1 that provides advanced render distance controls to improve FPS in crowded areas and in raids.

# Companion addon
https://github.com/pepopo978/PerfBoostSettings

## Features

- **Selective Unit Rendering**: Control render distances for different types of units (players, pets, trash mobs, corpses)
- **Context-Aware Settings**: Different render distances for combat vs non-combat, and city vs outdoor areas
- **Smart Exceptions**: Always render raid-marked units, PvP enemies, and other important targets
- **Performance Optimized**: Uses fast distance approximation and frame-based caching for minimal overhead

### Installation
Grab the latest perf_boost.dll from https://github.com/pepopo978/perf_boost/releases and place in the same directory as WoW.exe.  You can also get the helper addon mentioned below and place that in Interface/Addons.
You will need to add it to your dlls.txt file.

If you would prefer to compile yourself you will need to get:
- boost 1.80 32 bit from https://www.boost.org/users/history/version_1_80_0.html
- hadesmem from https://github.com/namreeb/hadesmem

CMakeLists.txt is currently looking for boost at `set(BOOST_INCLUDEDIR "C:/software/boost_1_80_0")` and hadesmem at `set(HADESMEM_ROOT "C:/software/hadesmem-v142-Debug-Win32")`.  Edit as needed.

### Configuration

#### Configure with addon
There is a companion addon to make it easy to check/change the settings in game.  You can download it here - https://github.com/pepopo978/PerfBoostSettings

#### Manual Configuration
The following CVars control the behavior of the spell queuing system:

### Core Settings

#### `PB_Enabled`
- **Default**: `1` (enabled)
- **Description**: Master switch to enable/disable all performance boost features
- **Values**: `1` = enabled, `0` = disabled

### Player Render Distance

#### `PB_PlayerRenderDist`
- **Default**: `-1` (disabled)
- **Description**: Maximum distance to render other players when not in combat
- **Values**: Distance in yards, `-1` = no limit, `0` = hide all players

#### `PB_PlayerRenderDistInCities`
- **Default**: `-1` (disabled)
- **Description**: Override player render distance when in major cities (Stormwind, Orgrimmar, Ironforge, Undercity, Darnassus, Thunder Bluff)
- **Values**: Distance in yards, `-1` = use normal distance

#### `PB_PlayerRenderDistInCombat`
- **Default**: `-1` (disabled)
- **Description**: Override player render distance when in combat
- **Values**: Distance in yards, `-1` = use normal distance

### Pet Render Distance

#### `PB_PetRenderDist`
- **Default**: `-1` (disabled)
- **Description**: Maximum distance to render pets, totems, and other player-controlled units
- **Values**: Distance in yards, `-1` = no limit

#### `PB_PetRenderDistInCombat`
- **Default**: `-1` (disabled)
- **Description**: Override pet render distance when in combat
- **Values**: Distance in yards, `-1` = use normal distance

### NPC Render Distance

#### `PB_TrashUnitRenderDist`
- **Default**: `-1` (disabled)
- **Description**: Maximum distance to render low-level NPCs (level < 63)
- **Values**: Distance in yards, `-1` = no limit

#### `PB_TrashUnitRenderDistInCombat`
- **Default**: `-1` (disabled)
- **Description**: Override trash unit render distance when in combat
- **Values**: Distance in yards, `-1` = use normal distance

### Corpse Render Distance

#### `PB_CorpseRenderDist`
- **Default**: `-1` (disabled)
- **Description**: Maximum distance to render corpses
- **Values**: Distance in yards, `-1` = no limit

### Special Rendering

#### `PB_AlwaysRenderRaidMarks`
- **Default**: `1` (enabled)
- **Description**: Always render units with raid markers (skull, cross, etc.) regardless of distance settings
- **Values**: `1` = enabled, `0` = disabled

### Event Filtering

#### `PB_FilterGuidEvents`
- **Default**: `1` (enabled)
- **Description**: Filters out generally unnecessary GUID-based events to reduce event spam and improve performance. Blocks events like UNIT_AURA, UNIT_HEALTH, UNIT_MANA when triggered with a guid instead of a string like 'player' or 'raid1', while preserving important events like UNIT_COMBAT and UNIT_MODEL_CHANGED.
- **Values**: `1` = enabled, `0` = disabled

## Legal

This tool modifies game rendering behavior for performance optimization. Use at your own risk/discretion and in compliance with your server's rules.
