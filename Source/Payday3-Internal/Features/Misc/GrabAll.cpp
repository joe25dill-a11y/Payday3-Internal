#include "GrabAll.hpp"
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

namespace Cheat::GrabAll
{
    std::string g_sDebugStatus = "GrabAll off";

    namespace
    {
        // DLL = Claim/F only (no CreateBag — that mints TAB bags while piles stay).
        // Num7 Lua still covers CreateBag leftovers.
        // v2: floor bags first (already), harden live-Claim, friends pacing, one retry pass.
        enum class EKind : uint8_t
        {
            Pile,
            Bag,
            Instant
        };

        struct Target_t
        {
            EKind m_eKind = EKind::Pile;
            SDK::AActor* m_pActor = nullptr;
        };

        constexpr int kBatchSolo = 12;
        constexpr int kBatchFriends = 8;
        constexpr auto kWaveGapSolo = std::chrono::milliseconds(400);
        constexpr auto kWaveGapFriends = std::chrono::milliseconds(700);
        constexpr auto kRetryGap = std::chrono::milliseconds(400);
        constexpr auto kRescanGap = std::chrono::milliseconds(1500);

        static bool s_bBusy = false;
        static uint32_t s_uGen = 0;
        static int s_iOk = 0;
        static int s_iTotal = 0;
        static size_t s_iIndex = 0;
        static bool s_bFriends = false;
        static bool s_bRetryPassDone = false;
        static std::vector<Target_t> s_vecQueue{};
        static std::unordered_set<uintptr_t> s_setDone{};
        static std::chrono::steady_clock::time_point s_timeNextBatch{};
        static std::chrono::steady_clock::time_point s_timeLastStart{};

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
            // Prefer class name — instance names can be UAID junk without "money"
            if (pActor->Class)
            {
                const std::string cn = pActor->Class->GetName();
                if (!cn.empty())
                    return cn + " " + pActor->GetName();
            }
            return pActor->GetName();
        }

        static bool Contains(const std::string& hay, const char* needle)
        {
            return hay.find(needle) != std::string::npos;
        }

        static bool IsMoneyOrCashName(const std::string& low)
        {
            return Contains(low, "money") || Contains(low, "cash") || Contains(low, "moneypile")
                || Contains(low, "dyepack") || Contains(low, "dye");
        }

        // Num7 IsLootActorName — whitelist only (blacklist-only was CreateBag'ing junk → empty slots)
        static bool IsLootPileName(const std::string& low)
        {
            if (low.empty())
                return false;
            if (Contains(low, "lootablecar") || Contains(low, "carbag") || Contains(low, "carpart")
                || Contains(low, "underliner"))
                return true;
            // Money / dye-cage cash always (Num7 adds BP_InteractableMoneyPile with no filter)
            if (IsMoneyOrCashName(low))
                return true;
            if (Contains(low, "meth_") || Contains(low, "muriatic") || Contains(low, "caustic")
                || Contains(low, "hydrogen") || Contains(low, "thermite") || Contains(low, "thermal")
                || Contains(low, "lance") || Contains(low, "turret") || Contains(low, "drill")
                || Contains(low, "ecm") || Contains(low, "door") || Contains(low, "button")
                || Contains(low, "keypad") || Contains(low, "hack") || Contains(low, "computer")
                || Contains(low, "methpure") || Contains(low, "cookingstation")
                || Contains(low, "multibaggenerator") || Contains(low, "placeable")
                || Contains(low, "ammobag") || Contains(low, "armorbag") || Contains(low, "medic")
                || Contains(low, "doctor") || Contains(low, "sentry"))
                return false;
            return Contains(low, "jewel")
                || Contains(low, "gold") || Contains(low, "coke") || Contains(low, "cocaine")
                || Contains(low, "paint") || Contains(low, "loot") || Contains(low, "pile")
                || Contains(low, "instantloot") || Contains(low, "baggenerator")
                || Contains(low, "singlebag") || Contains(low, "mineral") || Contains(low, "evidence")
                || Contains(low, "crypto") || Contains(low, "art") || Contains(low, "weaponbag")
                || Contains(low, "holdoutloot") || Contains(low, "baggable") || Contains(low, "cigar")
                || Contains(low, "statue") || Contains(low, "catfigure") || Contains(low, "figurine")
                || Contains(low, "teddy") || Contains(low, "sculpture") || Contains(low, "antique")
                || Contains(low, "satellite") || Contains(low, "prototype") || Contains(low, "opal")
                || Contains(low, "rarestone") || Contains(low, "raremineral")
                || Contains(low, "degradabletech") || Contains(low, "diamond") || Contains(low, "crystal")
                || Contains(low, "gem") || Contains(low, "weapon") || Contains(low, "pistol")
                || Contains(low, "rifle") || Contains(low, "server") || Contains(low, "electronics")
                || Contains(low, "watch");
        }

