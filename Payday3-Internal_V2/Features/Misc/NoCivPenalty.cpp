#include "pch.h"
#include "NoCivPenalty.hpp"

#include <Windows.h>
#include <chrono>
#include <cstdio>
#include <string>

namespace NoCivPenalty
{
    std::string g_sStatus = "No Civ/Custody Penalty off";

    namespace
    {
        constexpr auto kScrubGap = std::chrono::milliseconds(250);

        static bool s_bWasOn = false;
        static int s_iScrubs = 0;
        static std::chrono::steady_clock::time_point s_timeNext{};

        static bool IsPenaltyType(SDK::ESBZRewardReductionType t)
        {
            return t == SDK::ESBZRewardReductionType::KillingCivilian
                || t == SDK::ESBZRewardReductionType::PlayerInCustody;
        }

        // Zero civ kill count + custody flag + cash reduction rows (no TArray remove — crashy).
        static bool ScrubPlayerResultSeh(SDK::FSBZPlayerEndMissionResultData* pRd)
        {
            __try
            {
                if (!pRd)
                    return false;

                bool b = false;

                if (pRd->HaveCivilianKilledCount != 0)
                {
                    pRd->HaveCivilianKilledCount = 0;
                    b = true;
                }

                if (pRd->bHasBeenInCustody)
                {
                    pRd->bHasBeenInCustody = false;
                    b = true;
                }

                auto& cash = pRd->CashRewardData;
                const int n = cash.CashRewardReductionData.Num();
                for (int i = 0; i < n; ++i)
                {
                    auto& row = cash.CashRewardReductionData[i];
                    if (!IsPenaltyType(row.ReductionType))
                        continue;

                    const int dock = row.ReductionCash;
                    if (dock > 0)
                    {
                        // Put the docked cash back into totals so the payout matches.
                        cash.TotalCashEarned += dock;
                        cash.TotalCashValue += dock;
                        cash.IndividualTotalCashValue += dock;
                        b = true;
                    }
                    if (row.Count != 0 || row.ReductionPercentage != 0 || row.ReductionCash != 0)
                    {
                        row.Count = 0;
                        row.ReductionPercentage = 0;
                        row.ReductionCash = 0;
                        b = true;
                    }
                }

                return b;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool ScrubCharResultSeh(SDK::FSBZCharacterEndMissionResultData* pRd)
        {
            __try
            {
                if (!pRd)
                    return false;
                if (pRd->HaveCivilianKilledCount == 0)
                    return false;
                pRd->HaveCivilianKilledCount = 0;
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool ScrubMissionResultSeh(SDK::FSBZEndMissionResultData* pData)
        {
            __try
            {
                if (!pData)
                    return false;
                bool b = false;
                pData->bAllCiviliansAlive = true;
                if (ScrubPlayerResultSeh(&pData->AllPlayerAIsResult))
                    b = true;
                if (ScrubCharResultSeh(&pData->AllOtherCharacterResult))
                    b = true;
                const int n = pData->PlayerResultArray.Num();
                for (int i = 0; i < n; ++i)
                {
                    if (ScrubPlayerResultSeh(&pData->PlayerResultArray[i]))
                        b = true;
                }
                return b;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool ScrubPlayerStateSeh(SDK::ASBZPlayerState* pPS)
        {
            __try
            {
                if (!pPS)
                    return false;
                bool b = ScrubPlayerResultSeh(&pPS->ResultData);
                if (pPS->CustodyCount != 0)
                {
                    pPS->CustodyCount = 0;
                    b = true;
                }
                return b;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static SDK::ASBZMissionState* MissionFromWorldSeh(SDK::UWorld* pWorld)
        {
            __try
            {
                if (!pWorld)
                    return nullptr;
                return SDK::ASBZMissionState::GetSBZMissionState(pWorld);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return nullptr;
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
                g_sStatus = "No Civ/Custody Penalty off";
            }
            return;
        }

        s_bWasOn = true;
        const auto now = std::chrono::steady_clock::now();
        if (now < s_timeNext)
            return;
        s_timeNext = now + kScrubGap;

        int n = 0;

        if (pLocalPlayer && pLocalPlayer->SBZPlayerState)
        {
            if (ScrubPlayerStateSeh(pLocalPlayer->SBZPlayerState))
                ++n;
        }

        if (pGWorld)
        {
            if (auto* cls = SDK::ASBZPlayerState::StaticClass())
            {
                SDK::TArray<SDK::AActor*> states{};
                SDK::UGameplayStatics::GetAllActorsOfClass(pGWorld, cls, &states);
                for (int i = 0; i < states.Num(); ++i)
                {
                    if (ScrubPlayerStateSeh(reinterpret_cast<SDK::ASBZPlayerState*>(states[i])))
                        ++n;
                }
            }

            if (auto* pMs = MissionFromWorldSeh(pGWorld))
            {
                if (ScrubMissionResultSeh(&pMs->CurrentMissionResultData))
                    ++n;
            }
        }

        if (n > 0)
            s_iScrubs += n;

        char buf[160]{};
        std::snprintf(buf, sizeof(buf),
            "ON — scrubbed=%d (civ kills + custody cash dock → 0; solo/host)",
            s_iScrubs);
        g_sStatus = buf;
    }
}
