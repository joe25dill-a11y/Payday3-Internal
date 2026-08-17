#pragma once
#include "pch.h"

namespace HeistUtil
{
	inline bool IsInHeist()
	{
		return Unreal::GetLocalCharacter() != nullptr;
	}

	inline bool IsSoloGame(SDK::UWorld* pWorld = nullptr)
	{
		if (!pWorld)
			pWorld = SDK::UWorld::GetWorld();
		if (!pWorld)
			return true;
		return SDK::USBZOnlineFunctionLibrary::IsSoloGame(pWorld);
	}

	inline SDK::ASBZPlayerController* GetLocalSBZController()
	{
		SDK::APlayerController* pPC = Unreal::GetPlayerController();
		if (!pPC || !pPC->IsA(SDK::ASBZPlayerController::StaticClass()))
			return nullptr;
		return reinterpret_cast<SDK::ASBZPlayerController*>(pPC);
	}

	inline SDK::UClass* FindClass(const char* name)
	{
		return SDK::UObject::FindClassFast(name);
	}
}