        static bool IsFloorBagName(const std::string& low)
        {
            if (low.empty())
                return true;
            return !(Contains(low, "turret") || Contains(low, "sentry") || Contains(low, "ecm")
                || Contains(low, "medic") || Contains(low, "armorbag") || Contains(low, "ammobag")
                || Contains(low, "thermal") || Contains(low, "lance") || Contains(low, "breaching")
                || Contains(low, "canister") || Contains(low, "zipline"));
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

        static bool HasLiveBagHandle(SDK::USBZBagManager* pBagMgr, SDK::FSBZBagHandle handle)
        {
            if (handle.Id <= 0 || !handle.BagType)
                return false;
            if (pBagMgr && !pBagMgr->IsValidHandle(handle))
                return false;
            return SDK::USBZBagHandleLibrary::IsValid(&handle);
        }

        static bool SafeClaimRaw(
            SDK::USBZBagManager* pBagMgr,
            SDK::FSBZBagHandle handle,
            SDK::AActor* pPawn,
            bool bFriends)
        {
            __try
            {
                if (!pBagMgr || !pPawn || !HasLiveBagHandle(pBagMgr, handle))
                    return false;
                // Game often returns false/nil even when claim worked — still count it.
                pBagMgr->ClaimBag(handle, pPawn);
                if (!bFriends && handle.Id > 0)
                    pBagMgr->Multicast_ClaimBag(handle.Id, pPawn);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool SafeCreateAndClaimRaw(
            SDK::USBZBagManager* pBagMgr,
            SDK::USBZBagType* pBagType,
            SDK::AActor* pPawn,
            bool bFriends)
        {
            __try
            {
                if (!pBagMgr || !pBagType || !pPawn)
                    return false;
                SDK::FSBZBagHandle created = pBagMgr->CreateBag(pBagType);
                if (created.Id <= 0 || !created.BagType)
                    return false;
                pBagMgr->ClaimBag(created, pPawn);
                if (!bFriends && created.Id > 0)
                    pBagMgr->Multicast_ClaimBag(created.Id, pPawn);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool InteractStillEnabled(SDK::ASBZSingleBagGenerator* pGen)
        {
            __try
            {
                if (!pGen || !pGen->Interactable)
                    return false;
                auto* pTyped = reinterpret_cast<SDK::USBZInteractableComponent*>(pGen->Interactable);
                return pTyped->bInteractionEnabled != 0;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        // Num7: after HandleInteraction pcall, treat as did=true (do NOT require interact off).
        static bool SafeHandleInteractionRaw(
            SDK::ASBZSingleBagGenerator* pGen,
            SDK::USBZInteractorComponent* pInteractor)
        {
            __try
            {
                if (!pGen || !pInteractor)
                    return false;
                auto* pInter = pGen->Interactable;
                if (!pInter)
                    return false;
                // Num7: call Handle even if flag looks off — dye cash still F-picks
                pGen->HandleInteraction(pInter, pInteractor, true);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        // Num7 CompleteFInteract — solo only; no Multicast in friends.
        static bool SafeServerCompleteRaw(
            SDK::USBZInteractorComponent* pInteractor,
            SDK::USBZBaseInteractableComponent* pInteractable,
            bool bFriends)
        {
            __try
            {
                if (bFriends || !pInteractor || !pInteractable)
                    return false;
                auto* pTyped = reinterpret_cast<SDK::USBZInteractableComponent*>(pInteractable);
                if (!pTyped->IsA(SDK::USBZInteractableComponent::StaticClass()))
                    return false;
                const int32_t id = ++pInteractor->InteractId;
                pInteractor->Server_StartInteraction(pInteractable, id, 0);
                pInteractor->Server_CompleteInteraction(pInteractable, id);
                if (Cheat::g_bIsSoloGame)
                    pInteractor->Multicast_CompletedInteraction(pInteractable, true);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool SafeDisablePileRaw(SDK::ASBZSingleBagGenerator* pGen)
        {
            __try
            {
                if (!pGen)
                    return false;
                pGen->SetInteractionEnabled(false);
                pGen->SetEnabled(false);
                if (pGen->Interactable)
                    pGen->Interactable->SetInteractionEnabled(false);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool SafeGrabInstantLootRaw(SDK::ASBZPlayerController* pController)
        {
            __try
            {
                if (!pController || !pController->CheatManager)
                    return false;
                if (!pController->CheatManager->IsA(SDK::USBZCheatManager::StaticClass()))
                    return false;
                reinterpret_cast<SDK::USBZCheatManager*>(pController->CheatManager)->GrabInstantLoot();
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool IsHost(SDK::UWorld* pGWorld, SDK::ASBZPlayerCharacter* pLocal)
        {
            if (pLocal && pLocal->HasAuthority())
                return true;
            if (pGWorld && SDK::UKismetSystemLibrary::IsServer(pGWorld))
                return true;
            return Cheat::g_bIsSoloGame;
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

        // ZERO CreateBag. That mints TAB bags while map loot stays (2x + empties).
        // DLL only does real F-pickup / Claim. Num7 still for anything this misses.
        // Do NOT mark done on bare Claim alone unless interact turns off (Lua SoftClaim bug).
        static bool PickupPile(
            SDK::ASBZSingleBagGenerator* pGen,
            SDK::ASBZPlayerCharacter* pPawn,
            SDK::USBZBagManager* pBagMgr,
            SDK::USBZInteractorComponent* pInteractor,
            bool bFriends)
        {
            if (!ActorOk(pGen) || !pPawn || AlreadyDone(pGen))
                return false;

            const std::string low = ToLower(ActorName(pGen));
            if (Contains(low, "methpure") || Contains(low, "cookingstation"))
                return false;
            if (!IsLootPileName(low))
            {
                SetDone(pGen);
                return false;
            }

            auto* pInter = pGen->Interactable;

            // A) Claim bag already spawned on this pile — only "solid" if pile interact dies
            if (pBagMgr && HasLiveBagHandle(pBagMgr, pGen->BagHandle))
            {
                if (SafeClaimRaw(pBagMgr, pGen->BagHandle, pPawn, bFriends))
                {
                    if (!InteractStillEnabled(pGen))
                    {
                        SafeDisablePileRaw(pGen);
                        SetDone(pGen);
                        return true;
                    }
                    // Claimed but pile still F-able — fall through to real F (don't SetDone)
                }
            }

            // B) Real F-pickup only — must actually turn interact OFF or it didn't grab
            if (pInteractor && pInter)
            {
                SafeHandleInteractionRaw(pGen, pInteractor);
                if (!bFriends)
                    SafeServerCompleteRaw(pInteractor, pInter, bFriends);

                if (!InteractStillEnabled(pGen))
                {
                    SafeDisablePileRaw(pGen);
                    SetDone(pGen);
                    return true;
                }
            }

            // Did not clear the pile — do NOT CreateBag, do NOT SetDone (retry / use Num7)
            return false;
        }

        // Soft claim for thrown/world bags — IsValidHandle is often false right after a drop.
        static bool SafeClaimFloorBagSeh(
            SDK::ASBZBagItem* pBag,
            SDK::ASBZPlayerCharacter* pPawn,
            SDK::USBZBagManager* pBagMgr,
            SDK::USBZInteractorComponent* pInteractor,
            bool bFriends)
        {
            __try
            {
                if (!pBag || !pPawn || !pBagMgr || !pBag->Class || pBag->IsActorBeingDestroyed())
                    return false;

                bool bDid = false;
                const SDK::FSBZBagHandle handle = pBag->Bag;
                const int32_t bagId = pBag->BagId;

                // A) Claim by handle (Num7: ignore return / IsValidHandle)
                if (handle.Id > 0 && handle.BagType)
                {
                    pBagMgr->ClaimBag(handle, pPawn);
                    bDid = true;
                }

                // B) Multicast by BagId (solo) — same as Num7 PickupFloorBag
                if (bagId > 0)
                {
                    if (handle.Id > 0 && handle.BagType)
                        pBagMgr->ClaimBag(handle, pPawn);
                    if (!bFriends)
                        pBagMgr->Multicast_ClaimBag(bagId, pPawn);
                    bDid = true;
                }

                // C) F-interact fallback (walk-up-and-press-F)
                auto* pInter = pBag->Interactable;
                if (pInteractor && pInter)
                {
                    pBag->OnPickup(pInter, pInteractor, true);
                    bDid = true;
                    if (!bFriends)
                    {
                        auto* pTyped = reinterpret_cast<SDK::USBZInteractableComponent*>(pInter);
                        if (pTyped->IsA(SDK::USBZInteractableComponent::StaticClass()))
                        {
                            const int32_t id = ++pInteractor->InteractId;
                            pInteractor->Server_StartInteraction(pInter, id, 0);
                            pInteractor->Server_CompleteInteraction(pInter, id);
                            if (Cheat::g_bIsSoloGame)
                                pInteractor->Multicast_CompletedInteraction(pInter, true);
                        }
                    }
                }

                return bDid;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        // Floor bags you dropped / world bags — Claim + Multicast + OnPickup (Num7 path).
        static bool PickupBag(
            SDK::ASBZBagItem* pBag,
            SDK::ASBZPlayerCharacter* pPawn,
            SDK::USBZBagManager* pBagMgr,
            SDK::USBZInteractorComponent* pInteractor,
            bool bFriends)
        {
            if (!ActorOk(pBag) || !pPawn || !pBagMgr || AlreadyDone(pBag))
                return false;

            if (!IsFloorBagName(ToLower(ActorName(pBag))))
            {
                SetDone(pBag);
                return false;
            }

            if (!SafeClaimFloorBagSeh(pBag, pPawn, pBagMgr, pInteractor, bFriends))
                return false;

            SetDone(pBag);
            return true;
        }

        static bool PickupInstant(
            SDK::ASBZInstantLoot* pLoot,
            SDK::USBZInteractorComponent* pInteractor)
        {
            if (!ActorOk(pLoot) || pLoot->bIsLooted || !pInteractor || AlreadyDone(pLoot))
                return false;

            auto* pInter = pLoot->Interactable;
            if (!pInter)
                return false;

            // Instant: one complete; don't require disabled check as strict (bIsLooted may lag)
            __try
            {
                auto* pTyped = reinterpret_cast<SDK::USBZInteractableComponent*>(pInter);
                if (!pTyped->bInteractionEnabled)
                {
                    SetDone(pLoot);
                    return false;
                }
                const int32_t id = ++pInteractor->InteractId;
                pInteractor->Server_StartInteraction(pInter, id, 0);
                pInteractor->Server_CompleteInteraction(pInter, id);
                if (Cheat::g_bIsSoloGame)
                    pInteractor->Multicast_CompletedInteraction(pInter, true);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }

            SetDone(pLoot);
            return true;
        }

        static void CollectTargets(SDK::UWorld* pGWorld, std::vector<Target_t>& out, bool bIncludeInstant)
        {
            out.clear();
            std::unordered_set<uintptr_t> seen{};

            auto pushUnique = [&](EKind kind, SDK::AActor* pActor)
            {
                if (!ActorOk(pActor) || AlreadyDone(pActor))
                    return;
                const uintptr_t key = reinterpret_cast<uintptr_t>(pActor);
                if (!seen.insert(key).second)
                    return;
                out.push_back(Target_t{ kind, pActor });
            };

            // Floor bags FIRST — money/meth/paint before other bags, then piles, then instant
            std::vector<Target_t> priorityBags{};
            std::vector<Target_t> otherBags{};
            {
                SDK::TArray<SDK::AActor*> bags{};
                SDK::UGameplayStatics::GetAllActorsOfClass(pGWorld, SDK::ASBZBagItem::StaticClass(), &bags);
                for (int i = 0; i < bags.Num(); ++i)
                {
                    auto* pBag = bags[i];
                    if (!pBag)
                        continue;
                    const std::string low = ToLower(ActorName(pBag));
                    if (!IsFloorBagName(low))
                        continue;
                    const uintptr_t key = reinterpret_cast<uintptr_t>(pBag);
                    if (AlreadyDone(pBag) || !seen.insert(key).second)
                        continue;
                    Target_t t{ EKind::Bag, pBag };
                    if (Contains(low, "money") || Contains(low, "meth") || Contains(low, "paint")
                        || Contains(low, "cash") || Contains(low, "dye"))
                        priorityBags.push_back(t);
                    else
                        otherBags.push_back(t);
                }
            }
            out.insert(out.end(), priorityBags.begin(), priorityBags.end());
            out.insert(out.end(), otherBags.begin(), otherBags.end());

            {
                SDK::TArray<SDK::AActor*> piles{};
                SDK::UGameplayStatics::GetAllActorsOfClass(pGWorld, SDK::ASBZSingleBagGenerator::StaticClass(), &piles);
                for (int i = 0; i < piles.Num(); ++i)
                {
                    auto* pPile = piles[i];
                    if (!pPile)
                        continue;
                    if (!IsLootPileName(ToLower(ActorName(pPile))))
                        continue;
                    pushUnique(EKind::Pile, pPile);
                }
            }

            if (bIncludeInstant)
            {
                SDK::TArray<SDK::AActor*> instants{};
                SDK::UGameplayStatics::GetAllActorsOfClass(pGWorld, SDK::ASBZInstantLoot::StaticClass(), &instants);
                for (int i = 0; i < instants.Num(); ++i)
                {
                    auto* pLoot = reinterpret_cast<SDK::ASBZInstantLoot*>(instants[i]);
                    if (!pLoot || pLoot->bIsLooted)
                        continue;
                    pushUnique(EKind::Instant, instants[i]);
                }
            }
        }

        static void CancelGrab(const char* reason)
        {
            ++s_uGen;
            s_bBusy = false;
            s_vecQueue.clear();
            g_sDebugStatus = reason ? reason : "GrabAll off";
        }

        static void ClearDoneCache()
        {
            s_setDone.clear();
        }

        static void StartGrab(
            SDK::UWorld* pGWorld,
            SDK::ASBZPlayerController* pController,
            SDK::ASBZPlayerCharacter* pLocal)
        {
            if (s_bBusy || !pLocal)
                return;

            const auto now = std::chrono::steady_clock::now();
            if (now - s_timeLastStart < kRescanGap)
                return;
            s_timeLastStart = now;

            ++s_uGen;
            s_bBusy = true;
            s_iOk = 0;
            s_iIndex = 0;
            s_bRetryPassDone = false;
            s_bFriends = IsFriendsLobby(pGWorld, pLocal);

            const bool bInstantBulk = SafeGrabInstantLootRaw(pController);
            if (bInstantBulk)
                ++s_iOk;

            CollectTargets(pGWorld, s_vecQueue, !bInstantBulk);
            s_iTotal = static_cast<int>(s_vecQueue.size());
            s_timeNextBatch = now;

            if (s_vecQueue.empty())
            {
                s_bBusy = false;
                g_sDebugStatus = bInstantBulk
                    ? "GrabAll instant OK — waiting for more piles"
                    : "GrabAll ON — no loot left";
                return;
            }

            g_sDebugStatus = "GrabAll loot=" + std::to_string(s_iTotal)
                + (bInstantBulk ? " +instant" : "")
                + (s_bFriends ? " FRIENDS" : " SOLO");
        }

        static bool SafeProcessOne(
            EKind kind,
            SDK::AActor* pActor,
            SDK::ASBZPlayerCharacter* pLocal,
            SDK::USBZBagManager* pBagMgr,
            SDK::USBZInteractorComponent* pInteractor,
            bool bFriends)
        {
            __try
            {
                if (!pActor || !pLocal)
                    return false;
                switch (kind)
                {
                case EKind::Pile:
                    if (pActor->IsA(SDK::ASBZSingleBagGenerator::StaticClass()))
                        return PickupPile(reinterpret_cast<SDK::ASBZSingleBagGenerator*>(pActor), pLocal, pBagMgr, pInteractor, bFriends);
                    break;
                case EKind::Bag:
                    if (pActor->IsA(SDK::ASBZBagItem::StaticClass()))
                        return PickupBag(reinterpret_cast<SDK::ASBZBagItem*>(pActor), pLocal, pBagMgr, pInteractor, bFriends);
                    break;
                case EKind::Instant:
                    if (pActor->IsA(SDK::ASBZInstantLoot::StaticClass()))
                        return PickupInstant(reinterpret_cast<SDK::ASBZInstantLoot*>(pActor), pInteractor);
                    break;
                }
                return false;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static void ProcessBatch(
            SDK::UWorld* pGWorld,
            SDK::ASBZPlayerCharacter* pLocal)
        {
            if (!s_bBusy || !pLocal)
                return;

            const auto now = std::chrono::steady_clock::now();
            if (now < s_timeNextBatch)
                return;

            auto* pInteractor = pLocal->Interactor;
            auto* pBagMgr = SDK::USBZBagManager::Get(pGWorld);
            const int batch = s_bFriends ? kBatchFriends : kBatchSolo;
            const auto waveGap = s_bFriends ? kWaveGapFriends : kWaveGapSolo;
            const int end = std::min(static_cast<int>(s_iIndex) + batch, s_iTotal);

            for (; static_cast<int>(s_iIndex) < end; ++s_iIndex)
            {
                if (s_iIndex >= s_vecQueue.size())
                    break;

                Target_t& t = s_vecQueue[s_iIndex];
                if (!ActorOk(t.m_pActor))
                    continue;

                if (SafeProcessOne(t.m_eKind, t.m_pActor, pLocal, pBagMgr, pInteractor, s_bFriends))
                    ++s_iOk;
            }

            g_sDebugStatus = "GrabAll " + std::to_string(s_iOk) + "/" + std::to_string(s_iTotal)
                + " @" + std::to_string(s_iIndex);

            if (static_cast<int>(s_iIndex) >= s_iTotal)
            {
                // One retry pass for piles/bags that failed (not SetDone) — then idle
                if (!s_bRetryPassDone)
                {
                    s_bRetryPassDone = true;
                    CollectTargets(pGWorld, s_vecQueue, true);
                    if (!s_vecQueue.empty())
                    {
                        s_iIndex = 0;
                        s_iTotal = static_cast<int>(s_vecQueue.size());
                        s_timeNextBatch = now + kRetryGap;
                        g_sDebugStatus = "GrabAll retry " + std::to_string(s_iTotal)
                            + " leftovers";
                        return;
                    }
                }
                s_bBusy = false;
                g_sDebugStatus = "GrabAll wave " + std::to_string(s_iOk)
                    + " — idle (Num7 for CreateBag leftovers)";
                s_timeLastStart = now;
                return;
            }

            s_timeNextBatch = now + waveGap;
        }
    }

    void OnPlayerControllerTick(
        SDK::UWorld* pGWorld,
        SDK::ASBZPlayerController* pLocalController,
        SDK::ASBZPlayerCharacter* pLocalPlayer)
    {
        if (!CheatConfig::Get().m_misc.m_bGrabAll)
        {
            if (s_bBusy || !s_setDone.empty() || g_sDebugStatus != "GrabAll off")
            {
                ClearDoneCache();
                CancelGrab("GrabAll off");
            }
            return;
        }

        if (!pGWorld || !pLocalPlayer)
            return;

        if (!Cheat::g_bIsInGame)
        {
            if (s_bBusy)
                CancelGrab("GrabAll paused (not in heist)");
            else
                g_sDebugStatus = "GrabAll ON — wait for heist";
            ClearDoneCache();
            return;
        }

        StartGrab(pGWorld, pLocalController, pLocalPlayer);
        ProcessBatch(pGWorld, pLocalPlayer);
    }
}
