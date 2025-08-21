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

enum class Offsets : std::uint32_t {
    OsGetAsyncTimeMs = 0X0042B790,

    ClntObjMgrObjectPtr = 0x00468460,
    GetObjectPtr = 0x464870,
    GetActivePlayer = 0x468550,
    GetUnitFromName = 0x00515940,
    GetGUIDFromName = 0x00515970,

    Language = 0xC0E080,
    SpellDb = 0xC0D780,

    SpellVisualsInitialize = 0X006EC0E0,
    OnWorldRender = 0x00483460,

    lua_state_ptr = 0x7040D0,

    lua_isstring = 0x006F3510,
    lua_isnumber = 0X006F34D0,
    lua_tostring = 0x006F3690,
    lua_tonumber = 0X006F3620,
    lua_pushnumber = 0X006F3810,
    lua_gettable = 0X6F3A40,
    lua_pushstring = 0X006F3890,
    lua_pushnil = 0X006F37F0,
    lua_call = 0x00704CD0,
    lua_pcall = 0x006F41A0,
    lua_error = 0X006F4940,
    lua_settop = 0X006F3080,

    luaC_collectgarbage = 0X006F7340,
    lua_getcontext = 0x007040D0,
    lua_getgccount = 0x006f43f0,

    CM2ModelAnimateMT = 0x00714260,
    CM2ModelSetAnimating = 0X00710B90,
    ObjectFree = 0X00463B00,

    GetSpellVisual = 0X006EC1E0,
    ObjectVisKitProc = 0x005d55c0,

    CGUnitPreAnimate = 0x00607ed0,
    CGUnitAnimate = 0x00608560,
    CGUnitShouldRender = 0x00607da0,
    CGUnitGetUnitName = 0x00609210,
    CGUnitCanAttack = 0x0000606980,
    CGUnitPlaySpellVisual = 0X0060EDF0,
    CGUnitPlayChannelVisual = 0x00612a30,
    CGUnitGetAppropriateSpellVisual = 0X0060D450,

    CGCorpseShouldRender = 0x005d67e0,

    CGUnitGetPosition = 0x5f1f10,

    CVarLookup = 0x0063DEC0,
    RegisterCVar = 0X0063DB90,
    Script_SetCVar = 0x00488C10,

    SendUnitSignal = 0x00515e50,
    GetNamesFromGUID = 0x00515c50,
    SignalEventParam = 0x703F50,

    RealZoneText = 0X0B4B404,
    ZoneAreaIds = 0X0B4E314,

    CGDynamicObjectGetVisualEffectNameRec = 0x005d57c0,
    CGDynamicObjectUpdateModelLoadStatus = 0x005d52f0,
};
