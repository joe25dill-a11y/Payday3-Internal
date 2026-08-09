#include "SpawnerTools.hpp"
#include "../../Menu.hpp"
#include "../../Utils/Logging.hpp"

// Use Utils::LogDebug for diagnostics.

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <string>
#include <vector>

namespace Cheat::SpawnerTools
{
    std::string g_sStatusMeth = "idle";
    std::string g_sStatusVan = "idle";
    std::string g_sStatusExit = "idle";
    std::string g_sStatusMoney = "idle";

    namespace
    {
        constexpr int kMethBags = 40;
        constexpr float kMethDelay = 0.05f;
        constexpr int kMaxExitPoints = 4;
        constexpr auto kVanFinalizeDelay = std::chrono::milliseconds(2800);
        constexpr auto kEscapeTimerZeroDelay = std::chrono::milliseconds(3200);

        enum class EPending : uint8_t
        {
            None = 0,
            Meth,
            Van,
            GreenExit,
            Money
        };

        static EPending s_ePending = EPending::None;
        static bool s_bVanFinalizePending = false;
        static std::chrono::steady_clock::time_point s_timeVanFinalize{};
        static bool s_bEscapeTimerZeroPending = false;
        static std::chrono::steady_clock::time_point s_timeEscapeTimerZero{};

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

        static bool ActorOk(SDK::AActor* pActor)
        {
            return ActorOkSeh(pActor);
        }

        static std::string ToLower(std::string s)
        {
            for (char& c : s)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        }

        // GetName only — never GetFullName (Outer walk can crash).
        static std::string SafeNameLower(SDK::UObject* pObj)
        {
            if (!pObj)
                return {};
            return ToLower(pObj->GetName());
        }

        static bool Contains(const std::string& hay, const char* needle)
        {
            return hay.find(needle) != std::string::npos;
        }

        static bool LooksLikeEscapeName(const std::string& s)
        {
            return Contains(s, "escape") || Contains(s, "leave") || Contains(s, "exfil")
                || Contains(s, "extract") || Contains(s, "getaway") || Contains(s, "evac");
        }

        static bool IsPureMethType(SDK::USBZBagType* pBt)
        {
            if (!pBt)
                return false;
            const std::string n = SafeNameLower(pBt);
            if (n.empty())
                return false;
            if (Contains(n, "burnt") || Contains(n, "burn") || Contains(n, "caustic")
                || Contains(n, "muriatic") || Contains(n, "hydrogen") || Contains(n, "ingredient")
                || Contains(n, "overkill") || Contains(n, "ovk") || Contains(n, "paint"))
                return false;
            return Contains(n, "pure") && (Contains(n, "meth") || Contains(n, "bag"));
        }

        static SDK::USBZBagType* BagTypeOkSeh(SDK::USBZBagType* pBt)
        {
            __try
            {
                if (!pBt || !pBt->Class)
                    return nullptr;
                if (!pBt->IsA(SDK::USBZBagType::StaticClass()))
                    return nullptr;
                return pBt;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return nullptr;
            }
        }

