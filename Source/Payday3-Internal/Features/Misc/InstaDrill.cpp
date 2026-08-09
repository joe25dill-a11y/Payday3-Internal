#include "InstaDrill.hpp"
#include "../../Menu.hpp"

#include <Windows.h>
#include <chrono>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace Cheat::InstaDrill
{
    std::string g_sDebugStatus = "InstaDrill off";

    namespace
    {
        constexpr auto kPollGap = std::chrono::milliseconds(400);

        static std::unordered_set<uintptr_t> s_setDone{};
        static std::unordered_map<uintptr_t, std::chrono::steady_clock::time_point> s_mapCleanerCd{};
        static std::chrono::steady_clock::time_point s_timeNextPoll{};
        static int s_iFinished = 0;
        static int s_iCleanerHits = 0;
        static bool s_bFriends = false;

        static bool ActorOk(SDK::AActor* pActor)
        {
            return pActor && pActor->Class && !pActor->IsActorBeingDestroyed();
        }

        static bool AlreadyDone(SDK::AActor* pActor)
        {
            return pActor && s_setDone.contains(reinterpret_cast<uintptr_t>(pActor));
        }

        static void SetDone(SDK::AActor* pActor)
        {
            if (pActor)
                s_setDone.insert(reinterpret_cast<uintptr_t>(pActor));
        }

        static bool IsFriendsLobby(SDK::UWorld* pGWorld, SDK::ASBZPlayerCharacter* pLocal)
        {
            if (!pGWorld)
                return false;
            SDK::TArray<SDK::AActor*> players{};
            SDK::UGameplayStatics::GetAllActorsOfClass(pGWorld, SDK::ASBZPlayerCharacter::StaticClass(), &players);
            int n = 0;
            for (int i = 0; i < players.Num(); ++i)
            {
                auto* p = reinterpret_cast<SDK::ASBZPlayerCharacter*>(players[i]);
                if (!ActorOk(p) || p == pLocal)
                    continue;
                if (p->IsLocallyControlled())
                    continue;
                ++n;
            }
            return n > 0;
        }

        static bool FinishDrillExRaw(SDK::ASBZDrillEx* pDrill)
        {
            __try
            {
                if (!pDrill || !pDrill->Class || pDrill->IsActorBeingDestroyed())
                    return false;

                const auto state = pDrill->Data.State;
                if (state == SDK::ESBZDrillState::Done)
                    return false;
                if (state != SDK::ESBZDrillState::Drilling && state != SDK::ESBZDrillState::Jammed)
                    return false;

                if (state == SDK::ESBZDrillState::Jammed)
                    pDrill->UnjamDrill();

                pDrill->Duration = 0.05f;
                pDrill->Data.ProgressLast = 1.f;
                pDrill->Data.ProgressPerSec = 50.f;
                pDrill->Data.HeatPerSec = 0.f;
                pDrill->Data.State = SDK::ESBZDrillState::Done;
                pDrill->OnStateChanged(SDK::ESBZDrillState::Done);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool FinishDrillLegacyRaw(SDK::ASBZDrill* pDrill, bool bFriends)
        {
            __try
            {
                if (!pDrill || !pDrill->Class || pDrill->IsActorBeingDestroyed())
                    return false;

                const auto state = pDrill->State;
                if (state == SDK::ESBZDrillState::Done)
                    return false;
                if (state != SDK::ESBZDrillState::Drilling && state != SDK::ESBZDrillState::Jammed)
                    return false;

                if (state == SDK::ESBZDrillState::Jammed)
                    pDrill->UnjamDrill();

                pDrill->Duration = 0.05f;
                pDrill->TimeLeft = 0.f;
                pDrill->EndTime = 0.f;
                pDrill->State = SDK::ESBZDrillState::Done;
                if (!bFriends)
                    pDrill->Multicast_StopDrill(SDK::ESBZDrillState::Done, 0.f);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool FinishBreachRaw(SDK::ASBZBreachingEquipmentBase* pEq, bool bFriends)
        {
            __try
            {
                if (!pEq || !pEq->Class || pEq->IsActorBeingDestroyed())
                    return false;

                const auto st = pEq->GetCurrentState();
                if (st == SDK::ESBZBreachingEquipmentState::Done)
                    return false;

                const bool bActive =
                    st == SDK::ESBZBreachingEquipmentState::HeatingUp
                    || st == SDK::ESBZBreachingEquipmentState::Running
                    || st == SDK::ESBZBreachingEquipmentState::Jammed
                    || st == SDK::ESBZBreachingEquipmentState::NeedsAdjusting;
                const bool bHasTimer = pEq->EstimatedDurationLeft > 0.05f;
                if (!bActive && !bHasTimer)
                    return false;

                pEq->DurationTimeSeconds = 0.05f;
                pEq->EstimatedDurationLeft = 0.f;

                // Thermal lance / heavy: pin fuel times like Num8
                if (pEq->IsA(SDK::ASBZHeavyBreachingEquipmentBase::StaticClass()))
                {
                    auto* pHeavy = reinterpret_cast<SDK::ASBZHeavyBreachingEquipmentBase*>(pEq);
                    pHeavy->HeatupTimeSeconds = 0.05f;
                    pHeavy->EstimatedFuelEndTime = 0.f;
                    pHeavy->EquipmentTimes.RedZoneTimeSeconds = 0.05f;
                    pHeavy->EquipmentTimes.YellowZoneTimeSeconds = 0.05f;
                    pHeavy->EquipmentTimes.GreenZoneTimeSeconds = 0.05f;
                    pHeavy->EquipmentTimes.TotalFuelTime = 0.05f;
                }

                pEq->SetState(SDK::ESBZBreachingEquipmentState::Done, true);
                if (!bFriends)
                {
                    pEq->Multicast_SetState(SDK::ESBZBreachingEquipmentState::Done);
                    pEq->Multicast_SetEstimatedDurationLeft(0.f);
                }
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        // Num8 FinishThermite — Multicast_SetState is what actually finishes burn.
        static bool FinishThermiteRaw(SDK::ASBZThermite* pTherm, bool bFriends)
        {
            __try
            {
                if (!pTherm || !pTherm->Class || pTherm->IsActorBeingDestroyed())
                    return false;

                const auto st = pTherm->CurrentBurnState;
                if (st == SDK::ESBZThermiteBurnState::Completed)
                    return false;

                const bool bBurning =
                    st == SDK::ESBZThermiteBurnState::Burning
                    || st == SDK::ESBZThermiteBurnState::CriticalBurning
                    || st == SDK::ESBZThermiteBurnState::FlashOver
                    || st == SDK::ESBZThermiteBurnState::Unlit;
                if (!bBurning)
                    return false;

                pTherm->BurnDuration = 0.05f;
                pTherm->ElapsedBurnTime = 999.f;
                pTherm->CriticalBurnTimeLeft = 0.f;
                pTherm->BurnModifier = 100.f;
                pTherm->ExplosionChance = 0.f;
                pTherm->CurrentBurnState = SDK::ESBZThermiteBurnState::Completed;

                if (!bFriends)
                    pTherm->Multicast_SetState(SDK::ESBZThermiteBurnState::Completed);
                else
                    pTherm->BP_OnStateChanged(st, SDK::ESBZThermiteBurnState::Completed, true);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        // Num8 Dirty Ice / cash cleaner — pin duration + Multicast ProcessedBag while Running/Paused.
        // Do NOT permanent-done: machine processes bags one-by-one.
        static bool FinishLootProcessorRaw(SDK::ASBZLootProcessorBase* pLp, bool bFriends)
        {
            __try
            {
                if (!pLp || !pLp->Class || pLp->IsActorBeingDestroyed())
                    return false;

                pLp->ProcessDuration = 0.01f;
                pLp->PreplanningProcessDuration = 0.01f;

                const auto st = pLp->CurrentState;
                if (st != SDK::ESBZLootProcessorState::Running
                    && st != SDK::ESBZLootProcessorState::Paused)
                    return false;

                if (!bFriends)
                {
                    pLp->Multicast_SetState(SDK::ESBZLootProcessorState::ProcessedBag);
                    return true;
                }

                pLp->ResumeProcessing();
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool FinishDrillEx(SDK::ASBZDrillEx* p)
        {
            if (!ActorOk(p) || AlreadyDone(p))
                return false;
            if (!FinishDrillExRaw(p))
                return false;
            SetDone(p);
            return true;
        }

        static bool FinishDrillLegacy(SDK::ASBZDrill* p, bool bFriends)
        {
            if (!ActorOk(p) || AlreadyDone(p))
                return false;
            if (!FinishDrillLegacyRaw(p, bFriends))
                return false;
            SetDone(p);
            return true;
        }

        static bool FinishBreach(SDK::ASBZBreachingEquipmentBase* p, bool bFriends)
        {
            if (!ActorOk(p) || AlreadyDone(p))
                return false;
            if (!FinishBreachRaw(p, bFriends))
                return false;
            SetDone(p);
            return true;
        }

        static bool FinishThermite(SDK::ASBZThermite* p, bool bFriends)
        {
            if (!ActorOk(p) || AlreadyDone(p))
                return false;
            if (!FinishThermiteRaw(p, bFriends))
                return false;
            SetDone(p);
            return true;
        }

        static bool FinishCleaner(SDK::ASBZLootProcessorBase* pLp, bool bFriends)
        {
            if (!ActorOk(pLp))
                return false;

            const uintptr_t key = reinterpret_cast<uintptr_t>(pLp)
                ^ (static_cast<uintptr_t>(static_cast<uint8_t>(pLp->CurrentProcessingIndex)) << 48)
                ^ (static_cast<uintptr_t>(pLp->BagCount) << 32);

            const auto now = std::chrono::steady_clock::now();
            const auto it = s_mapCleanerCd.find(key);
            if (it != s_mapCleanerCd.end() && now - it->second < std::chrono::milliseconds(600))
                return false;

            if (!FinishLootProcessorRaw(pLp, bFriends))
                return false;

            s_mapCleanerCd[key] = now;
            return true;
        }

        // Num8 FinishHackable — only while Hacking; ALWAYS Multicast (don't unlock idle PCs).
        static bool FinishHackableRaw(SDK::ASBZHackableActor* pH, bool* pbMarkDone)
        {
            __try
            {
                if (pbMarkDone)
                    *pbMarkDone = false;
                if (!pH || !pH->Class || pH->IsActorBeingDestroyed())
                    return false;

                pH->StealthDurationSeconds = 0.05f;
                pH->LoudDurationSeconds = 0.05f;
                pH->DurationSeconds = 0.05f;

                const auto st = pH->ReplicatedData.CurrentState;
                if (st == SDK::ESBZHackableActorState::Unlocked
                    || st == SDK::ESBZHackableActorState::GainedAccess)
                {
                    if (pbMarkDone)
                        *pbMarkDone = true;
                    return false;
                }
                if (st != SDK::ESBZHackableActorState::Hacking)
                    return false;

                pH->Multicast_SetState(SDK::ESBZHackableActorState::Unlocked);
                pH->Multicast_SetState(SDK::ESBZHackableActorState::GainedAccess);
                pH->ReplicatedData.CurrentState = SDK::ESBZHackableActorState::GainedAccess;
                pH->BP_OnStateChanged(st, SDK::ESBZHackableActorState::GainedAccess);
                pH->BP_OnUnlocked();
                pH->BP_GainedAccess();
                pH->BP_UpdateProgressBar(100);

                const auto after = pH->ReplicatedData.CurrentState;
                if (pbMarkDone
                    && (after == SDK::ESBZHackableActorState::Unlocked
                        || after == SDK::ESBZHackableActorState::GainedAccess))
                    *pbMarkDone = true;
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool FinishHackable(SDK::ASBZHackableActor* pH)
        {
            if (!ActorOk(pH) || AlreadyDone(pH))
                return false;
            bool bMark = false;
            const bool bOk = FinishHackableRaw(pH, &bMark);
            if (bMark)
                SetDone(pH);
            return bOk;
        }

        // Num8 InstantLocalMiniGame — lockpick / computer UI while active.
        static bool InstantMiniGameRaw(SDK::ASBZPlayerState* pPS)
        {
            __try
            {
                if (!pPS)
                    return false;
                const auto st = pPS->MiniGameState;
                if (st == SDK::EPD3MiniGameState::None
                    || st == SDK::EPD3MiniGameState::Success)
                    return false;
                if (st != SDK::EPD3MiniGameState::Initiated
                    && st != SDK::EPD3MiniGameState::InProgress)
                    return false;

                pPS->Server_SetMiniGameState(SDK::EPD3MiniGameState::Success);
                pPS->Multicast_SetMiniGameState(SDK::EPD3MiniGameState::Success, pPS);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static int ProcessCleaners(SDK::UWorld* pGWorld, bool bFriends)
        {
            int n = 0;
            SDK::TArray<SDK::AActor*> list{};
            SDK::UGameplayStatics::GetAllActorsOfClass(
                pGWorld, SDK::ASBZLootProcessorBase::StaticClass(), &list);
            for (int i = 0; i < list.Num(); ++i)
            {
                if (FinishCleaner(reinterpret_cast<SDK::ASBZLootProcessorBase*>(list[i]), bFriends))
                    ++n;
            }
            return n;
        }

        static int ProcessHackables(SDK::UWorld* pGWorld)
        {
            int n = 0;
            SDK::TArray<SDK::AActor*> list{};
            SDK::UGameplayStatics::GetAllActorsOfClass(
                pGWorld, SDK::ASBZHackableActor::StaticClass(), &list);
            for (int i = 0; i < list.Num(); ++i)
            {
                if (FinishHackable(reinterpret_cast<SDK::ASBZHackableActor*>(list[i])))
                    ++n;
            }
            return n;
        }

        static int ProcessDrills(SDK::UWorld* pGWorld, bool bFriends)
        {
            int n = 0;

            {
                SDK::TArray<SDK::AActor*> list{};
                SDK::UGameplayStatics::GetAllActorsOfClass(pGWorld, SDK::ASBZDrillEx::StaticClass(), &list);
                for (int i = 0; i < list.Num(); ++i)
                {
                    if (FinishDrillEx(reinterpret_cast<SDK::ASBZDrillEx*>(list[i])))
                        ++n;
                }
            }
            {
                SDK::TArray<SDK::AActor*> list{};
                SDK::UGameplayStatics::GetAllActorsOfClass(pGWorld, SDK::ASBZDrill::StaticClass(), &list);
                for (int i = 0; i < list.Num(); ++i)
                {
                    if (FinishDrillLegacy(reinterpret_cast<SDK::ASBZDrill*>(list[i]), bFriends))
                        ++n;
                }
            }
            {
                SDK::TArray<SDK::AActor*> list{};
                SDK::UGameplayStatics::GetAllActorsOfClass(pGWorld, SDK::ASBZPocketDrill::StaticClass(), &list);
                for (int i = 0; i < list.Num(); ++i)
                {
                    if (FinishBreach(reinterpret_cast<SDK::ASBZBreachingEquipmentBase*>(list[i]), bFriends))
                        ++n;
                }
            }
            {
                SDK::TArray<SDK::AActor*> list{};
                SDK::UGameplayStatics::GetAllActorsOfClass(pGWorld, SDK::ASBZThermalLance::StaticClass(), &list);
                for (int i = 0; i < list.Num(); ++i)
                {
                    if (FinishBreach(reinterpret_cast<SDK::ASBZBreachingEquipmentBase*>(list[i]), bFriends))
                        ++n;
                }
            }
            {
                SDK::TArray<SDK::AActor*> list{};
                SDK::UGameplayStatics::GetAllActorsOfClass(pGWorld, SDK::ASBZThermite::StaticClass(), &list);
                for (int i = 0; i < list.Num(); ++i)
                {
                    if (FinishThermite(reinterpret_cast<SDK::ASBZThermite*>(list[i]), bFriends))
                        ++n;
                }
            }

            return n;
        }
    }

    void OnPlayerControllerTick(
        SDK::UWorld* pGWorld,
        SDK::ASBZPlayerController* pLocalController,
        SDK::ASBZPlayerCharacter* pLocalPlayer)
    {
        if (!CheatConfig::Get().m_misc.m_bInstaDrill)
        {
            if (!s_setDone.empty() || !s_mapCleanerCd.empty() || g_sDebugStatus != "InstaDrill off")
            {
                s_setDone.clear();
                s_mapCleanerCd.clear();
                s_iFinished = 0;
                s_iCleanerHits = 0;
                g_sDebugStatus = "InstaDrill off";
            }
            return;
        }

        if (!pGWorld || !pLocalPlayer)
            return;

        if (!Cheat::g_bIsInGame)
        {
            s_setDone.clear();
            s_mapCleanerCd.clear();
            g_sDebugStatus = "InstaDrill ON — wait for heist";
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now < s_timeNextPoll)
            return;
        s_timeNextPoll = now + kPollGap;

        s_bFriends = IsFriendsLobby(pGWorld, pLocalPlayer);

        // Num8: minigame UI, active computers, cleaners, then drills/thermite/lance
        SDK::ASBZPlayerState* pPS = pLocalPlayer->SBZPlayerState;
        if (!pPS && pLocalController)
            pPS = reinterpret_cast<SDK::ASBZPlayerState*>(pLocalController->PlayerState);
        const bool bMini = InstantMiniGameRaw(pPS);
        const int nHack = ProcessHackables(pGWorld);
        const int nClean = ProcessCleaners(pGWorld, s_bFriends);
        const int nDrill = ProcessDrills(pGWorld, s_bFriends);
        if (nClean > 0)
            s_iCleanerHits += nClean;
        if (nDrill + nHack > 0)
            s_iFinished += nDrill + nHack;

        const int nPulse = nClean + nDrill + nHack + (bMini ? 1 : 0);
        g_sDebugStatus = "InstaDrill drills/pc=" + std::to_string(s_iFinished)
            + " cleaner=" + std::to_string(s_iCleanerHits)
            + (nPulse > 0 ? (" (+" + std::to_string(nPulse) + ")") : "")
            + (s_bFriends ? " FRIENDS" : " SOLO");
    }
}
