#pragma once

#include "../Features.hpp"
#include <string>

namespace Cheat::SilentKill
{
    extern std::string g_sDebugStatus;

    // Num* equivalent — silent bury-kill COPS ONLY (CH_BaseCop).
    // Never Houston / FWB inside man / civs / crew. Checkbox only (Lua keeps Num*).
    void OnPlayerControllerTick(
        SDK::UWorld* pGWorld,
        SDK::ASBZPlayerController* pLocalController,
        SDK::ASBZPlayerCharacter* pLocalPlayer);
}
