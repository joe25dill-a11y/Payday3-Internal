#include "pch.h"
#include "ClientMove.hpp"

#undef min
#undef max

namespace FreecamFly
{
	namespace
	{
		bool s_bActive = false;
		bool s_bCollisionOff = false;

		SDK::USBZPlayerMovementComponent* GetMove(SDK::ASBZPlayerCharacter* pLocal)
		{
			if (!pLocal || !IsValidObjectPtr(pLocal))
				return nullptr;
			auto* pMove = reinterpret_cast<SDK::USBZPlayerMovementComponent*>(pLocal->CharacterMovement);
			if (!pMove || !IsValidObjectPtr(pMove))
				return nullptr;
			if (!pMove->IsA(SDK::UCharacterMovementComponent::StaticClass()))
				return nullptr;
			return pMove;
		}

		SDK::FVector FlyInput(SDK::ASBZPlayerCharacter* pLocal, SDK::USBZPlayerMovementComponent* pMove)
		{
			SDK::FVector v{};
			if (pLocal)
			{
				// APawn::LastControlInputVector @ 0x320 — avoid ProcessEvent GetLastInputVector
				v = *reinterpret_cast<SDK::FVector*>(reinterpret_cast<uintptr_t>(pLocal) + 0x320);
			}
			if (v.X == 0.f && v.Y == 0.f && v.Z == 0.f && pMove)
				v = pMove->Acceleration;
			return v;
		}

		void SetCollisionFlag(SDK::ASBZPlayerCharacter* pLocal, bool bEnable)
		{
			if (!pLocal)
				return;
			// AActor::bActorEnableCollision @ 0x64 bit 7 — avoid ProcessEvent (UE4SS races)
			auto* pByte = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(pLocal) + 0x64);
			if (bEnable)
				*pByte |= (1u << 7);
			else
				*pByte &= static_cast<uint8_t>(~(1u << 7));
		}

		void ApplyFreecam(SDK::ASBZPlayerCharacter* pLocal, SDK::USBZPlayerMovementComponent* pMove, bool bOn, bool bFaster, float flSpeed)
		{
			if (!pLocal || !pMove)
				return;

			if (bOn)
			{
				if (!s_bCollisionOff)
				{
					SetCollisionFlag(pLocal, false);
					s_bCollisionOff = true;
				}

				pMove->MovementMode = SDK::EMovementMode::MOVE_Flying;
				pMove->BrakingDecelerationFlying = 10000.f;
				pMove->MaxFlySpeed = flSpeed > 1.f ? flSpeed : 2500.f;

				float speed = pMove->MaxFlySpeed;
				if (bFaster)
					speed *= 2.f;

				pMove->Velocity = FlyInput(pLocal, pMove) * speed;
			}
			else
			{
				if (s_bCollisionOff)
				{
					SetCollisionFlag(pLocal, true);
					s_bCollisionOff = false;
				}
				pMove->MovementMode = SDK::EMovementMode::MOVE_Walking;
				pMove->Velocity = SDK::FVector{};
			}
		}

		void ApplyToLocal(bool bOn, bool bFaster, float flSpeed)
		{
			__try
			{
				auto* local = Unreal::GetLocalCharacter();
				if (!local || !IsValidObjectPtr(local) || local->IsActorBeingDestroyed())
					return;
				auto* move = GetMove(local);
				if (!move)
					return;
				ApplyFreecam(local, move, bOn, bFaster, flSpeed);
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
			}
		}
	}

	void Tick(bool bFeatureEnabled, bool bFlying, bool bFasterHeld, float flFlySpeed)
	{
		const bool bOn = bFeatureEnabled && bFlying;
		if (!bOn && !s_bActive)
			return;

		s_bActive = bOn;
		ApplyToLocal(bOn, bFasterHeld, flFlySpeed);
	}
}
