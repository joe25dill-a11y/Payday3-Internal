#include "SilentKill.hpp"
#include "../../Menu.hpp"

#include <Windows.h>
#include <cctype>
#include <chrono>
#include <string>
#include <unordered_set>

namespace Cheat::SilentKill
{
    std::string g_sDebugStatus = "SilentDespawn off";

    namespace
    {
        constexpr int kMaxPerWave = 80;
        // Was 2500ms (Lua Num* lockout). Checkbox mode can re-scan faster for new spawns.
        constexpr auto kWaveGap = std::chrono::milliseconds(900);

        static std::unordered_set<uintptr_t> s_setDone{};
        static std::chrono::steady_clock::time_point s_timeNextWave{};
        static int s_iKilledTotal = 0;

        static std::string ToLower(std::string s)
        {
            for (char& c : s)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        }

        static bool Contains(const std::string& hay, const char* needle)
        {
            return hay.find(needle) != std::string::npos;
        }

        static std::string ActorName(SDK::AActor* pActor)
        {
            if (!pActor)
                return {};
            if (pActor->Class)
                return pActor->Class->GetName() + " " + pActor->GetName();
            return pActor->GetName();
        }

        static bool ActorOk(SDK::AActor* pActor)
        {
            return pActor && pActor->Class && !pActor->IsActorBeingDestroyed();
        }

        // Mission NPCs — never bury (Houston Breakout, FWB inside man, etc.)
        static bool IsProtectedNpc(const std::string& low)
        {
            if (low.empty() || Contains(low, "default__"))
                return true;
            static const char* keys[] = {
                "houston", "insideman", "inside_man", "inside man", "insider",
                "informant", "storynpc", "story_character", "storycharacter",
                "civilian", "basecivilian", "crewaicharacter", "sbzcrewaicharacter",
                "friendlyai", "playercharacter", "sbzplayercharacter", "ch_player",
                "hostage", "vip", "teller", "banker", "witness", "contact",
                "negotiator", "escort", "managercharacter", "inside_character",
                "bp_houston", "bp_inside", "chus_houston", "chus_character",
            };
            for (const char* k : keys)
            {
                if (Contains(low, k))
                    return true;
            }
            return false;
        }

