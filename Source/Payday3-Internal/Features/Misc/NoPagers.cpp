#include "NoPagers.hpp"

#include <Windows.h>
#include <chrono>

namespace Cheat::NoPagers
{
	namespace
	{
		constexpr auto kScrubGap = std::chrono::milliseconds(100);

		static std::chrono::steady_clock::time_point s_timeNext{};

		static bool ActorOk(SDK::AActor* pActor)
		{
			return pActor && pActor->Class && !pActor->IsActorBeingDestroyed();
		}

		// After a real pager answer: enabled=false, enabledOnce=true, snatched=false, F = Pick Up.
		// The first build set snatched=true (in-progress answer) which blocked body carry.
		static bool ForcePickupModeSeh(SDK::ASBZAICharacter* pAI)
		{
			__try
			{
				if (!pAI || !pAI->Interactable)
					return false;
				if (!pAI->Interactable->IsA(SDK::USBZAICharacterInteractableComponent::StaticClass()))
					return false;

				auto* pInter = reinterpret_cast<SDK::USBZAICharacterInteractableComponent*>(pAI->Interactable);
				auto& modes = pInter->ModeArray;
				bool bHasPickup = false;
				for (int i = 0; i < modes.Num(); ++i)
				{
					if (modes[i] == SDK::ESBZAICharacterInteractableMode::AnswerPager)
						modes[i] = SDK::ESBZAICharacterInteractableMode::PickUp;
					if (modes[i] == SDK::ESBZAICharacterInteractableMode::PickUp)
						bHasPickup = true;
				}
				if (!bHasPickup)
					modes.Add(SDK::ESBZAICharacterInteractableMode::PickUp);

				pInter->ModeDataArray[static_cast<int>(SDK::ESBZAICharacterInteractableMode::PickUp)].Duration = 0.01f;
				pInter->bInteractionEnabled = true;
				return true;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				return false;
			}
		}

		static bool ScrubPagerSeh(SDK::ASBZAICharacter* pAI)
		{
			__try
			{
				if (!pAI)
					return false;

				pAI->bIsPagerEnabled = false;
				pAI->bIsPagerEnabledOnce = true;
				pAI->bIsPendingPagerEnabled = false;
				pAI->bIsPagerSnatched = false;
				pAI->PagerSnatchedInteractor = nullptr;
				pAI->CurrentAnswerPagerDialog = nullptr;
				pAI->bEnablePagerAfterOperatorTimeout = false;
				pAI->PagerTriggeredCount = 0;
				pAI->PagerData = nullptr;
				ForcePickupModeSeh(pAI);
				return true;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				return false;
			}
		}

		static bool NeedsScrubSeh(SDK::ASBZAICharacter* pAI, bool* pOut)
		{
			__try
			{
				if (!pAI || !pOut)
					return false;

				if (pAI->bIsPagerEnabled || pAI->bIsPendingPagerEnabled)
				{
					*pOut = true;
					return true;
				}

				if (pAI->AttributeSet && pAI->AttributeSet->Health.CurrentValue <= 0.01f)
				{
					*pOut = true;
					return true;
				}

				*pOut = false;
				return true;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				return false;
			}
		}
	}

	void OnPlayerControllerTick(
		SDK::UWorld* pGWorld,
		SDK::ASBZPlayerController* /*pLocalController*/,
		SDK::ASBZPlayerCharacter* pLocalPlayer)
	{
		if (!Cheat::g_bIsInGame)
			return;
		if (!pGWorld || !pLocalPlayer)
			return;

		const auto now = std::chrono::steady_clock::now();
		if (now < s_timeNext)
			return;
		s_timeNext = now + kScrubGap;

		SDK::UClass* pClass = SDK::ACH_BaseCop_C::StaticClass();
		if (!pClass)
			return;

		SDK::TArray<SDK::AActor*> list{};
		SDK::UGameplayStatics::GetAllActorsOfClass(pGWorld, pClass, &list);

		for (int i = 0; i < list.Num(); ++i)
		{
			auto* pActor = list[i];
			if (!ActorOk(pActor))
				continue;
			if (pActor->IsA(SDK::ASBZPlayerCharacter::StaticClass()))
				continue;
			if (pActor->IsA(SDK::ASBZAICrewCharacter::StaticClass()))
				continue;

			auto* pAI = reinterpret_cast<SDK::ASBZAICharacter*>(pActor);
			bool bNeed = false;
			if (!NeedsScrubSeh(pAI, &bNeed) || !bNeed)
				continue;

			ScrubPagerSeh(pAI);
		}
	}
}
