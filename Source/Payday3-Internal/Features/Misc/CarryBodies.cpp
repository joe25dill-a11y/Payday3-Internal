#include "CarryBodies.hpp"
#include "../FNames.hpp"
#include "../../Menu.hpp"

#include <Windows.h>
#include <cfloat>
#include <chrono>
#include <string>
#include <vector>

namespace Cheat::CarryBodies
{
    std::string g_sStatus = "Carry More Bodies off";

    namespace
    {
        constexpr int32_t kMaxStash = 50;
        constexpr int kAiModeSlots = 11;
        constexpr auto kRescanGap = std::chrono::milliseconds(250);

        static std::vector<SDK::AActor*> s_stashedBodies;
        static SDK::ASBZPlayerCharacter* s_pLocalPlayer = nullptr;
        static std::chrono::steady_clock::time_point s_timeScan{};
        static int s_nStashOps = 0;
        static int s_nPickupRetry = 0;
        static int s_nReqBypass = 0;
        static bool s_bWasOn = false;

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

        static bool IsLocalPlayerSeh(SDK::ASBZCharacter* pChar)
        {
            return pChar && s_pLocalPlayer && pChar == s_pLocalPlayer;
        }

        static bool IsBodyActorSeh(SDK::AActor* pActor)
        {
            __try
            {
                return ActorOkSeh(pActor)
                    && pActor->IsA(SDK::ASBZAIBaseCharacter::StaticClass());
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static void PatchModeDataSeh(SDK::FSBZInteractableModeData& mode)
        {
            __try
            {
                mode.bIsEncumberedAllowed = true;
                mode.bIsAllowedInCasing = true;
                mode.bIsIllegal = false;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }

        static void BypassRequirementsSeh(SDK::USBZBaseInteractableComponent* pBase)
        {
            __try
            {
                if (!pBase)
                    return;
                pBase->Requirement = nullptr;
                pBase->NativeRequirement = nullptr;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }

        static void PatchInteractableSeh(SDK::USBZInteractableComponent* pInter)
        {
            __try
            {
                if (!pInter)
                    return;

                pInter->bIsEncumberedAllowed = 1;
                pInter->bIsAllowedInCasing = 1;
                pInter->bIsIllegal = 0;
                pInter->SetInteractionEnabled(true);
                pInter->SetLocalEnabled(true);

                BypassRequirementsSeh(pInter);

                auto& altModes = pInter->AlternativeModeData;
                for (int i = 0; i < altModes.Num(); ++i)
                    PatchModeDataSeh(altModes[i]);

                if (pInter->IsA(SDK::USBZAICharacterInteractableComponent::StaticClass()))
                {
                    auto* pAiInter = reinterpret_cast<SDK::USBZAICharacterInteractableComponent*>(pInter);
                    const int nModes = pAiInter->ModeArray.Num();
                    const int nPatch = nModes > 0 ? nModes : kAiModeSlots;
                    for (int i = 0; i < nPatch && i < kAiModeSlots; ++i)
                        PatchModeDataSeh(pAiInter->ModeDataArray[i]);
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }

        static void PatchCharacterInteractSeh(SDK::ASBZCharacter* pChar)
        {
            __try
            {
                if (!ActorOkSeh(pChar))
                    return;
                PatchInteractableSeh(reinterpret_cast<SDK::USBZInteractableComponent*>(pChar->Interactable));
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }

        static void RemoveEncumberedSeh(SDK::ASBZPlayerCharacter* pLocal)
        {
            __try
            {
                if (!pLocal || !pLocal->PlayerAbilitySystem)
                    return;
                pLocal->PlayerAbilitySystem->Multicast_RemoveEncumbered();
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }

        static SDK::USBZPlayerMovementWeightAsset* FindLightestWeightAssetSeh(
            SDK::USBZPlayerMovementComponent* pMove)
        {
            SDK::USBZPlayerMovementWeightAsset* pBest = nullptr;
            float flBestTier = FLT_MAX;

            __try
            {
                if (!pMove)
                    return nullptr;

                auto& assets = pMove->WeightAssetArray;
                for (int i = 0; i < assets.Num(); ++i)
                {
                    auto* pAsset = assets[i];
                    if (!pAsset || pAsset->TierWeight >= flBestTier)
                        continue;
                    flBestTier = pAsset->TierWeight;
                    pBest = pAsset;
                }

                if (!pBest)
                    pBest = pMove->WeightAsset;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return nullptr;
            }

            return pBest;
        }

        static void ClearCarryWeightSeh(SDK::ASBZPlayerCharacter* pLocal)
        {
            __try
            {
                if (!pLocal)
                    return;

                RemoveEncumberedSeh(pLocal);

                if (pLocal->PlayerAttributeSet)
                {
                    auto& tier = pLocal->PlayerAttributeSet->WeightTierOffset;
                    tier.BaseValue = 0.f;
                    tier.CurrentValue = 0.f;
                }

                auto* pMove = reinterpret_cast<SDK::USBZPlayerMovementComponent*>(pLocal->CharacterMovement);
                if (!pMove || !pMove->IsA(SDK::USBZPlayerMovementComponent::StaticClass()))
                    return;

                pMove->WeightAssetOverride = nullptr;

                if (auto* pLight = FindLightestWeightAssetSeh(pMove))
                {
                    pMove->WeightTierAsset = pLight;
                    pMove->WeightAsset = pLight;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }

        static void AttachBodyToBackSeh(SDK::ASBZPlayerCharacter* pPlayer, SDK::AActor* pBody)
        {
            __try
            {
                if (!pPlayer || !ActorOkSeh(pBody))
                    return;

                auto* pMesh = pPlayer->Mesh;
                auto* pBodyChar = reinterpret_cast<SDK::ASBZCharacter*>(pBody);
                auto* pBodyMesh = pBodyChar ? pBodyChar->Mesh : nullptr;
                if (!pMesh || !pBodyMesh)
                    return;

                const float zOff = static_cast<float>(s_stashedBodies.size()) * 18.f;

                pBody->K2_AttachToComponent(
                    pMesh,
                    pPlayer->CarryActorSocketName,
                    SDK::EAttachmentRule::KeepWorld,
                    SDK::EAttachmentRule::KeepWorld,
                    SDK::EAttachmentRule::KeepWorld,
                    true);

                SDK::FVector loc{ 0.f, -35.f, zOff };
                SDK::FRotator rot{ 0.f, 90.f, 0.f };
                pBody->K2_SetActorRelativeLocation(loc, false, nullptr, true);
                pBody->K2_SetActorRelativeRotation(rot, false, nullptr, true);
                pBodyMesh->SetVisibility(true, true);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }

        static void StashCurrentCarrySeh(SDK::ASBZPlayerCharacter* pPlayer)
        {
            __try
            {
                if (!pPlayer)
                    return;

                SDK::AActor* pCarry = pPlayer->CurrentCarryActor;
                if (!IsBodyActorSeh(pCarry))
                    return;
                if (static_cast<int>(s_stashedBodies.size()) >= kMaxStash)
                    return;

                AttachBodyToBackSeh(pPlayer, pCarry);
                s_stashedBodies.push_back(pCarry);
                ++s_nStashOps;

                pPlayer->CurrentCarryActor = nullptr;
                pPlayer->CurrentCarryNetID = 0;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }

        static void ReleaseStashedBodiesSeh()
        {
            __try
            {
                for (SDK::AActor* pBody : s_stashedBodies)
                {
                    if (!ActorOkSeh(pBody))
                        continue;
                    pBody->K2_DetachFromActor(
                        SDK::EDetachmentRule::KeepWorld,
                        SDK::EDetachmentRule::KeepWorld,
                        SDK::EDetachmentRule::KeepWorld);
                }
                s_stashedBodies.clear();
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }

        static void ScanAiInteractablesSeh(SDK::UWorld* pGWorld)
        {
            __try
            {
                if (!pGWorld)
                    return;

                auto* cls = SDK::ASBZAIBaseCharacter::StaticClass();
                if (!cls)
                    return;

                SDK::TArray<SDK::AActor*> actors{};
                SDK::UGameplayStatics::GetAllActorsOfClass(pGWorld, cls, &actors);
                for (int i = 0; i < actors.Num(); ++i)
                    PatchCharacterInteractSeh(reinterpret_cast<SDK::ASBZCharacter*>(actors[i]));
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }
    }

    void OnPlayerControllerTick(
        SDK::UWorld* pGWorld,
        SDK::ASBZPlayerController* /*pLocalController*/,
        SDK::ASBZPlayerCharacter* pLocalPlayer)
    {
        const bool bOn = CheatConfig::Get().m_misc.m_bCarryMoreBodies;
        if (!bOn)
        {
            if (s_bWasOn)
            {
                s_bWasOn = false;
                s_stashedBodies.clear();
                g_sStatus = "Carry More Bodies off";
            }
            return;
        }

        s_bWasOn = true;
        s_pLocalPlayer = pLocalPlayer;
        if (!pGWorld || !ActorOkSeh(pLocalPlayer))
        {
            g_sStatus = "Carry More Bodies ON — wait for heist";
            return;
        }

        ClearCarryWeightSeh(pLocalPlayer);

        const auto now = std::chrono::steady_clock::now();
        if (now - s_timeScan >= kRescanGap)
        {
            s_timeScan = now;
            ScanAiInteractablesSeh(pGWorld);
        }

        const int arms = pLocalPlayer->CurrentCarryActor ? 1 : 0;
        const int back = static_cast<int>(s_stashedBodies.size());
        g_sStatus = "arms=" + std::to_string(arms)
            + " back=" + std::to_string(back)
            + " total=" + std::to_string(arms + back)
            + " stash=" + std::to_string(s_nStashOps)
            + " retry=" + std::to_string(s_nPickupRetry)
            + " req=" + std::to_string(s_nReqBypass);
    }

    void OnThrowCarryPostHook(SDK::UObject* pObject)
    {
        __try
        {
            if (!IsLocalPlayerSeh(reinterpret_cast<SDK::ASBZCharacter*>(pObject)))
                return;
            ReleaseStashedBodiesSeh();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }

    void OnPickupFailedPostHook(SDK::UObject* pObject, void* pParams)
    {
        __try
        {
            auto* pChar = reinterpret_cast<SDK::ASBZCharacter*>(pObject);
            if (!IsLocalPlayerSeh(pChar))
                return;

            struct PickupFailedParms final
            {
                SDK::uint32 NetID;
            };
            auto& params = *reinterpret_cast<PickupFailedParms*>(pParams);
            StashCurrentCarrySeh(reinterpret_cast<SDK::ASBZPlayerCharacter*>(pChar));
            ++s_nPickupRetry;
            pChar->Multicast_OnPickupCarryActor(params.NetID);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }

    bool TryProcessEventHook(const SDK::UObject* pObject, SDK::UFunction* pFunction, void* /*pParams*/)
    {
        if (!CheatConfig::Get().m_misc.m_bCarryMoreBodies || !pFunction)
            return false;

        const auto fnName = pFunction->Name;

        if (fnName == FNames::BP_CheckRequirement)
        {
            __try
            {
                RemoveEncumberedSeh(s_pLocalPlayer);
                ++s_nReqBypass;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
            return false;
        }

        if (fnName == FNames::Multicast_OnPickupCarryActor)
        {
            __try
            {
                auto* pChar = reinterpret_cast<SDK::ASBZCharacter*>(const_cast<SDK::UObject*>(pObject));
                if (IsLocalPlayerSeh(pChar) && pChar->CurrentCarryActor)
                    StashCurrentCarrySeh(reinterpret_cast<SDK::ASBZPlayerCharacter*>(pChar));
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
            return false;
        }

        return false;
    }

    bool IsThrowCarryEvent(SDK::UFunction* pFunction)
    {
        return pFunction && pFunction->Name == FNames::Multicast_OnThrowCarryActor;
    }

    bool IsPickupFailedEvent(SDK::UFunction* pFunction)
    {
        return pFunction && pFunction->Name == FNames::Client_OnPickupCarryActorFailed;
    }
}
