#pragma once

#include "../Features.hpp"
#include <string>

namespace Cheat::VaultCodes
{
    extern std::string g_sStatus;

    // One-shot from Misc button / F10 hotkey.
    void RequestScan();

    // True while overlay flash text should draw ( ~4s after scan ).
    bool TryGetFlashText(std::string& outText);

    void OnPlayerControllerTick(
        SDK::UWorld* pGWorld,
        SDK::ASBZPlayerController* pLocalController,
        SDK::ASBZPlayerCharacter* pLocalPlayer);
}
