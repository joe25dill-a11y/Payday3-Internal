#pragma once
#include "pch.h"
#include <string>

namespace VaultCodes
{
	extern std::string g_sStatus;
	extern std::string g_sHelperStatus;

	void RequestScan();
	bool TryGetFlashText(std::string& outText);

	void Tick(
		SDK::UWorld* pGWorld,
		SDK::ASBZPlayerController* pLocalController,
		SDK::ASBZPlayerCharacter* pLocalPlayer,
		bool bKeypadHelper);
}