        static bool ReadCookingBagTypesSeh(SDK::ASBZCookingStation* pSt,
            SDK::USBZBagType** ppSec, SDK::USBZBagType** ppTer, SDK::USBZBagType** ppPri)
        {
            __try
            {
                if (!pSt || !ppSec || !ppTer || !ppPri)
                    return false;
                *ppSec = pSt->SecondaryBagType;
                *ppTer = pSt->TertiaryBagType;
                *ppPri = pSt->BagType;
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static SDK::USBZBagType* ReadSpawnerBagTypeSeh(SDK::ASBZBagSpawner* pSp)
        {
            __try
            {
                return pSp ? pSp->BagTypeToSpawn : nullptr;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return nullptr;
            }
        }

        // Cooking station / debug spawner only — no GObjects scans (those crash).
        static SDK::USBZBagType* ResolvePureMethBagType(SDK::UWorld* pWorld)
        {
            if (!pWorld)
                return nullptr;

            if (auto* cls = SDK::ASBZCookingStation::StaticClass())
            {
                SDK::TArray<SDK::AActor*> stations{};
                SDK::UGameplayStatics::GetAllActorsOfClass(pWorld, cls, &stations);
                for (int i = 0; i < stations.Num(); ++i)
                {
                    auto* st = reinterpret_cast<SDK::ASBZCookingStation*>(stations[i]);
                    if (!ActorOk(st))
                        continue;

                    SDK::USBZBagType* sec = nullptr;
                    SDK::USBZBagType* ter = nullptr;
                    SDK::USBZBagType* pri = nullptr;
                    if (!ReadCookingBagTypesSeh(st, &sec, &ter, &pri))
                        continue;

                    SDK::USBZBagType* raw[3] = { sec, ter, pri };
                    for (auto* cand : raw)
                    {
                        auto* bt = BagTypeOkSeh(cand);
                        if (bt && IsPureMethType(bt))
                            return bt;
                    }
                }
            }

            if (auto* spCls = SDK::ASBZBagSpawner::StaticClass())
            {
                SDK::TArray<SDK::AActor*> spawners{};
                SDK::UGameplayStatics::GetAllActorsOfClass(pWorld, spCls, &spawners);
                for (int i = 0; i < spawners.Num(); ++i)
                {
                    auto* sp = reinterpret_cast<SDK::ASBZBagSpawner*>(spawners[i]);
                    if (!ActorOk(sp))
                        continue;
                    const std::string n = SafeNameLower(sp);
                    if (!(Contains(n, "bagspawnerdebug") || Contains(n, "spawnerdebug")))
                        continue;
                    if (Contains(n, "ovk") || Contains(n, "heli"))
                        continue;
                    auto* bt = BagTypeOkSeh(ReadSpawnerBagTypeSeh(sp));
                    if (bt && IsPureMethType(bt))
                        return bt;
                }
            }
            return nullptr;
        }

        static SDK::ASBZBagSpawner* FindDebugMethSpawner(SDK::UWorld* pWorld)
        {
            auto* cls = SDK::ASBZBagSpawner::StaticClass();
            if (!pWorld || !cls)
                return nullptr;

            SDK::TArray<SDK::AActor*> list{};
            SDK::UGameplayStatics::GetAllActorsOfClass(pWorld, cls, &list);
            for (int i = 0; i < list.Num(); ++i)
            {
                auto* sp = reinterpret_cast<SDK::ASBZBagSpawner*>(list[i]);
                if (!ActorOk(sp))
                    continue;
                const std::string n = SafeNameLower(sp);
                if (Contains(n, "ovk") || Contains(n, "helicopter") || Contains(n, "heli"))
                    continue;
                if (Contains(n, "bagspawnerdebug") || Contains(n, "spawnerdebug"))
                    return sp;
            }
            return nullptr;
        }

        static bool StartMethSpawnSeh(SDK::ASBZBagSpawner* pSp, SDK::USBZBagType* pPure)
        {
            __try
            {
                if (!pSp || !pSp->Class || pSp->IsActorBeingDestroyed())
                    return false;
                pSp->NumberOfBags = kMethBags;
                pSp->DelayBetweenSpawns = kMethDelay;
                if (pPure)
                    pSp->BagTypeToSpawn = pPure;
                pSp->StartSpawningBags();
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static void DoMeth(SDK::UWorld* pWorld)
        {
            Utils::LogDebug("[SpawnerTools] Meth start");
            auto* sp = FindDebugMethSpawner(pWorld);
            if (!sp)
            {
                g_sStatusMeth = "FAIL - no BagSpawnerDEBUG (Cook Off only)";
                return;
            }
            auto* pure = ResolvePureMethBagType(pWorld);
            if (StartMethSpawnSeh(sp, pure))
            {
                g_sStatusMeth = pure
                    ? ("OK - ~" + std::to_string(kMethBags) + " PURE meth @ debug pad")
                    : ("OK - ~" + std::to_string(kMethBags) + " bags (spawner default type)");
            }
            else
            {
                g_sStatusMeth = "FAIL - StartSpawningBags errored";
            }
        }

        static SDK::FVector ActorLocSeh(SDK::AActor* pActor)
        {
            SDK::FVector v{};
            __try
            {
                if (pActor)
                    v = pActor->K2_GetActorLocation();
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
            return v;
        }

        static float DistSq(const SDK::FVector& a, const SDK::FVector& b)
        {
            const float dx = a.X - b.X;
            const float dy = a.Y - b.Y;
            const float dz = a.Z - b.Z;
            return dx * dx + dy * dy + dz * dz;
        }

        static void CollectEscapeSplines(SDK::UWorld* pWorld, std::vector<SDK::ASBZTrafficSpline*>& out)
        {
            out.clear();
            auto* cls = SDK::ASBZTrafficSpline::StaticClass();
            if (!pWorld || !cls)
                return;

            SDK::TArray<SDK::AActor*> list{};
            SDK::UGameplayStatics::GetAllActorsOfClass(pWorld, cls, &list);
            for (int i = 0; i < list.Num(); ++i)
            {
                auto* s = reinterpret_cast<SDK::ASBZTrafficSpline*>(list[i]);
                if (!ActorOk(s))
                    continue;
                const std::string n = SafeNameLower(s);
                if (Contains(n, "escape") || Contains(n, "lootvan") || Contains(n, "evac")
                    || Contains(n, "extract") || Contains(n, "van"))
                    out.push_back(s);
            }
        }

        static SDK::ASBZTrafficSpline* FindNamedSpline(const std::vector<SDK::ASBZTrafficSpline*>& splines, const char* needle)
        {
            for (auto* s : splines)
            {
                if (Contains(SafeNameLower(s), needle))
                    return s;
            }
            return nullptr;
        }

        static int ScoreSpline(SDK::ASBZTrafficSpline* sp, bool bStart)
        {
            const std::string n = SafeNameLower(sp);
            int s = 0;
            if (Contains(n, "flyin") || Contains(n, "fly_in") || Contains(n, "heli"))
                s -= 40;
            if (Contains(n, "van") || Contains(n, "lower") || Contains(n, "ground"))
                s += 35;
            if (Contains(n, "drive") || Contains(n, "road"))
                s += 15;
            if (bStart)
            {
                if (Contains(n, "start") || Contains(n, "spawn") || Contains(n, "enter"))
                    s += 20;
            }
            else if (Contains(n, "dest") || Contains(n, "arrive") || Contains(n, "stop")
                || Contains(n, "drop") || Contains(n, "park") || Contains(n, "end") || Contains(n, "_in"))
            {
                s += 20;
            }
            return s;
        }

        static void PickBestPair(const std::vector<SDK::ASBZTrafficSpline*>& splines,
            SDK::ASBZTrafficSpline*& outStart, SDK::ASBZTrafficSpline*& outDest)
        {
            outStart = outDest = nullptr;
            if (splines.empty())
                return;
            if (splines.size() == 1)
            {
                outStart = outDest = splines[0];
                return;
            }
            int best = -9999;
            for (size_t i = 0; i < splines.size(); ++i)
            {
                for (size_t j = 0; j < splines.size(); ++j)
                {
                    if (i == j)
                        continue;
                    const int sc = ScoreSpline(splines[i], true) + ScoreSpline(splines[j], false);
                    if (sc > best)
                    {
                        best = sc;
                        outStart = splines[i];
                        outDest = splines[j];
                    }
                }
            }
        }

        static void FindEscapeVans(SDK::UWorld* pWorld, std::vector<SDK::ASBZWheeledVehicle*>& out)
        {
            out.clear();
            if (!pWorld)
                return;

            auto collect = [&](SDK::UClass* cls)
            {
                if (!cls)
                    return;
                SDK::TArray<SDK::AActor*> list{};
                SDK::UGameplayStatics::GetAllActorsOfClass(pWorld, cls, &list);
                for (int i = 0; i < list.Num(); ++i)
                {
                    auto* v = reinterpret_cast<SDK::ASBZWheeledVehicle*>(list[i]);
                    if (!ActorOk(v))
                        continue;
                    const std::string n = SafeNameLower(v);
                    if (Contains(n, "escapevan") || (Contains(n, "escape") && Contains(n, "van")))
                        out.push_back(v);
                }
            };

            collect(SDK::ASBZSabotagableVehicle::StaticClass());
            if (out.empty())
                collect(SDK::ASBZWheeledVehicle::StaticClass());

            std::sort(out.begin(), out.end(), [](SDK::ASBZWheeledVehicle* a, SDK::ASBZWheeledVehicle* b)
            {
                const std::string na = SafeNameLower(a);
                const std::string nb = SafeNameLower(b);
                const int sa = (Contains(na, "lower") ? 100 : 0) + (Contains(na, "escape") ? 10 : 0);
                const int sb = (Contains(nb, "lower") ? 100 : 0) + (Contains(nb, "escape") ? 10 : 0);
                return sa > sb;
            });
        }

        static bool TryDrivePairSeh(SDK::UWorld* pWorld, SDK::ASBZWheeledVehicle* pVan,
            SDK::ASBZTrafficSpline* pStart, SDK::ASBZTrafficSpline* pDest)
        {
            __try
            {
                if (!pWorld || !pVan || !pStart || !pDest)
                    return false;
                if (!pVan->Class || pVan->IsActorBeingDestroyed())
                    return false;
                SDK::USBZTrafficFunctionLibrary::SetEscapeVan(pWorld, pVan);
                return SDK::USBZTrafficFunctionLibrary::SetAndDriveCustomVehicleRoute(
                    pWorld, pVan, pStart, pDest, true);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool SetBagVolumeEnabledSeh(SDK::ASBZBagTriggerVolume* pVol)
        {
            __try
            {
                if (!pVol || !pVol->Class || pVol->IsActorBeingDestroyed())
                    return false;
                pVol->SetVolumeEnabled(true);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool SetPlayerEscapeEnabledSeh(SDK::ASBZPlayerEscapeVolume* pVol)
        {
            __try
            {
                if (!pVol || !pVol->Class || pVol->IsActorBeingDestroyed())
                    return false;
                // Only SetVolumeEnabled — Hidden/Collision/Tick calls crashed some maps.
                pVol->SetVolumeEnabled(true);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool ActivateObjectiveSeh(SDK::ASBZObjective* pObj)
        {
            __try
            {
                if (!pObj || !pObj->Class || pObj->IsActorBeingDestroyed())
                    return false;
                pObj->Activate(false, false);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        // AddMarker without MarkerAsset crashes — require asset first.
        static bool AddWaypointMarkerSeh(SDK::ASBZWaypoint* pWp)
        {
            __try
            {
                if (!pWp || !pWp->Class || pWp->IsActorBeingDestroyed())
                    return false;
                if (!pWp->MarkerAsset)
                    return false;
                pWp->AddMarker();
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool OpenVanDoorsSeh(SDK::ASBZWheeledVehicle* pVan)
        {
            __try
            {
                if (!pVan || !pVan->Class || pVan->IsActorBeingDestroyed())
                    return false;
                if (!pVan->IsA(SDK::ASBZSabotagableVehicle::StaticClass()))
                    return false;
                reinterpret_cast<SDK::ASBZSabotagableVehicle*>(pVan)->SetRearDoorsState(
                    SDK::ESBZVehicleDoorState::Opened);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool MulticastVanDoorsSeh(SDK::ASBZWheeledVehicle* pVan)
        {
            __try
            {
                if (!pVan || !pVan->IsA(SDK::ASBZSabotagableVehicle::StaticClass()))
                    return false;
                reinterpret_cast<SDK::ASBZSabotagableVehicle*>(pVan)->Multicast_SetRearDoorsState(
                    SDK::ESBZVehicleDoorState::Opened);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool HandleVanArrivedSeh(SDK::UWorld* pWorld, SDK::ASBZWheeledVehicle* pVan)
        {
            __try
            {
                if (!pWorld || !pVan)
                    return false;
                SDK::USBZTrafficFunctionLibrary::SetEscapeVan(pWorld, pVan);
                auto* tm = SDK::USBZTrafficFunctionLibrary::GetTrafficManager(pWorld);
                if (!tm)
                    return false;
                tm->HandleEscapeVanArrived(pVan);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool ForceEscapeVolumeDataSeh(SDK::UWorld* pWorld)
        {
            __try
            {
                auto* ms = SDK::ASBZMissionState::GetSBZMissionState(pWorld);
                if (!ms)
                    return false;
                ms->Multicast_SetEscapeVolumeData(1, 1);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool ForceEscapeTimeLeftSeh(SDK::UWorld* pWorld, int32_t time)
        {
            __try
            {
                auto* ms = SDK::ASBZMissionState::GetSBZMissionState(pWorld);
                if (!ms)
                    return false;
                ms->Multicast_SetEscapeTimeLeft(time);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static void ForceEscapeCountdown(SDK::UWorld* pWorld)
        {
            const bool a = ForceEscapeVolumeDataSeh(pWorld);
            const bool b = ForceEscapeTimeLeftSeh(pWorld, 3);
            if (a || b)
            {
                s_bEscapeTimerZeroPending = true;
                s_timeEscapeTimerZero = std::chrono::steady_clock::now() + kEscapeTimerZeroDelay;
            }
        }

        static void ZeroEscapeTimerSeh(SDK::UWorld* pWorld)
        {
            ForceEscapeTimeLeftSeh(pWorld, 0);
        }

        static bool RequestMissionSuccessSeh(SDK::ASBZPlayerController* pPC)
        {
            __try
            {
                if (!pPC || !pPC->CheatManager)
                    return false;
                if (!pPC->CheatManager->IsA(SDK::USBZCheatManager::StaticClass()))
                    return false;
                reinterpret_cast<SDK::USBZCheatManager*>(pPC->CheatManager)->RequestMissionSuccess(1);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool RequestMissionEndSeh(SDK::ASBZPlayerController* pPC)
        {
            __try
            {
                if (!pPC || !pPC->CheatManager)
                    return false;
                if (!pPC->CheatManager->IsA(SDK::USBZCheatManager::StaticClass()))
                    return false;
                reinterpret_cast<SDK::USBZCheatManager*>(pPC->CheatManager)->RequestMissionEnd(
                    SDK::ESBZEndMissionResult::Success, 0, 1);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static int EnableEscapeBagVolumes(SDK::UWorld* pWorld)
        {
            int n = 0;
            auto* cls = SDK::ASBZBagTriggerVolume::StaticClass();
            if (!pWorld || !cls)
                return 0;

            SDK::TArray<SDK::AActor*> list{};
            SDK::UGameplayStatics::GetAllActorsOfClass(pWorld, cls, &list);
            for (int i = 0; i < list.Num(); ++i)
            {
                auto* vol = reinterpret_cast<SDK::ASBZBagTriggerVolume*>(list[i]);
                if (!ActorOk(vol))
                    continue;
                const std::string name = SafeNameLower(vol);
                if (!(Contains(name, "escape") || Contains(name, "van") || Contains(name, "lower")
                    || Contains(name, "secure") || Contains(name, "loot") || Contains(name, "bag")
                    || Contains(name, "elevator") || Contains(name, "deposit") || Contains(name, "drop")))
                    continue;
                if (SetBagVolumeEnabledSeh(vol))
                    ++n;
            }
            return n;
        }

        struct ExitCand
        {
            SDK::AActor* pActor = nullptr;
            float flDistSq = 0.f;
            int iPrio = 99;
        };

        static void TakeClosest(std::vector<ExitCand>& cands, int maxN, std::vector<SDK::AActor*>& out)
        {
            out.clear();
            std::sort(cands.begin(), cands.end(), [](const ExitCand& a, const ExitCand& b)
            {
                if (a.iPrio != b.iPrio)
                    return a.iPrio < b.iPrio;
                return a.flDistSq < b.flDistSq;
            });
            const int n = (std::min)(maxN, static_cast<int>(cands.size()));
            for (int i = 0; i < n; ++i)
                out.push_back(cands[i].pActor);
        }

        static SDK::FVector ExitOrigin(SDK::ASBZWheeledVehicle* pVan, SDK::ASBZPlayerCharacter* pLocal)
        {
            if (ActorOk(pVan))
                return ActorLocSeh(pVan);
            if (ActorOk(pLocal))
                return ActorLocSeh(pLocal);
            return {};
        }

        static int ArmLeaveVolumes(SDK::UWorld* pWorld, SDK::ASBZWheeledVehicle* pVan, SDK::ASBZPlayerCharacter* pLocal)
        {
            const SDK::FVector origin = ExitOrigin(pVan, pLocal);
            std::vector<ExitCand> cands;

            auto* cls = SDK::ASBZPlayerEscapeVolume::StaticClass();
            if (cls)
            {
                SDK::TArray<SDK::AActor*> list{};
                SDK::UGameplayStatics::GetAllActorsOfClass(pWorld, cls, &list);
                for (int i = 0; i < list.Num(); ++i)
                {
                    SDK::AActor* vol = list[i];
                    if (!ActorOk(vol))
                        continue;
                    cands.push_back({ vol, DistSq(ActorLocSeh(vol), origin), 1 });
                }
            }

            std::vector<SDK::AActor*> picked;
            TakeClosest(cands, kMaxExitPoints, picked);
            int n = 0;
            for (SDK::AActor* a : picked)
            {
                if (SetPlayerEscapeEnabledSeh(reinterpret_cast<SDK::ASBZPlayerEscapeVolume*>(a)))
                    ++n;
            }
            return n;
        }

        static int ActivateEscapeObjectives(SDK::UWorld* pWorld, SDK::ASBZWheeledVehicle* pVan, SDK::ASBZPlayerCharacter* pLocal)
        {
            const SDK::FVector origin = ExitOrigin(pVan, pLocal);
            std::vector<ExitCand> cands;
            auto* cls = SDK::ASBZObjective::StaticClass();
            if (!cls)
                return 0;

            SDK::TArray<SDK::AActor*> list{};
            SDK::UGameplayStatics::GetAllActorsOfClass(pWorld, cls, &list);
            for (int i = 0; i < list.Num(); ++i)
            {
                auto* obj = reinterpret_cast<SDK::ASBZObjective*>(list[i]);
                if (!ActorOk(obj))
                    continue;
                // Name only — FText.ToString crashed Green Exit.
                if (!LooksLikeEscapeName(SafeNameLower(obj)))
                    continue;
                cands.push_back({ obj, DistSq(ActorLocSeh(obj), origin), 1 });
            }
            std::vector<SDK::AActor*> picked;
            TakeClosest(cands, kMaxExitPoints, picked);
            int n = 0;
            for (SDK::AActor* a : picked)
            {
                if (ActivateObjectiveSeh(reinterpret_cast<SDK::ASBZObjective*>(a)))
                    ++n;
            }
            return n;
        }

        // Map markers — escape/leave named waypoints with a valid MarkerAsset only.
        // No FText, no "light all closest waypoints" fallback (that crashed).
        static int EnableEscapeWaypoints(SDK::UWorld* pWorld, SDK::ASBZWheeledVehicle* pVan, SDK::ASBZPlayerCharacter* pLocal)
        {
            const SDK::FVector origin = ExitOrigin(pVan, pLocal);
            std::vector<ExitCand> cands;
            auto* cls = SDK::ASBZWaypoint::StaticClass();
            if (!cls)
                return 0;

            SDK::TArray<SDK::AActor*> list{};
            SDK::UGameplayStatics::GetAllActorsOfClass(pWorld, cls, &list);
            for (int i = 0; i < list.Num(); ++i)
            {
                auto* wp = reinterpret_cast<SDK::ASBZWaypoint*>(list[i]);
                if (!ActorOk(wp))
                    continue;
                const std::string n = SafeNameLower(wp);
                if (!(LooksLikeEscapeName(n) || Contains(n, "exit") || Contains(n, "leave")))
                    continue;
                cands.push_back({ wp, DistSq(ActorLocSeh(wp), origin), 1 });
            }

            std::vector<SDK::AActor*> picked;
            TakeClosest(cands, kMaxExitPoints, picked);
            int n = 0;
            for (SDK::AActor* a : picked)
            {
                if (AddWaypointMarkerSeh(reinterpret_cast<SDK::ASBZWaypoint*>(a)))
                    ++n;
            }
            return n;
        }

        static void ArmLeaveZone(SDK::UWorld* pWorld, SDK::ASBZWheeledVehicle* pVan, SDK::ASBZPlayerCharacter* pLocal,
            int& bags, int& leave, int& objs, int& marks)
        {
            bags = 0;
            leave = 0;
            objs = 0;
            marks = 0;
            // Safest first (what worked): leave volumes + bag volumes + countdown.
            leave = ArmLeaveVolumes(pWorld, pVan, pLocal);
            bags = EnableEscapeBagVolumes(pWorld);
            ForceEscapeCountdown(pWorld);
            // Markers last — if these fault, SEH should swallow; volumes already armed.
            objs = ActivateEscapeObjectives(pWorld, pVan, pLocal);
            marks = EnableEscapeWaypoints(pWorld, pVan, pLocal);
        }

        static void FinalizeEscapeVan(SDK::UWorld* pWorld, SDK::ASBZPlayerCharacter* pLocal)
        {
            // Re-find van — never keep a raw pointer across the delay.
            std::vector<SDK::ASBZWheeledVehicle*> vans;
            FindEscapeVans(pWorld, vans);
            if (vans.empty() || !ActorOk(vans[0]))
            {
                g_sStatusVan = "finalize SKIP - van gone";
                return;
            }
            SDK::ASBZWheeledVehicle* pVan = vans[0];

            HandleVanArrivedSeh(pWorld, pVan);
            OpenVanDoorsSeh(pVan);
            MulticastVanDoorsSeh(pVan);

            int bags = 0, leave = 0, objs = 0, marks = 0;
            ArmLeaveZone(pWorld, pVan, pLocal, bags, leave, objs, marks);
            g_sStatusVan = "DONE - doors/arrive bags=" + std::to_string(bags)
                + " leave=" + std::to_string(leave)
                + " obj=" + std::to_string(objs)
                + " markers=" + std::to_string(marks);
        }

        static void DoVan(SDK::UWorld* pWorld, SDK::ASBZPlayerCharacter* pLocal)
        {
            (void)pLocal;
            Utils::LogDebug("[SpawnerTools] Van start");
            std::vector<SDK::ASBZTrafficSpline*> splines;
            CollectEscapeSplines(pWorld, splines);
            std::vector<SDK::ASBZWheeledVehicle*> vans;
            FindEscapeVans(pWorld, vans);
            if (vans.empty())
            {
                g_sStatusVan = "FAIL - no escape van in world";
                return;
            }
            SDK::ASBZWheeledVehicle* van = vans[0];
            auto* dest = FindNamedSpline(splines, "escapelower_in");
            if (!dest)
                dest = FindNamedSpline(splines, "escapelower");
            if (!dest)
                dest = FindNamedSpline(splines, "escapeupper_in");
            auto* start = FindNamedSpline(splines, "escape_flyout_1");
            if (!start)
                start = FindNamedSpline(splines, "flyout_1");
            if (!start)
                start = FindNamedSpline(splines, "escape_flyin_1");
            if (!start || !dest)
                PickBestPair(splines, start, dest);
            if (!start || !dest)
            {
                g_sStatusVan = "FAIL - missing start/dest spline";
                return;
            }
            if (!TryDrivePairSeh(pWorld, van, start, dest))
            {
                g_sStatusVan = "FAIL - SetAndDriveCustomVehicleRoute false/crash";
                return;
            }
            s_bVanFinalizePending = true;
            s_timeVanFinalize = std::chrono::steady_clock::now() + kVanFinalizeDelay;
            g_sStatusVan = "driving - finalize in ~2.8s";
        }

        static void DoGreenExit(SDK::UWorld* pWorld, SDK::ASBZPlayerCharacter* pLocal)
        {
            Utils::LogDebug("[SpawnerTools] GreenExit start");
            std::vector<SDK::ASBZWheeledVehicle*> vans;
            FindEscapeVans(pWorld, vans);
            SDK::ASBZWheeledVehicle* van = vans.empty() ? nullptr : vans[0];
            int bags = 0, leave = 0, objs = 0, marks = 0;
            ArmLeaveZone(pWorld, van, pLocal, bags, leave, objs, marks);
            g_sStatusExit = "armed leave=" + std::to_string(leave)
                + " bags=" + std::to_string(bags)
                + " obj=" + std::to_string(objs)
                + " markers=" + std::to_string(marks)
                + " (check map / green icons)";
        }

        static void DoMoney(SDK::UWorld* pWorld, SDK::ASBZPlayerController* pPC, SDK::ASBZPlayerCharacter* pLocal)
        {
            Utils::LogDebug("[SpawnerTools] Money start");
            std::vector<SDK::ASBZWheeledVehicle*> vans;
            FindEscapeVans(pWorld, vans);
            SDK::ASBZWheeledVehicle* van = vans.empty() ? nullptr : vans[0];
            int bags = 0, leave = 0, objs = 0, marks = 0;
            ArmLeaveZone(pWorld, van, pLocal, bags, leave, objs, marks);

            if (RequestMissionSuccessSeh(pPC))
            {
                g_sStatusMoney = "OK - RequestMissionSuccess (wait for results)";
                return;
            }
            if (RequestMissionEndSeh(pPC))
            {
                g_sStatusMoney = "OK - RequestMissionEnd(Success)";
                return;
            }
            g_sStatusMoney = "FAIL - no CheatManager (enable cheats / try Green exit)";
        }

        static void DrainEscapeTimer(SDK::UWorld* pWorld)
        {
            if (!s_bEscapeTimerZeroPending)
                return;
            if (std::chrono::steady_clock::now() < s_timeEscapeTimerZero)
                return;
            s_bEscapeTimerZeroPending = false;
            ZeroEscapeTimerSeh(pWorld);
        }
    }

    bool HasPendingWork()
    {
        return s_ePending != EPending::None || s_bVanFinalizePending || s_bEscapeTimerZeroPending;
    }

    void RequestMeth()
    {
        s_ePending = EPending::Meth;
        g_sStatusMeth = "queued...";
    }

    void RequestVan()
    {
        s_ePending = EPending::Van;
        g_sStatusVan = "queued...";
    }

    void RequestGreenExit()
    {
        s_ePending = EPending::GreenExit;
        g_sStatusExit = "queued...";
    }

    void RequestMoneyScreen()
    {
        s_ePending = EPending::Money;
        g_sStatusMoney = "queued...";
    }

    void OnPlayerControllerTick(
        SDK::UWorld* pGWorld,
        SDK::ASBZPlayerController* pLocalController,
        SDK::ASBZPlayerCharacter* pLocalPlayer)
    {
        if (!pGWorld)
            return;

        DrainEscapeTimer(pGWorld);

        if (s_bVanFinalizePending && std::chrono::steady_clock::now() >= s_timeVanFinalize)
        {
            s_bVanFinalizePending = false;
            FinalizeEscapeVan(pGWorld, pLocalPlayer);
        }

        if (s_ePending == EPending::None)
            return;

        const EPending action = s_ePending;
        s_ePending = EPending::None;

        if (!Cheat::g_bIsInGame)
        {
            const char* msg = "not in heist";
            if (action == EPending::Meth)
                g_sStatusMeth = msg;
            else if (action == EPending::Van)
                g_sStatusVan = msg;
            else if (action == EPending::GreenExit)
                g_sStatusExit = msg;
            else
                g_sStatusMoney = msg;
            return;
        }

        if (!ActorOk(pLocalPlayer) && action != EPending::Money)
        {
            const char* msg = "no local pawn";
            if (action == EPending::Meth)
                g_sStatusMeth = msg;
            else if (action == EPending::Van)
                g_sStatusVan = msg;
            else if (action == EPending::GreenExit)
                g_sStatusExit = msg;
            return;
        }

        switch (action)
        {
        case EPending::Meth:
            DoMeth(pGWorld);
            break;
        case EPending::Van:
            DoVan(pGWorld, pLocalPlayer);
            break;
        case EPending::GreenExit:
            DoGreenExit(pGWorld, pLocalPlayer);
            break;
        case EPending::Money:
            DoMoney(pGWorld, pLocalController, pLocalPlayer);
            break;
        default:
            break;
        }
    }
}
