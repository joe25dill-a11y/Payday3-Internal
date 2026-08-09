#pragma once

#include "../Features.hpp"
#include <string>

namespace Cheat::GrabAccess
{
    extern std::string g_sDebugStatus;

    // Num/ equivalent — keycards / RFID / press badge. Checkbox only (Lua keeps Num/).
    void OnPlayerControllerTick(
        SDK::UWorld* pGWorld,
        SDK::ASBZPlayerController* pLocalController,
        SDK::ASBZPlayerCharacter* pLocalPlayer);
}
