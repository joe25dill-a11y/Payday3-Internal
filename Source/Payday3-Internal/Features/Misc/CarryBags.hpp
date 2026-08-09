#pragma once

#include "../Features.hpp"
#include <string>

namespace Cheat::CarryBags
{
    extern std::string g_sStatus;

    // Raise MaxCarryBagCount for local player + AI crew (SkysBags-style). Starts OFF.
    void OnPlayerControllerTick(
        SDK::UWorld* pGWorld,
        SDK::ASBZPlayerController* pLocalController,
        SDK::ASBZPlayerCharacter* pLocalPlayer);
}
