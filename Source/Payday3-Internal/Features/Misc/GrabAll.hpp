#pragma once

#include "../Features.hpp"
#include <string>

namespace Cheat::GrabAll
{
    extern std::string g_sDebugStatus;

    void OnPlayerControllerTick(
        SDK::UWorld* pGWorld,
        SDK::ASBZPlayerController* pLocalController,
        SDK::ASBZPlayerCharacter* pLocalPlayer);
}
