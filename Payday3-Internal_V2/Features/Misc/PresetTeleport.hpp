#pragma once
#include "pch.h"
#include <string>
#include <vector>

namespace PresetTeleport
{
	extern std::string g_sStatus;

	void PollPendingUi();
	void Tick(
		SDK::UWorld* pGWorld,
		SDK::ASBZPlayerController* pLocalController,
		SDK::ASBZPlayerCharacter* pLocalPlayer);

	void RequestLoadDialog();
	void PrevSpot();
	void NextSpot();
	void TeleportHere();
	void TeleportEveryone();
	void StartTourSpots();
	void StopTour();
	void SetTourDelay(float seconds);
	float GetTourDelay();
	int GetSpotCount();
	int GetSelectedIndex();
	void SetSelectedIndex(int index);
	std::string GetSpotName(int index);
	bool IsTouring();
}
