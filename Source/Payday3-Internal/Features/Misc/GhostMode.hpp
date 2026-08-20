#pragma once

#include "../Features.hpp"
#include <string>

namespace Cheat::GhostMode
{
    extern std::string g_sStatus;

    // Cams + guards ignore you while Misc checkbox / F11 is on.
    void OnPlayerControllerTick(
        SDK::UWorld* pGWorld,
        SDK::ASBZPlayerController* pLocalController,
        SDK::ASBZPlayerCharacter* pLocalPlayer);
}
