#include "AlwaysOnQoL.hpp"

#include <Windows.h>

namespace Cheat::AlwaysOnQoL
{
	namespace
	{
		static bool s_bSpaceWasDown = false;

		static bool IsZipTie(SDK::ASBZPlayerCharacter* pLocalPlayer)
		{
			if (!pLocalPlayer || !pLocalPlayer->Interactor || !pLocalPlayer->Interactor->CurrentInteraction)
				return false;
			return pLocalPlayer->Interactor->CurrentInteraction->IsA(
				SDK::USBZAICharacterInteractableComponent::StaticClass());
		}

		static bool IsUnmasked(SDK::ASBZPlayerCharacter* pLocalPlayer)
		{
			__try
			{
				if (pLocalPlayer->SBZPlayerState && !pLocalPlayer->SBZPlayerState->bIsMaskOn)
					return true;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
			}
			return false;
		}
	}

	void ApplyMovement(SDK::ASBZPlayerCharacter* pLocalPlayer)
	{
		if (!pLocalPlayer)
			return;

		__try
		{
			pLocalPlayer->FallingStartHeight = pLocalPlayer->K2_GetActorLocation().Z;
			if (pLocalPlayer->JumpMaxCount < 1)
				pLocalPlayer->JumpMaxCount = 1;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}

		const bool bSpace = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
		const bool bPressed = bSpace && !s_bSpaceWasDown;
		s_bSpaceWasDown = bSpace;

		if (!bPressed || !IsUnmasked(pLocalPlayer) || IsZipTie(pLocalPlayer))
			return;

		auto* pMove = pLocalPlayer->CharacterMovement;
		if (!pMove)
			return;
		if (pMove->MovementMode != SDK::EMovementMode::MOVE_Walking
			&& pMove->MovementMode != SDK::EMovementMode::MOVE_NavWalking)
			return;

		__try
		{
			float z = pMove->JumpZVelocity;
			if (z < 200.f)
				z = 420.f;
			pMove->MovementMode = SDK::EMovementMode::MOVE_Falling;
			pMove->Velocity.Z = z;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
	}
}
