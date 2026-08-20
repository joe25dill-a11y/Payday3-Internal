#pragma once

#include "../Features.hpp"
#include <string>

namespace Cheat::ThirdPerson
{
    extern std::string g_sStatus;

    // Offset the *render* camera behind the local pawn. Fire path stays first-person.
    void ApplyCamera(SDK::ULocalPlayer* pLocalPlayer, SDK::FMinimalViewInfo* pView);

    void OnPlayerControllerTick(
        SDK::UWorld* pGWorld,
        SDK::ASBZPlayerController* pLocalController,
        SDK::ASBZPlayerCharacter* pLocalPlayer);
}
