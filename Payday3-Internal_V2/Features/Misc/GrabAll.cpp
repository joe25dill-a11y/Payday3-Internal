#include "pch.h"
#include "GrabAll.hpp"
#include "HeistUtil.hpp"

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <string>
#include <unordered_set>
#include <vector>

#undef min
#undef max

namespace GrabAll
{
    std::string g_sDebugStatus = "GrabAll off";

    namespace
    {
        // DLL = Claim/F only (no CreateBag — that mints TAB bags while piles stay).
        // Num7 Lua still covers CreateBag leftovers.
        // v2: floor bags first (already), harden live-Claim, friends pacing, one retry pass.
        enum class EKind : uint8_t
        {
            Instant,
            Bag,
            Pile,
            Multi
        };

        struct Target_t
        {
            EKind m_eKind = EKind::Pile;
            SDK::AActor* m_pActor = nullptr;
        };

        // One press = one full sweep (instant + bags + piles), then STOP.
        // Continuous InstantLoot pulses were crashing after long sessions (AV via UE4SS).
        constexpr int kBatchSolo = 10;
        constexpr int kBatchFriends = 6;
        constexpr int kMaxRetries = 2;
        constexpr auto kWaveGapSolo = std::chrono::milliseconds(280);
        constexpr auto kWaveGapFriends = std::chrono::milliseconds(450);
        constexpr auto kRetryGap = std::chrono::milliseconds(400);

        static bool s_bBusy = false;
        static uint32_t s_uGen = 0;
        static int s_iOk = 0;
        static int s_iTotal = 0;
        static size_t s_iIndex = 0;
        static bool s_bFriends = false;
        static bool s_bAllowCreateBag = false;
        static bool s_bSweepDone = false;
        static bool s_bWasEnabled = false;
        static int s_iRetryPass = 0;
        static std::vector<Target_t> s_vecQueue{};
        static std::unordered_set<uintptr_t> s_setDone{};
        static std::chrono::steady_clock::time_point s_timeNextBatch{};

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
                // Multicast by id only (safe after bag actor dies). Skip in friends.
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
                if (!pController)
                    return false;
                if (!pController->CheatManager)
                    pController->EnableCheats();
                if (!pController->CheatManager)
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
            return HeistUtil::IsSoloGame();
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

            // B) Real F-pickup — Handle + Server_Complete (solo)
            if (pInteractor && pInter)
            {
                SafeHandleInteractionRaw(pGen, pInteractor);
                if (!ActorOk(pGen))
                {
                    SetDone(pGen);
                    return true;
                }
                if (!bFriends)
                    SafeServerCompleteRaw(pInteractor, pInter, bFriends);

                if (!InteractStillEnabled(pGen))
                {
                    SafeDisablePileRaw(pGen);
                    SetDone(pGen);
                    return true;
                }
            }

            // C) Num7 leftover — CreateBag from pile BagType, then kill the pile interact
            if (s_bAllowCreateBag && pBagMgr && pGen->BagType)
            {
                if (SafeCreateAndClaimRaw(pBagMgr, pGen->BagType, pPawn, bFriends))
                {
                    SafeDisablePileRaw(pGen);
                    SetDone(pGen);
                    return true;
                }
            }

            // Did not clear — retry later
            return false;
        }

