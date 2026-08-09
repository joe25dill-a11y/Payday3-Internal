#pragma once

#include "../Features.hpp"
#include <string>

namespace Cheat::GodAmmo
{
    extern std::string g_sStatusGod;
    extern std::string g_sStatusAmmo;
    extern std::string g_sStatusInstaKill;

    // SkyCheats Num0-quality: true god + true infinite ammo + insta kill (checkbox, starts OFF).
    void OnPlayerControllerTick(
        SDK::UWorld* pGWorld,
        SDK::ASBZPlayerController* pLocalController,
        SDK::ASBZPlayerCharacter* pLocalPlayer);
}
