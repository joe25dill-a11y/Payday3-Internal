#pragma once

#include "../Features.hpp"
#include <string>

namespace Cheat::SpawnerTools
{
    extern std::string g_sStatusMeth;
    extern std::string g_sStatusVan;
    extern std::string g_sStatusExit;
    extern std::string g_sStatusMoney;

    // One-shot requests from ImGui buttons (Lua keeps Num2/3/-/+).
    void RequestMeth();
    void RequestVan();
    void RequestGreenExit();
    void RequestMoneyScreen();

    // True while a button action or delayed van finalize / escape timer is outstanding.
    bool HasPendingWork();

    void OnPlayerControllerTick(
        SDK::UWorld* pGWorld,
        SDK::ASBZPlayerController* pLocalController,
        SDK::ASBZPlayerCharacter* pLocalPlayer);
}
