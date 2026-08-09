#pragma once

#include "../Features.hpp"
#include <string>

namespace Cheat::FriendlyFire
{
    extern std::string g_sDebugStatus;

    void OnPlayerControllerTick(
        SDK::UWorld* pGWorld,
        SDK::USBZWorldRuntime* pWorldRuntime,
        SDK::ASBZPlayerController* pLocalController,
        SDK::ASBZPlayerCharacter* pLocalPlayer);

    void NotifyRemoteShot(SDK::UObject* pObject);
    void OnPossibleShot(SDK::UObject* pObject);
    void ProcessPendingHits();
}
