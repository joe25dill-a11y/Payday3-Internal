#pragma once

#include "../Features.hpp"
#include <string>

namespace Cheat::NoCivPenalty
{
    extern std::string g_sStatus;

    // Zero civ-kill + custody end penalties (HaveCivilianKilledCount, PlayerInCustody cash). Starts OFF.
    void OnPlayerControllerTick(
        SDK::UWorld* pGWorld,
        SDK::ASBZPlayerController* pLocalController,
        SDK::ASBZPlayerCharacter* pLocalPlayer);
}
