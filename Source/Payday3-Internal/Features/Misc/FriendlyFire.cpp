#include "FriendlyFire.hpp"
#include "../../Menu.hpp"
#include "../../Utils/Logging.hpp"

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

#undef min
#undef max

namespace Cheat::FriendlyFire
{
    std::string g_sDebugStatus = "FF off";

    namespace
    {
        constexpr float kShotDamage = 200.f;
        constexpr auto kHitCooldown = std::chrono::milliseconds(50);

        static int s_iPlayers = 0;
        static int s_iHits = 0;
        static const char* s_szHost = "?";

        static bool IsPlayerPawn(SDK::UObject* pObj)
        {
            return pObj && pObj->IsA(SDK::ASBZPlayerCharacter::StaticClass());
        }

        static bool IsAlivePlayer(SDK::ASBZPlayerCharacter* pPlayer)
        {
            return IsPlayerPawn(pPlayer)
                && pPlayer->PlayerAttributeSet
                && pPlayer->PlayerAttributeSet->Health.CurrentValue > 1.f;
        }

        static bool IsLobbyHost(SDK::UWorld* pGWorld, SDK::ASBZPlayerCharacter* pLocal)
        {
            if (pLocal && pLocal->HasAuthority())
            {
                s_szHost = "authority";
                return true;
            }
            if (pGWorld && SDK::UKismetSystemLibrary::IsServer(pGWorld))
            {
                s_szHost = "IsServer";
                return true;
            }
            if (pGWorld && pGWorld->OwningGameInstance
                && pGWorld->OwningGameInstance->IsA(SDK::USBZGameInstance::StaticClass()))
            {
                auto* pGI = reinterpret_cast<SDK::USBZGameInstance*>(pGWorld->OwningGameInstance);
                if (pGI->AccelByteUser && pGI->AccelByteUser->UserActivity.bIsHost)
                {
                    s_szHost = "bIsHost";
                    return true;
                }
            }
            s_szHost = "FORCE";
            return true;
        }

        static void CollectPlayers(
            SDK::UWorld* pGWorld,
            SDK::USBZWorldRuntime* pWorldRuntime,
            SDK::ASBZPlayerCharacter* pLocal,
            std::vector<SDK::ASBZPlayerCharacter*>& out)
        {
            out.clear();

            if (pGWorld && pGWorld->GameState)
            {
                auto& aPS = pGWorld->GameState->PlayerArray;
                for (int32_t i = 0; i < aPS.Num(); ++i)
                {
                    if (!aPS.IsValidIndex(i) || !aPS[i] || !aPS[i]->PawnPrivate)
                        continue;
                    if (!IsPlayerPawn(aPS[i]->PawnPrivate))
                        continue;
                    auto* p = reinterpret_cast<SDK::ASBZPlayerCharacter*>(aPS[i]->PawnPrivate);
                    if (std::find(out.begin(), out.end(), p) == out.end())
                        out.push_back(p);
                }
            }

            if (pWorldRuntime && pWorldRuntime->AllPawns)
            {
                auto& aPawns = pWorldRuntime->AllPawns->Objects;
                for (int32_t i = 0; i < aPawns.Num(); ++i)
                {
                    if (!aPawns.IsValidIndex(i) || !IsPlayerPawn(aPawns[i]))
                        continue;
                    auto* p = reinterpret_cast<SDK::ASBZPlayerCharacter*>(aPawns[i]);
                    if (std::find(out.begin(), out.end(), p) == out.end())
                        out.push_back(p);
                }
            }

            if (pLocal && std::find(out.begin(), out.end(), pLocal) == out.end())
                out.push_back(pLocal);

            s_iPlayers = static_cast<int>(out.size());
        }

        static bool CanHit(int32_t iVictim)
        {
            static std::unordered_map<int32_t, std::chrono::steady_clock::time_point> s_last;
            const auto now = std::chrono::steady_clock::now();
            auto& t = s_last[iVictim];
            if (now - t < kHitCooldown)
                return false;
            t = now;
            return true;
        }

        static void ApplyDamage(SDK::ASBZPlayerCharacter* pVictim, float flDamage)
        {
            if (!IsAlivePlayer(pVictim))
                return;

            auto* pAttr = pVictim->PlayerAttributeSet;
            pAttr->IncomingDamageMultiplier.BaseValue = 1.f;
            pAttr->IncomingDamageMultiplier.CurrentValue = 1.f;

            float flArmor = pAttr->Armor.CurrentValue;
            float flHealth = pAttr->Health.CurrentValue;
            float flLeft = flDamage;

            if (flArmor > 0.f)
            {
                const float flNewArmor = (std::max)(0.f, flArmor - flLeft);
                flLeft -= (flArmor - flNewArmor);
                pAttr->Armor.BaseValue = flNewArmor;
                pAttr->Armor.CurrentValue = flNewArmor;
                pAttr->Multicast_SetArmor(flNewArmor);
            }

            if (flLeft > 0.f)
            {
                const float flNewHealth = (std::max)(0.f, flHealth - flLeft);
                pAttr->Health.BaseValue = flNewHealth;
                pAttr->Health.CurrentValue = flNewHealth;
                pAttr->Multicast_SetHealth(flNewHealth);

                if (flNewHealth <= 1.f && pVictim->SBZPlayerState)
                    pVictim->SBZPlayerState->Multicast_SetDefeatState(SDK::EPD3DefeatState::Downed);
            }

            ++s_iHits;
        }
    }

    void NotifyRemoteShot(SDK::UObject* /*pObject*/)
    {
        // PARKED — ProcessEvent fire hooks crashed the host.
    }

    void OnPossibleShot(SDK::UObject* /*pObject*/) {}
    void ProcessPendingHits() {}

    void OnPlayerControllerTick(
        SDK::UWorld* pGWorld,
        SDK::USBZWorldRuntime* pWorldRuntime,
        SDK::ASBZPlayerController* /*pLocalController*/,
        SDK::ASBZPlayerCharacter* pLocalPlayer)
    {
        if (!CheatConfig::Get().m_misc.m_bFriendlyFire)
        {
            g_sDebugStatus = "FF off";
            return;
        }

        if (!pGWorld || !pLocalPlayer)
        {
            g_sDebugStatus = "FF: no world/player";
            return;
        }

        if (!IsLobbyHost(pGWorld, pLocalPlayer))
        {
            g_sDebugStatus = "FF: not host";
            return;
        }

        std::vector<SDK::ASBZPlayerCharacter*> aPlayers;
        CollectPlayers(pGWorld, pWorldRuntime, pLocalPlayer, aPlayers);

        const bool bLmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

        // Clear your god block (so later mutual work has a chance).
        if (pLocalPlayer->PlayerAttributeSet)
        {
            pLocalPlayer->PlayerAttributeSet->IncomingDamageMultiplier.BaseValue = 1.f;
            pLocalPlayer->PlayerAttributeSet->IncomingDamageMultiplier.CurrentValue = 1.f;
        }

        // HOST → friends only (stable path). Hold LMB = damage all other players.
        // Friend → you is PARKED (fire RPC hooks crashed).
        if (bLmb)
        {
            for (auto* pOther : aPlayers)
            {
                if (!pOther || pOther == pLocalPlayer || !IsAlivePlayer(pOther))
                    continue;
                if (!CanHit(pOther->Index))
                    continue;
                ApplyDamage(pOther, kShotDamage);
            }
        }

        g_sDebugStatus = std::format(
            "SAFE host={} players={} hits={} LMB={} (you->them only)",
            s_szHost, s_iPlayers, s_iHits, bLmb ? 1 : 0);
    }
}
