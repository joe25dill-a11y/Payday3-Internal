#include "GrabAccess.hpp"
#include "../../Menu.hpp"

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <string>
#include <unordered_set>
#include <vector>

#undef min
#undef max

namespace Cheat::GrabAccess
{
    std::string g_sDebugStatus = "GrabAccess off";

    namespace
    {
        constexpr int kBatchSize = 8;
        constexpr auto kWaveGap = std::chrono::milliseconds(120);
        constexpr auto kRescanGap = std::chrono::milliseconds(1500);
        constexpr auto kDropSettle = std::chrono::milliseconds(250);

        static bool s_bBusy = false;
        static bool s_bWaitingDrop = false;
        static int s_iOk = 0;
        static int s_iTotal = 0;
        static size_t s_iIndex = 0;
        static bool s_bFriends = false;
        static std::vector<SDK::AActor*> s_vecQueue{};
        static std::unordered_set<uintptr_t> s_setDone{};
        static std::chrono::steady_clock::time_point s_timeNextBatch{};
        static std::chrono::steady_clock::time_point s_timeLastStart{};
        static std::chrono::steady_clock::time_point s_timeDropReady{};

        static std::string ToLower(std::string s)
        {
            for (char& c : s)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        }

        static std::string ActorName(SDK::AActor* pActor)
        {
            if (!pActor)
                return {};
            if (pActor->Class)
                return pActor->Class->GetName() + " " + pActor->GetName();
            return pActor->GetName();
        }

        static bool Contains(const std::string& hay, const char* needle)
        {
            return hay.find(needle) != std::string::npos;
        }

        static bool IsAccessName(const std::string& low)
        {
            return Contains(low, "keycard")
                || Contains(low, "rfid")
                || Contains(low, "carriedhackablekey")
                || Contains(low, "sbzcarriedhackablekey")
                || Contains(low, "pressbadge")
                || Contains(low, "press_badge");
        }

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

