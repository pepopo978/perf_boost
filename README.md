# PerfBoost

A performance optimization DLL for World of Warcraft 1.12.1 that provides advanced render distance controls to improve FPS in crowded areas and in raids.

# Companion addon
https://github.com/pepopo978/PerfBoostSettings

there is also a guide there for new users
https://github.com/pepopo978/PerfBoostSettings/blob/master/GUIDE.md

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

#### Configure with addon
There is a companion addon to make it easy to check/change the settings in game.  You can download it here - https://github.com/pepopo978/PerfBoostSettings

See the readme at https://github.com/pepopo978/PerfBoostSettings for descriptions of the cvars.

## Legal

This tool modifies game rendering behavior for performance optimization. Use at your own risk/discretion and in compliance with your server's rules.
