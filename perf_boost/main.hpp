//
// Created by pmacc on 9/21/2024.
//

#pragma once

#include <hadesmem/process.hpp>
#include <hadesmem/patcher.hpp>

#include <Windows.h>

#include "types.hpp"

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
#include <sstream>

namespace perf_boost {
    // Function pointer type definitions
    using ClntObjMgrObjectPtrT = uintptr_t *(__fastcall *)(OBJECT_TYPE_MASK typeMask, const char *debugMessage,
                                                           unsigned __int64 guid, int debugCode);

    using ISceneEndT = int *(__fastcall *)(uintptr_t *unk);
    using EndSceneT = int (__fastcall *)(uintptr_t *unk);
    using GetTimeMsT = uint64_t (__stdcall *)();

    using FastcallFrameT = void (__fastcall *)(uintptr_t *frame);
    using FrameBatchT = void (__fastcall *)(uintptr_t *frame, void *param_1, uint32_t param_2, int unk);

    using FrameOnLayerUpdateT = void (__fastcall *)(uintptr_t *frame, uint8_t unk, int unk2);

    using SignalEventT = void (__fastcall *)(int eventCode);
    using SignalEventParamT = void (__cdecl *)(int eventId, const char *formatString, ...);

    using FrameOnScriptEventT = void (__fastcall *)(int *param_1, int *param_2);
    using FrameOnScriptEventParamT = void (__cdecl *)(int *param_1, int *param_2, char *param_3, va_list args);
    using FrameScript_ExecuteT = void (__cdecl *)(int *param_1, int *param_2, char *param_3);

    using WorldUpdateT = void (__fastcall *)(float *param_1, float *param_2, float *param_3);

    using GetNetStatsT = void (__thiscall *)(uintptr_t *connection, float *param_1, float *param_2, uint32_t *param_3);

    using FrameScript_RegisterFunctionT = void (__fastcall *)(char *name, uintptr_t *func);
    using FrameScript_CreateEventsT = void (__fastcall *)(int param_1, uint32_t maxEventId);

    using LuaGetContextT = uintptr_t *(__fastcall *)(void);
    using LuaGetTableT = void (__fastcall *)(uintptr_t *luaState, int globalsIndex);
    using LuaCallT = void (__fastcall *)(const char *code, const char *unused);
    using LuaScriptT = uint32_t (__fastcall *)(uintptr_t *luaState);
    using GetGUIDFromNameT = std::uint64_t (__fastcall *)(const char *);
    using GetUnitFromNameT = uintptr_t *(__fastcall *)(const char *);

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

    using CGUnitPlaySpellVisualT = void (__fastcall *)(uintptr_t *unitPtr, void *dummy_edx, SpellRec *spellRec,
                                                       uintptr_t *visualKit, void *param_3, void *param_4);

    using CGUnitPlayChannelVisualT = void (__fastcall *)(uintptr_t *unitPtr, void *dummy_edx);

    using CGUnitGetAppropriateSpellVisualT = uintptr_t *(__fastcall *)(uintptr_t *unitPtr, void *dummy_edx,
                                                                       SpellRec *spellRec, uintptr_t *visualKit);

    using CGDynamicObjectGetVisualEffectNameRecT = SpellVisualEffectNameRec *(__fastcall *)(uintptr_t *dynamicObjPtr, void *dummy_edx);
    
    using CGDynamicObjectUpdateModelLoadStatusT = void (__fastcall *)(uintptr_t *dynamicObjPtr, void *dummy_edx, int param_1);

    using GetSpellVisualT = uintptr_t* (__fastcall *)(SpellRec *param_1);

    using ObjectVisKitProcT = void (__fastcall *)(uintptr_t *dynamicObjPtr);

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
    using CM2ModelSetAnimatingT = void (__fastcall *)(uintptr_t *this_ptr, void *dummy_edx, int param_1);

    using ObjectFreeT = void (__fastcall *)(int param_1, uint32_t param_2);

    using CGUnitPreAnimateT = void (__fastcall *)(uintptr_t *this_ptr, void *dummy_edx, void *param_1);
    using CGUnitAnimateT = void (__fastcall *)(uintptr_t *this_ptr, void *dummy_edx, int *param_3);
    using CGUnitShouldRenderT = uint32_t (__fastcall *)(uintptr_t *this_ptr, void *dummy_edx, uint32_t param_1);
    using CGCorpseShouldRenderT = uint32_t (__fastcall *)(uintptr_t *this_ptr, void *dummy_edx, uint32_t param_1);
    using CGUnitGetPositionT = C3Vector *(__thiscall *)(uintptr_t *this_ptr, C3Vector *param_1);

    using CGUnitGetNameT = char *(__thiscall *)(uintptr_t *this_ptr, uint32_t flag);
    using CGUnitCanAttackT = bool *(__thiscall *)(uintptr_t *this_ptr, uintptr_t *unit);

    using CVarLookupT = uintptr_t *(__fastcall *)(const char *);
    using SetCVarT = int (__fastcall *)(uintptr_t *luaPtr);
    using CVarRegisterT = int *(__fastcall *)(char *name, char *help, int unk1, const char *defaultValuePtr,
                                              void *callbackPtr,
                                              int category, char unk2, int unk3);

    using SendUnitSignalT = void (__fastcall *)(uint64_t *guid, uint32_t eventCode);
    using GetNamesFromGUIDT = char **(__fastcall *)(uint64_t *guid, int *numNamesReturn);
    using SignalEventParamSingleStringT = int (__cdecl *)(uint32_t eventCode, char *format, char *str);
}