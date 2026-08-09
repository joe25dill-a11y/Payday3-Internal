#pragma once

#include "../Features.hpp"
#include <string>

namespace Cheat::PresetTeleport
{
    extern std::string g_sStatus;

    // Heist Farmer-style JSON presets.
    // TP Here = you | TP All = everyone to selected spot | Tour Spots = you through every spot
    void DrawTab();
    void PollPendingUi(); // deferred file dialog from Load button
    bool NeedsPlayerTick();

    void OnPlayerControllerTick(
        SDK::UWorld* pGWorld,
        SDK::ASBZPlayerController* pLocalController,
        SDK::ASBZPlayerCharacter* pLocalPlayer);

    void RequestLoadDialog();
    void PrevSpot();
    void NextSpot();
    void TeleportHere();       // you only → selected
    void TeleportEveryone();   // you + AI + players → selected (no tour)
    void StartTourSpots();     // you only → every spot 1-by-1
    void StopTour();
}
