#pragma once
#include "pch.h"
#include <string>

namespace SpawnerTools
{
	extern std::string g_sStatusMeth;
	extern std::string g_sStatusVan;
	extern std::string g_sStatusExit;
	extern std::string g_sStatusMoney;

	void RequestMeth();
	void RequestVan();
	void RequestGreenExit();
	void RequestMoneyScreen();

	bool HasPendingWork();

	void Tick(
		SDK::UWorld* pGWorld,
		SDK::ASBZPlayerController* pLocalController,
		SDK::ASBZPlayerCharacter* pLocalPlayer);
}
