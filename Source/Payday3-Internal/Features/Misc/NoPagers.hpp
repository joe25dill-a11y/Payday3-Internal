#pragma once

#include "../Features.hpp"

namespace Cheat::NoPagers
{
	// Always-on, no menu. Kill a cop / lead guard and the radio never rings.
	void OnPlayerControllerTick(
		SDK::UWorld* pGWorld,
		SDK::ASBZPlayerController* pLocalController,
		SDK::ASBZPlayerCharacter* pLocalPlayer);
}
