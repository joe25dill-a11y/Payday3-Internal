#pragma once

#include "../Features.hpp"
#include <string>

namespace Cheat::InstaDrill
{
    extern std::string g_sDebugStatus;

    // Num8 equivalent — drills / PCs / thermite / lance / cleaner / cable-box wires. Checkbox only (Lua keeps Num8).
    void OnPlayerControllerTick(
        SDK::UWorld* pGWorld,
        SDK::ASBZPlayerController* pLocalController,
        SDK::ASBZPlayerCharacter* pLocalPlayer);
}