        static bool UnregisterCopRaw(SDK::ASBZAICharacter* pAI)
        {
            __try
            {
                if (!pAI)
                    return false;
                pAI->bIsPagerEnabled = false;
                pAI->bIsPagerEnabledOnce = false;
                pAI->bIsPendingPagerEnabled = false;
                pAI->bIsPagerSnatched = true;
                pAI->bIsPagerScrambled = true;
                pAI->PagerTriggeredCount = 0;
                pAI->PagerData = nullptr;
                if (pAI->DeadBodyPOIInstance && !pAI->DeadBodyPOIInstance->IsActorBeingDestroyed())
                    pAI->DeadBodyPOIInstance->K2_DestroyActor();
                pAI->DeadBodyPOIInstance = nullptr;
                pAI->DeadBodyPOIClass = nullptr;
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool BurySafeRaw(SDK::AActor* pActor)
        {
            __try
            {
                if (!pActor)
                    return false;
                const SDK::FVector loc = pActor->K2_GetActorLocation();
                SDK::FHitResult hit{};
                const SDK::FVector buried{
                    loc.X,
                    loc.Y,
                    loc.Z - 50000.f
                };
                pActor->K2_SetActorLocation(buried, false, &hit, true);
                pActor->SetActorHiddenInGame(true);
                pActor->SetActorEnableCollision(false);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool CorpseThenBuryRaw(SDK::ASBZAICharacter* pAI)
        {
            __try
            {
                if (!pAI || !pAI->Class || pAI->IsActorBeingDestroyed())
                    return false;

                // Drop keycards/RFID before bury so Grab Access / Num/ can grant them
                pAI->Multicast_DropAttachedLoot();
                if (pAI->AttachedLoot && !pAI->AttachedLoot->IsActorBeingDestroyed())
                {
                    auto* pLoot = pAI->AttachedLoot;
                    const SDK::FVector loc = pAI->K2_GetActorLocation();
                    SDK::FHitResult hit{};
                    const SDK::FVector drop{ loc.X, loc.Y, loc.Z + 20.f };
                    pLoot->K2_SetActorLocation(drop, false, &hit, true);
                    pLoot->SetActorHiddenInGame(false);
                    pLoot->SetActorEnableCollision(true);
                    pAI->AttachedLoot = nullptr;
                }

                UnregisterCopRaw(pAI);

                bool bKilled = false;
                if (auto* pAttr = pAI->AttributeSet)
                {
                    pAttr->Health.CurrentValue = 0.f;
                    pAttr->Multicast_SetHealth(0.f);
                    pAttr->Armor.CurrentValue = 0.f;
                    bKilled = true;
                }

                UnregisterCopRaw(pAI);
                BurySafeRaw(pAI);
                return bKilled;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static int DestroyDeadBodyPOIs(SDK::UWorld* pGWorld)
        {
            int n = 0;
            SDK::TArray<SDK::AActor*> list{};
            SDK::UGameplayStatics::GetAllActorsOfClass(
                pGWorld, SDK::ASBZAIPointOfInterestDeadBody::StaticClass(), &list);
            for (int i = 0; i < list.Num(); ++i)
            {
                auto* p = list[i];
                if (!ActorOk(p))
                    continue;
                __try
                {
                    p->K2_DestroyActor();
                    ++n;
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                }
            }
            return n;
        }

        static int RunWave(SDK::UWorld* pGWorld)
        {
            int killed = 0;
            int skipped = 0;

            SDK::UClass* pCopClass = SDK::ACH_BaseCop_C::StaticClass();
            if (!pCopClass)
                return 0;

            SDK::TArray<SDK::AActor*> list{};
            SDK::UGameplayStatics::GetAllActorsOfClass(pGWorld, pCopClass, &list);

            for (int i = 0; i < list.Num() && killed < kMaxPerWave; ++i)
            {
                auto* pActor = list[i];
                if (!ActorOk(pActor))
                    continue;

                const uintptr_t key = reinterpret_cast<uintptr_t>(pActor);
                if (s_setDone.contains(key))
                {
                    ++skipped;
                    continue;
                }

                const std::string low = ToLower(ActorName(pActor));
                if (IsProtectedNpc(low))
                {
                    ++skipped;
                    continue;
                }

                // Extra safety: never touch civ / crew class even if somehow in list
                if (pActor->IsA(SDK::ACH_BaseCivilian_C::StaticClass()))
                {
                    ++skipped;
                    continue;
                }
                if (pActor->IsA(SDK::ASBZAICrewCharacter::StaticClass()))
                {
                    ++skipped;
                    continue;
                }
                if (pActor->IsA(SDK::ASBZPlayerCharacter::StaticClass()))
                {
                    ++skipped;
                    continue;
                }

                auto* pAI = reinterpret_cast<SDK::ASBZAICharacter*>(pActor);
                if (!CorpseThenBuryRaw(pAI))
                    continue;

                s_setDone.insert(key);
                ++killed;
            }

            DestroyDeadBodyPOIs(pGWorld);
            return killed;
        }
    }

    void OnPlayerControllerTick(
        SDK::UWorld* pGWorld,
        SDK::ASBZPlayerController* /*pLocalController*/,
        SDK::ASBZPlayerCharacter* pLocalPlayer)
    {
        if (!CheatConfig::Get().m_misc.m_bSilentKillCops)
        {
            if (!s_setDone.empty() || g_sDebugStatus != "SilentDespawn off")
            {
                s_setDone.clear();
                s_iKilledTotal = 0;
                g_sDebugStatus = "SilentDespawn off";
            }
            return;
        }

        if (!pGWorld || !pLocalPlayer)
            return;

        if (!Cheat::g_bIsInGame)
        {
            s_setDone.clear();
            g_sDebugStatus = "SilentDespawn ON — wait for heist";
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now < s_timeNextWave)
            return;
        s_timeNextWave = now + kWaveGap;

        const int n = RunWave(pGWorld);
        if (n > 0)
            s_iKilledTotal += n;

        g_sDebugStatus = "SilentDespawn cops=" + std::to_string(s_iKilledTotal)
            + (n > 0 ? (" (+" + std::to_string(n) + ")") : "")
            + " (no Houston/inside)";
    }
}