        static bool PickupMultiSeh(
            SDK::ASBZMultiBagGenerator* pGen,
            SDK::ASBZPlayerCharacter* pPawn,
            SDK::USBZBagManager* pBagMgr,
            SDK::USBZInteractorComponent* pInteractor,
            bool bFriends)
        {
            bool bDid = false;
            __try
            {
                if (!pGen || !pPawn)
                    return false;

                if (pBagMgr)
                {
                    const int n = pGen->BagHandleArray.Num();
                    for (int i = 0; i < n; ++i)
                    {
                        SDK::FSBZBagHandle handle = pGen->BagHandleArray[i];
                        if (handle.Id > 0 && handle.BagType)
                        {
                            pBagMgr->ClaimBag(handle, pPawn);
                            if (!bFriends)
                                pBagMgr->Multicast_ClaimBag(handle.Id, pPawn);
                            bDid = true;
                        }
                    }
                }

                auto* pInter = pGen->InteractableComponent;
                if (pInteractor && pInter)
                {
                    if (!bFriends && pInter->IsA(SDK::USBZInteractableComponent::StaticClass()))
                    {
                        const int32_t id = ++pInteractor->InteractId;
                        pInteractor->Server_StartInteraction(pInter, id, 0);
                        pInteractor->Server_CompleteInteraction(pInter, id);
                    }
                    pGen->OnServerCompleteInteraction(pInter, pInteractor, true);
                    bDid = true;
                }

                pGen->SetEnabled(false);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
            return bDid;
        }

        static bool PickupMulti(
            SDK::ASBZMultiBagGenerator* pGen,
            SDK::ASBZPlayerCharacter* pPawn,
            SDK::USBZBagManager* pBagMgr,
            SDK::USBZInteractorComponent* pInteractor,
            bool bFriends)
        {
            if (!ActorOk(pGen) || !pPawn || AlreadyDone(pGen))
                return false;

            const std::string low = ToLower(ActorName(pGen));
            if (Contains(low, "methpure") || Contains(low, "cookingstation"))
            {
                SetDone(pGen);
                return false;
            }

            bool bDid = PickupMultiSeh(pGen, pPawn, pBagMgr, pInteractor, bFriends);

            if (s_bAllowCreateBag && pBagMgr && pGen->BagType)
            {
                int bags = pGen->NumberOfBags;
                if (bags < 1)
                    bags = 1;
                if (bags > 16)
                    bags = 16;
                for (int i = 0; i < bags; ++i)
                {
                    if (SafeCreateAndClaimRaw(pBagMgr, pGen->BagType, pPawn, bFriends))
                        bDid = true;
                }
            }

            SetDone(pGen);
            return bDid;
        }

        // Floor bags: Claim + Multicast(by id) like v1/Num7.
        // Only OnPickup/Server if the bag actor is STILL alive after claim.
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

                if (handle.Id > 0 && handle.BagType)
                {
                    pBagMgr->ClaimBag(handle, pPawn);
                    bDid = true;
                }

                if (bagId > 0)
                {
                    if (handle.Id > 0 && handle.BagType)
                        pBagMgr->ClaimBag(handle, pPawn);
                    if (!bFriends)
                        pBagMgr->Multicast_ClaimBag(bagId, pPawn);
                    bDid = true;
                }

                // F-interact only while actor still valid (post-claim destroy = pure-virtual box)
                if (pBag->Class && !pBag->IsActorBeingDestroyed())
                {
                    auto* pInter = pBag->Interactable;
                    if (pInteractor && pInter)
                    {
                        pBag->OnPickup(pInter, pInteractor, true);
                        bDid = true;
                        if (!bFriends && pInter->IsA(SDK::USBZInteractableComponent::StaticClass()))
                        {
                            const int32_t id = ++pInteractor->InteractId;
                            pInteractor->Server_StartInteraction(pInter, id, 0);
                            pInteractor->Server_CompleteInteraction(pInter, id);
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
            if (!ActorOk(pLoot) || !pInteractor || AlreadyDone(pLoot))
                return false;

            bool bOk = false;
            bool bMarkDone = false;
            __try
            {
                if (pLoot->bIsLooted)
                {
                    bMarkDone = true;
                }
                else
                {
                    auto* pInter = pLoot->Interactable;
                    if (pInter && pInter->IsA(SDK::USBZInteractableComponent::StaticClass()))
                    {
                        auto* pTyped = reinterpret_cast<SDK::USBZInteractableComponent*>(pInter);
                        if (pTyped->bInteractionEnabled)
                        {
                            const int32_t id = ++pInteractor->InteractId;
                            pInteractor->Server_StartInteraction(pInter, id, 0);
                            pInteractor->Server_CompleteInteraction(pInter, id);
                            bOk = true;
                            // Only permanent-done when game says looted — else rescan can retry
                            bMarkDone = pLoot->bIsLooted;
                        }
                    }
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }

            if (bMarkDone)
                SetDone(pLoot);
            return bOk;
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

            // Instant / loose cash FIRST (desks, floors), then bags, then piles
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

            {
                SDK::TArray<SDK::AActor*> multis{};
                SDK::UGameplayStatics::GetAllActorsOfClass(pGWorld, SDK::ASBZMultiBagGenerator::StaticClass(), &multis);
                for (int i = 0; i < multis.Num(); ++i)
                {
                    auto* pMulti = reinterpret_cast<SDK::ASBZMultiBagGenerator*>(multis[i]);
                    if (!pMulti)
                        continue;
                    const std::string low = ToLower(ActorName(pMulti));
                    if (Contains(low, "methpure") || Contains(low, "cookingstation"))
                        continue;
                    if (!IsLootPileName(low) && !Contains(low, "multibag"))
                        continue;
                    pushUnique(EKind::Multi, pMulti);
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
            if (s_bBusy || s_bSweepDone || !pLocal)
                return;

            ++s_uGen;
            s_bBusy = true;
            s_iOk = 0;
            s_iIndex = 0;
            s_iRetryPass = 0;
            s_bAllowCreateBag = false;
            s_bFriends = IsFriendsLobby(pGWorld, pLocal);
            ClearDoneCache();

            // Loose cash / jewelry once at the start of the sweep
            const bool bInstantBulk = SafeGrabInstantLootRaw(pController);
            if (bInstantBulk)
                ++s_iOk;

            CollectTargets(pGWorld, s_vecQueue, true);
            s_iTotal = static_cast<int>(s_vecQueue.size());
            s_timeNextBatch = std::chrono::steady_clock::now();

            if (s_vecQueue.empty())
            {
                s_bBusy = false;
                s_bSweepDone = true;
                g_sDebugStatus = bInstantBulk
                    ? "GrabAll done — instant loot grabbed"
                    : "GrabAll done — nothing left";
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
            // Do NOT wrap pile/bag pickups in __try here — they use std::string.
            // SEH over C++ objects causes abort dialogs ("line 500" style boxes) on AV.
            // ProcessEvent leaves already have their own SEH.
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
            case EKind::Multi:
                if (pActor->IsA(SDK::ASBZMultiBagGenerator::StaticClass()))
                    return PickupMulti(reinterpret_cast<SDK::ASBZMultiBagGenerator*>(pActor), pLocal, pBagMgr, pInteractor, bFriends);
                break;
            }
            return false;
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
                if (!ActorOk(t.m_pActor) || AlreadyDone(t.m_pActor))
                {
                    SetDone(t.m_pActor);
                    continue;
                }

                if (SafeProcessOne(t.m_eKind, t.m_pActor, pLocal, pBagMgr, pInteractor, s_bFriends))
                    ++s_iOk;
            }

            g_sDebugStatus = "GrabAll " + std::to_string(s_iOk) + "/" + std::to_string(s_iTotal)
                + " @" + std::to_string(s_iIndex);

            if (static_cast<int>(s_iIndex) >= s_iTotal)
            {
                if (s_iRetryPass < kMaxRetries)
                {
                    ++s_iRetryPass;
                    s_bAllowCreateBag = true;
                    SafeGrabInstantLootRaw(HeistUtil::GetLocalSBZController());
                    CollectTargets(pGWorld, s_vecQueue, true);
                    if (!s_vecQueue.empty())
                    {
                        s_iIndex = 0;
                        s_iTotal = static_cast<int>(s_vecQueue.size());
                        s_timeNextBatch = now + kRetryGap;
                        g_sDebugStatus = "GrabAll leftovers " + std::to_string(s_iTotal)
                            + " (pass " + std::to_string(s_iRetryPass) + ")";
                        return;
                    }
                }

                SafeGrabInstantLootRaw(HeistUtil::GetLocalSBZController());
                s_bBusy = false;
                s_bAllowCreateBag = false;
                s_bSweepDone = true;
                s_vecQueue.clear();
                g_sDebugStatus = "GrabAll done (" + std::to_string(s_iOk) + ") — toggle to grab again";
                return;
            }

            s_timeNextBatch = now + waveGap;
        }
    }

    void Tick(
        SDK::UWorld* pGWorld,
        SDK::ASBZPlayerController* pLocalController,
        SDK::ASBZPlayerCharacter* pLocalPlayer,
        bool bEnabled)
    {
        if (Framework::bProcessExiting || !Framework::bShouldRun)
            return;

        if (!bEnabled)
        {
            if (s_bBusy || s_bSweepDone || s_bWasEnabled || !s_setDone.empty() || g_sDebugStatus != "GrabAll off")
            {
                ClearDoneCache();
                s_bSweepDone = false;
                CancelGrab("GrabAll off");
            }
            s_bWasEnabled = false;
            return;
        }

        if (!pGWorld || !pLocalPlayer)
            return;

        if (!HeistUtil::IsInHeist())
        {
            if (s_bBusy)
                CancelGrab("GrabAll paused (not in heist)");
            else
                g_sDebugStatus = "GrabAll ON — wait for heist";
            ClearDoneCache();
            s_bSweepDone = false;
            return;
        }

        if (!s_bWasEnabled)
        {
            s_bSweepDone = false;
            s_bWasEnabled = true;
        }

        if (s_bSweepDone)
            return;

        StartGrab(pGWorld, pLocalController, pLocalPlayer);
        ProcessBatch(pGWorld, pLocalPlayer);
    }
}
