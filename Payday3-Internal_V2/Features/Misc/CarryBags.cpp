#include "pch.h"
#include "CarryBags.hpp"
#include "HeistUtil.hpp"

#include <Windows.h>
#include <chrono>
#include <string>

namespace CarryBags
{
	std::string g_sStatus = "Carry More Bags off";

	namespace
	{
		constexpr int32_t kCarryCap = 50;
		constexpr auto kRescanGap = std::chrono::milliseconds(2000);

		static bool s_bWasOn = false;
		static std::chrono::steady_clock::time_point s_timeScan{};

		static bool ActorOkSeh(SDK::AActor* pActor)
		{
			__try
			{
				return pActor && pActor->Class && !pActor->IsActorBeingDestroyed();
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				return false;
			}
		}

		static bool BumpCarrySeh(SDK::ASBZCharacter* pChar)
		{
			__try
			{
				if (!pChar)
					return false;
				if (pChar->MaxCarryBagCount < kCarryCap)
					pChar->MaxCarryBagCount = kCarryCap;
				return true;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				return false;
			}
		}

		static bool GetActorsSeh(SDK::UWorld* pWorld, SDK::UClass* pCls, SDK::TArray<SDK::AActor*>* pOut)
		{
			__try
			{
				if (!pWorld || !pCls || !pOut)
					return false;
				SDK::UGameplayStatics::GetAllActorsOfClass(pWorld, pCls, pOut);
				return true;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				return false;
			}
		}
	}

	void Tick(
		SDK::UWorld* pGWorld,
		SDK::ASBZPlayerController* /*pLocalController*/,
		SDK::ASBZPlayerCharacter* pLocalPlayer,
		bool bEnabled)
	{
		if (!bEnabled)
		{
			if (s_bWasOn)
			{
				s_bWasOn = false;
				g_sStatus = "Carry More Bags off";
			}
			return;
		}

		s_bWasOn = true;
		if (!pGWorld || !ActorOkSeh(pLocalPlayer) || !HeistUtil::IsInHeist())
		{
			g_sStatus = "Carry More Bags ON — wait for heist";
			return;
		}

		const auto now = std::chrono::steady_clock::now();
		if (now - s_timeScan < kRescanGap && g_sStatus.find("you=") != std::string::npos)
			return;
		s_timeScan = now;

		int you = 0;
		int players = 0;
		int ai = 0;

		if (BumpCarrySeh(pLocalPlayer))
			you = 1;

		// Other human pawns — local write only (helps host/AI bots; remote clients need their own DLL)
		if (auto* pPlayerCls = SDK::ASBZPlayerCharacter::StaticClass())
		{
			SDK::TArray<SDK::AActor*> others{};
			if (GetActorsSeh(pGWorld, pPlayerCls, &others))
			{
				for (int i = 0; i < others.Num(); ++i)
				{
					auto* pOther = reinterpret_cast<SDK::ASBZPlayerCharacter*>(others[i]);
					if (!ActorOkSeh(pOther) || pOther == pLocalPlayer)
						continue;
					if (BumpCarrySeh(pOther))
						++players;
				}
			}
		}

		if (auto* pCrewCls = SDK::ASBZAICrewCharacter::StaticClass())
		{
			SDK::TArray<SDK::AActor*> crew{};
			if (GetActorsSeh(pGWorld, pCrewCls, &crew))
			{
				for (int i = 0; i < crew.Num(); ++i)
				{
					auto* pCrew = reinterpret_cast<SDK::ASBZAICrewCharacter*>(crew[i]);
					if (!ActorOkSeh(pCrew))
						continue;
					if (BumpCarrySeh(pCrew))
						++ai;
				}
			}
		}

		g_sStatus = "Carry More Bags ON (cap " + std::to_string(kCarryCap)
			+ ") you=" + std::to_string(you)
			+ " players=" + std::to_string(players)
			+ " AI=" + std::to_string(ai);
	}
}
