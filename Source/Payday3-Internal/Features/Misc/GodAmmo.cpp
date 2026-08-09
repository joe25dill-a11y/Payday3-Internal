#include "GodAmmo.hpp"
#include "../../Menu.hpp"

#include <Windows.h>
#include <chrono>
#include <algorithm>

namespace Cheat::GodAmmo
{
    std::string g_sStatusGod = "God Mode off";
    std::string g_sStatusAmmo = "Infinite Ammo off";
    std::string g_sStatusInstaKill = "Insta Kill off";

    namespace
    {
        constexpr auto kGodDmgMcGap = std::chrono::milliseconds(1000);
        constexpr auto kGodHealMcGap = std::chrono::milliseconds(1500);
        constexpr auto kAmmoReapplyGap = std::chrono::milliseconds(2000);
        constexpr auto kAmmoRefillGap = std::chrono::milliseconds(200);
        constexpr float kMagFallback = 60.f;

        static bool s_bWasGod = false;
        static bool s_bWasAmmo = false;
        static bool s_bTrueInf = false;
        static std::chrono::steady_clock::time_point s_timeGodDmgMc{};
        static std::chrono::steady_clock::time_point s_timeGodHealMc{};
        static std::chrono::steady_clock::time_point s_timeAmmoReapply{};
        static std::chrono::steady_clock::time_point s_timeAmmoRefill{};

