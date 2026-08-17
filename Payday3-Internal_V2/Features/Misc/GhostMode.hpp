#pragma once
#include "pch.h"
#include <string>

namespace GhostMode
{
	extern std::string g_sStatus;

	void Tick(
		SDK::UWorld* pGWorld,
		SDK::ASBZPlayerController* pLocalController,
		SDK::ASBZPlayerCharacter* pLocalPlayer,
		bool bEnabled);
}
