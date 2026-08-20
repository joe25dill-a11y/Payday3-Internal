#pragma once

#include "../Features.hpp"
#include <string>

namespace Cheat::CarryBodies
{
    extern std::string g_sStatus;

    void OnPlayerControllerTick(
        SDK::UWorld* pGWorld,
        SDK::ASBZPlayerController* pLocalController,
        SDK::ASBZPlayerCharacter* pLocalPlayer);

    bool TryProcessEventHook(const SDK::UObject* pObject, SDK::UFunction* pFunction, void* pParams);

    bool IsThrowCarryEvent(SDK::UFunction* pFunction);
    bool IsPickupFailedEvent(SDK::UFunction* pFunction);

    void OnThrowCarryPostHook(SDK::UObject* pObject);
    void OnPickupFailedPostHook(SDK::UObject* pObject, void* pParams);
}