        static void SetDebuggerFlags(bool bGod, bool bAmmo)
        {
            __try
            {
                auto* pDbg = SDK::USBZDebuggerOptions::GetDefaultObj();
                if (!pDbg)
                    return;
                pDbg->PlayerOptions.bIsGod = bGod;
                pDbg->PlayerOptions.bIsInfiniteAmmo = bAmmo;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }

        static SDK::USBZCheatManager* EnsureCheatManager(SDK::ASBZPlayerController* pPC)
        {
            __try
            {
                if (!pPC)
                    return nullptr;
                if (pPC->CheatManager && pPC->CheatManager->IsA(SDK::USBZCheatManager::StaticClass()))
                    return reinterpret_cast<SDK::USBZCheatManager*>(pPC->CheatManager);

                pPC->EnableCheats();
                if (pPC->CheatManager && pPC->CheatManager->IsA(SDK::USBZCheatManager::StaticClass()))
                    return reinterpret_cast<SDK::USBZCheatManager*>(pPC->CheatManager);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
            return nullptr;
        }

        static void ApplyTrueInfiniteAmmo(SDK::ASBZPlayerController* pPC, SDK::ASBZPlayerState* pPS, bool bOn)
        {
            SetDebuggerFlags(s_bWasGod || CheatConfig::Get().m_misc.m_bGodMode, bOn);

            __try
            {
                if (pPS)
                    pPS->Client_CheatSetInfiniteAmmo(bOn);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }

            __try
            {
                if (auto* pCM = EnsureCheatManager(pPC))
                    pCM->SetInfiniteAmmo(bOn, 0);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }

        static void ClearGodSoft(SDK::USBZPlayerAttributeSet* pAttr)
        {
            __try
            {
                if (!pAttr)
                    return;
                pAttr->IncomingDamageMultiplier.CurrentValue = 1.f;
                pAttr->Multicast_SetIncomingDamageMultiplier(1.f);
                pAttr->HealthDamageMultiplier.CurrentValue = 1.f;
                pAttr->ArmorDamageMultiplier.CurrentValue = 1.f;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }

        static void PushGod(SDK::USBZPlayerAttributeSet* pAttr, const std::chrono::steady_clock::time_point& now)
        {
            __try
            {
                if (!pAttr)
                    return;

                pAttr->IncomingDamageMultiplier.CurrentValue = 0.f;
                pAttr->HealthDamageMultiplier.CurrentValue = 0.f;
                pAttr->ArmorDamageMultiplier.CurrentValue = 0.f;

                if (now - s_timeGodDmgMc >= kGodDmgMcGap)
                {
                    pAttr->Multicast_SetIncomingDamageMultiplier(0.f);
                    s_timeGodDmgMc = now;
                }

                const float hpMax = (std::max)(1.f, pAttr->HealthMax.CurrentValue);
                const float arMax = (std::max)(0.f, pAttr->ArmorMax.CurrentValue);
                const bool bNeedHeal = pAttr->Health.CurrentValue < hpMax * 0.95f
                    || (arMax > 0.f && pAttr->Armor.CurrentValue < arMax * 0.95f);

                if (bNeedHeal)
                {
                    pAttr->Health.CurrentValue = hpMax;
                    if (arMax > 0.f)
                        pAttr->Armor.CurrentValue = arMax;

                    if (now - s_timeGodHealMc >= kGodHealMcGap)
                    {
                        pAttr->Multicast_SetHealth(hpMax);
                        if (arMax > 0.f)
                            pAttr->Multicast_SetArmor(arMax);
                        s_timeGodHealMc = now;
                    }
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }

        static float MagOr(float cur, float fallback)
        {
            if (cur > 1.f)
                return cur;
            return fallback;
        }

        static void TopSlot(
            SDK::USBZPlayerAttributeSet* pAttr,
            SDK::FGameplayAttributeData& loaded, float& loadedServer,
            SDK::FGameplayAttributeData& inv, float& invServer,
            int slot)
        {
            float mag = MagOr(loadedServer, MagOr(loaded.CurrentValue, kMagFallback));
            if (mag < 1.f)
                mag = kMagFallback;
            float stock = MagOr(invServer, MagOr(inv.CurrentValue, mag * 4.f));
            if (stock < mag)
                stock = mag * 4.f;

            const float cur = loadedServer > 0.f ? loadedServer : loaded.CurrentValue;
            if (cur > 0.f && cur >= mag * 0.95f)
                return;

            loaded.CurrentValue = mag;
            loadedServer = mag;
            inv.CurrentValue = stock;
            invServer = stock;
            if (slot == 1)
            {
                pAttr->Multicast_SetPrimaryEquippableAmmoLoaded(mag);
                pAttr->Multicast_SetPrimaryEquippableAmmoInventory(stock);
            }
            else if (slot == 2)
            {
                pAttr->Multicast_SetSecondaryEquippableAmmoLoaded(mag);
                pAttr->Multicast_SetSecondaryEquippableAmmoInventory(stock);
            }
            else
            {
                pAttr->Multicast_SetTertiaryEquippableAmmoLoaded(mag);
                pAttr->Multicast_SetTertiaryEquippableAmmoInventory(stock);
            }
        }

        // Low-only refill — safety net if true-inf doesn't stick on retail.
        static void RefillMagsIfLow(SDK::USBZPlayerAttributeSet* pAttr)
        {
            __try
            {
                if (!pAttr)
                    return;
                TopSlot(pAttr, pAttr->PrimaryEquippableAmmoLoaded, pAttr->PrimaryEquippableAmmoLoadedServer,
                    pAttr->PrimaryEquippableAmmoInventory, pAttr->PrimaryEquippableAmmoInventoryServer, 1);
                TopSlot(pAttr, pAttr->SecondaryEquippableAmmoLoaded, pAttr->SecondaryEquippableAmmoLoadedServer,
                    pAttr->SecondaryEquippableAmmoInventory, pAttr->SecondaryEquippableAmmoInventoryServer, 2);
                TopSlot(pAttr, pAttr->TertiaryEquippableAmmoLoaded, pAttr->TertiaryEquippableAmmoLoadedServer,
                    pAttr->TertiaryEquippableAmmoInventory, pAttr->TertiaryEquippableAmmoInventoryServer, 3);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }
    }

    void OnPlayerControllerTick(
        SDK::UWorld* /*pGWorld*/,
        SDK::ASBZPlayerController* pLocalController,
        SDK::ASBZPlayerCharacter* pLocalPlayer)
    {
        const bool bGod = CheatConfig::Get().m_misc.m_bGodMode;
        const bool bAmmo = CheatConfig::Get().m_misc.m_bInfiniteAmmo;
        const auto now = std::chrono::steady_clock::now();

        // Insta Kill is FireData-patched in Features.cpp (TraceurXu InstantKill style).
        if (!CheatConfig::Get().m_misc.m_bInstaKill)
            g_sStatusInstaKill = "Insta Kill off";

        if (!bGod && !bAmmo)
        {
            if (s_bWasGod || s_bWasAmmo)
            {
                SetDebuggerFlags(false, false);
                if (s_bWasAmmo && pLocalController)
                {
                    SDK::ASBZPlayerState* pPS = pLocalPlayer ? pLocalPlayer->SBZPlayerState : nullptr;
                    ApplyTrueInfiniteAmmo(pLocalController, pPS, false);
                }
                if (s_bWasGod && pLocalPlayer && pLocalPlayer->PlayerAttributeSet)
                    ClearGodSoft(pLocalPlayer->PlayerAttributeSet);
                s_bWasGod = false;
                s_bWasAmmo = false;
                s_bTrueInf = false;
                g_sStatusGod = "God Mode off";
                g_sStatusAmmo = "Infinite Ammo off";
            }
            return;
        }

        if (!pLocalPlayer || !pLocalController)
            return;

        auto* pAttr = pLocalPlayer->PlayerAttributeSet;
        auto* pPS = pLocalPlayer->SBZPlayerState;

        // ---- God ----
        if (bGod)
        {
            SetDebuggerFlags(true, bAmmo && s_bTrueInf);
            PushGod(pAttr, now);
            s_bWasGod = true;
            g_sStatusGod = "God Mode ON (dmg×0 + HP/armor top-up)";
        }
        else if (s_bWasGod)
        {
            SetDebuggerFlags(false, bAmmo && s_bTrueInf);
            ClearGodSoft(pAttr);
            s_bWasGod = false;
            g_sStatusGod = "God Mode off";
        }

        // ---- Infinite ammo ----
        if (bAmmo)
        {
            if (!s_bWasAmmo || now - s_timeAmmoReapply >= kAmmoReapplyGap)
            {
                ApplyTrueInfiniteAmmo(pLocalController, pPS, true);
                s_bTrueInf = true;
                s_timeAmmoReapply = now;
            }

            if (now - s_timeAmmoRefill >= kAmmoRefillGap)
            {
                RefillMagsIfLow(pAttr);
                s_timeAmmoRefill = now;
            }

            s_bWasAmmo = true;
            g_sStatusAmmo = s_bTrueInf
                ? "Infinite Ammo ON (true-inf + refill backup)"
                : "Infinite Ammo ON (refill)";
        }
        else if (s_bWasAmmo)
        {
            ApplyTrueInfiniteAmmo(pLocalController, pPS, false);
            s_bWasAmmo = false;
            s_bTrueInf = false;
            g_sStatusAmmo = "Infinite Ammo off";
        }
    }
}
