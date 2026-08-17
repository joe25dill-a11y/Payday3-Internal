#pragma once
#include "pch.h"
#include <string>

namespace GrabAccess
{
	extern std::string g_sDebugStatus;

	void Tick(
		SDK::UWorld* pGWorld,
		SDK::ASBZPlayerController* pLocalController,
		SDK::ASBZPlayerCharacter* pLocalPlayer,
		bool bEnabled);
}
