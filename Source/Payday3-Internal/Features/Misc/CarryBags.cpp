#include "CarryBags.hpp"
#include "../../Menu.hpp"

#include <Windows.h>
#include <cfloat>
#include <chrono>
#include <string>

namespace Cheat::CarryBags
{
    std::string g_sStatus = "Carry More Bags off";

    namespace
    {
        constexpr int32_t kCarryCap = 50;
        constexpr auto kRescanGap = std::chrono::milliseconds(1500);

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
    }

    void OnPlayerControllerTick(
        SDK::UWorld* pGWorld,
        SDK::ASBZPlayerController* /*pLocalController*/,
        SDK::ASBZPlayerCharacter* pLocalPlayer)
    {
        const bool bOn = CheatConfig::Get().m_misc.m_bCarryMoreBags;
        if (!bOn)
        {
            if (s_bWasOn)
            {
                s_bWasOn = false;
                g_sStatus = "Carry More Bags off";
            }
            return;
        }

        s_bWasOn = true;
        if (!pGWorld || !ActorOkSeh(pLocalPlayer))
        {
            g_sStatus = "Carry More Bags ON — wait for heist";
            return;
        }

        ClearCarryWeightSeh(pLocalPlayer);

        const auto now = std::chrono::steady_clock::now();
        const bool bDoScan = now - s_timeScan >= kRescanGap
            || g_sStatus.find("you=") == std::string::npos;
        if (!bDoScan)
            return;
        s_timeScan = now;

        int you = 0;
        int ai = 0;

        if (BumpCarrySeh(pLocalPlayer))
            you = 1;

        if (auto* cls = SDK::ASBZAICrewCharacter::StaticClass())
        {
            SDK::TArray<SDK::AActor*> crew{};
            SDK::UGameplayStatics::GetAllActorsOfClass(pGWorld, cls, &crew);
            for (int i = 0; i < crew.Num(); ++i)
            {
                auto* pCrew = reinterpret_cast<SDK::ASBZAICrewCharacter*>(crew[i]);
                if (!ActorOkSeh(pCrew))
                    continue;
                if (BumpCarrySeh(pCrew))
                    ++ai;
            }
        }

        g_sStatus = "Carry More Bags ON (cap " + std::to_string(kCarryCap)
            + ") you=" + std::to_string(you)
            + " AI=" + std::to_string(ai)
            + " no-weight=on";
    }
}
