#pragma once
#include "pch.h"

// ScoutFreecam ApplyFreecam, rewritten in C++ so the DLL does not need the .pak.
namespace FreecamFly
{
	// Call every frame. bFlying is the current fly state (hotkey already applied).
	void Tick(
		bool bFeatureEnabled,
		bool bFlying,
		bool bFasterHeld,
		float flFlySpeed);
}