        static void SafeDropLootRaw(SDK::ASBZAICharacter* pCop)
        {
            __try
            {
                if (pCop)
                    pCop->Multicast_DropAttachedLoot();
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }

        static int DropAccessFromCops(SDK::UWorld* pGWorld)
        {
            int dropped = 0;
            SDK::TArray<SDK::AActor*> cops{};
            SDK::UGameplayStatics::GetAllActorsOfClass(pGWorld, SDK::ASBZAICharacter::StaticClass(), &cops);
            for (int i = 0; i < cops.Num(); ++i)
            {
                auto* pCop = reinterpret_cast<SDK::ASBZAICharacter*>(cops[i]);
                if (!ActorOk(pCop))
                    continue;
                SafeDropLootRaw(pCop);
                ++dropped;
            }
            return dropped;
        }

        // Num/ PickupAccessItem — real Server grant + ALWAYS Multicast (grant needs it).
        static bool PickupAccessProcessEvent(
            SDK::ASBZInteractionActor* pInterActor,
            SDK::USBZInteractorComponent* pInteractor)
        {
            __try
            {
                if (!pInterActor || !pInteractor)
                    return false;
                auto* pInter = pInterActor->Interactable;
                if (!pInter)
                    return false;

                pInter->SetInteractionEnabled(true);
                pInterActor->SetActorHiddenInGame(false);
                pInterActor->SetActorEnableCollision(true);

                const int32_t id = ++pInteractor->InteractId;
                pInteractor->Server_StartInteraction(pInter, id, 0);
                pInteractor->Server_CompleteInteraction(pInter, id);
                pInterActor->HandleServerComplete(pInter, pInteractor, true);

                // Always Multicast — friends skip left cards on ground with no grant (Lua bug)
                pInteractor->Multicast_CompletedInteraction(pInter, true);

                if (pInterActor->Class && !pInterActor->IsActorBeingDestroyed())
                    pInterActor->K2_DestroyActor();
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool PickupAccess(
            SDK::AActor* pActor,
            SDK::USBZInteractorComponent* pInteractor)
        {
            if (!ActorOk(pActor) || !pInteractor || AlreadyDone(pActor))
                return false;
            if (!IsAccessName(ToLower(ActorName(pActor))))
                return false;
            if (!pActor->IsA(SDK::ASBZInteractionActor::StaticClass()))
                return false;

            auto* pInterActor = reinterpret_cast<SDK::ASBZInteractionActor*>(pActor);
            if (!PickupAccessProcessEvent(pInterActor, pInteractor))
                return false;

            SetDone(pActor);
            return true;
        }

        static void CollectAccessTargets(SDK::UWorld* pGWorld, std::vector<SDK::AActor*>& out)
        {
            out.clear();
            std::unordered_set<uintptr_t> seen{};

            auto pushFiltered = [&](SDK::UClass* pClass, bool bNameFilter)
            {
                if (!pClass)
                    return;
                SDK::TArray<SDK::AActor*> list{};
                SDK::UGameplayStatics::GetAllActorsOfClass(pGWorld, pClass, &list);
                for (int i = 0; i < list.Num(); ++i)
                {
                    auto* pActor = list[i];
                    if (!ActorOk(pActor) || AlreadyDone(pActor))
                        continue;
                    if (bNameFilter && !IsAccessName(ToLower(ActorName(pActor))))
                        continue;
                    const uintptr_t key = reinterpret_cast<uintptr_t>(pActor);
                    if (!seen.insert(key).second)
                        continue;
                    out.push_back(pActor);
                }
            };

            pushFiltered(SDK::ASBZCarriedHackableKey::StaticClass(), false);
            if (auto* pKey = SDK::ABP_KeycardBase_C::StaticClass())
                pushFiltered(pKey, false);
            if (auto* pRfid = SDK::ABP_RFIDTagBase_C::StaticClass())
                pushFiltered(pRfid, false);
            // Rock the Cradle press badge
            if (auto* pBadge = SDK::ABP_Chus_CarriedPressBadge_C::StaticClass())
                pushFiltered(pBadge, false);
            pushFiltered(SDK::ASBZCarriedStaticInteractionActor::StaticClass(), true);
        }

        static void CancelGrab(const char* reason)
        {
            s_bBusy = false;
            s_bWaitingDrop = false;
            s_vecQueue.clear();
            g_sDebugStatus = reason ? reason : "GrabAccess off";
        }

        static void ClearDone()
        {
            s_setDone.clear();
        }

        static void StartGrab(SDK::UWorld* pGWorld, SDK::ASBZPlayerCharacter* pLocal)
        {
            if (s_bBusy || s_bWaitingDrop || !pLocal)
                return;

            const auto now = std::chrono::steady_clock::now();
            if (now - s_timeLastStart < kRescanGap)
                return;
            s_timeLastStart = now;
            s_bFriends = IsFriendsLobby(pGWorld, pLocal);

            const int dropped = DropAccessFromCops(pGWorld);
            s_bWaitingDrop = true;
            s_timeDropReady = now + kDropSettle;
            g_sDebugStatus = "GrabAccess drop cops≈" + std::to_string(dropped)
                + " — settle…";
        }

        static void BeginWaveAfterDrop(SDK::UWorld* pGWorld)
        {
            s_bWaitingDrop = false;
            CollectAccessTargets(pGWorld, s_vecQueue);
            s_iTotal = static_cast<int>(s_vecQueue.size());
            s_iOk = 0;
            s_iIndex = 0;
            s_timeNextBatch = std::chrono::steady_clock::now();

            if (s_vecQueue.empty())
            {
                g_sDebugStatus = "GrabAccess ON — no keys left";
                return;
            }

            s_bBusy = true;
            g_sDebugStatus = "GrabAccess keys=" + std::to_string(s_iTotal)
                + " (cards/RFID/badge)"
                + (s_bFriends ? " FRIENDS" : " SOLO");
        }

        static void ProcessBatch(SDK::ASBZPlayerCharacter* pLocal)
        {
            if (!s_bBusy || !pLocal)
                return;

            const auto now = std::chrono::steady_clock::now();
            if (now < s_timeNextBatch)
                return;

            auto* pInteractor = pLocal->Interactor;
            const int end = std::min(static_cast<int>(s_iIndex) + kBatchSize, s_iTotal);

            for (; static_cast<int>(s_iIndex) < end; ++s_iIndex)
            {
                if (s_iIndex >= s_vecQueue.size())
                    break;
                if (PickupAccess(s_vecQueue[s_iIndex], pInteractor))
                    ++s_iOk;
            }

            g_sDebugStatus = "GrabAccess " + std::to_string(s_iOk) + "/" + std::to_string(s_iTotal);

            if (static_cast<int>(s_iIndex) >= s_iTotal)
            {
                s_bBusy = false;
                g_sDebugStatus = "GrabAccess wave " + std::to_string(s_iOk) + "/" + std::to_string(s_iTotal)
                    + " — idle";
                return;
            }

            s_timeNextBatch = now + kWaveGap;
        }
    }

    void OnPlayerControllerTick(
        SDK::UWorld* pGWorld,
        SDK::ASBZPlayerController* /*pLocalController*/,
        SDK::ASBZPlayerCharacter* pLocalPlayer)
    {
        if (!CheatConfig::Get().m_misc.m_bGrabAccess)
        {
            if (s_bBusy || s_bWaitingDrop || !s_setDone.empty() || g_sDebugStatus != "GrabAccess off")
            {
                ClearDone();
                CancelGrab("GrabAccess off");
            }
            return;
        }

        if (!pGWorld || !pLocalPlayer)
            return;

        if (!Cheat::g_bIsInGame)
        {
            if (s_bBusy || s_bWaitingDrop)
                CancelGrab("GrabAccess paused (not in heist)");
            else
                g_sDebugStatus = "GrabAccess ON — wait for heist";
            ClearDone();
            return;
        }

        if (s_bWaitingDrop)
        {
            if (std::chrono::steady_clock::now() >= s_timeDropReady)
                BeginWaveAfterDrop(pGWorld);
            return;
        }

        StartGrab(pGWorld, pLocalPlayer);
        ProcessBatch(pLocalPlayer);
    }
}
