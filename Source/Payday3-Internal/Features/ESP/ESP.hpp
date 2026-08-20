#pragma once

#include <imgui.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <cmath>
#include "../../Dumper-7/SDK.hpp"
#include "../../Utils/Logging.hpp"

namespace ESP
{
    // Shared draw colors (Visuals color pickers). Defaults match previous hardcoded look.
    struct Colors {
        ImU32 m_colBox = IM_COL32(255, 0, 0, 255);
        ImU32 m_colHealth = IM_COL32(142, 230, 11, 200);
        ImU32 m_colArmor = IM_COL32(64, 147, 255, 200);
        ImU32 m_colSkeleton = IM_COL32(255, 255, 0, 255);
        // ImGui highlight (corner brackets) when Outline is on — game Multicast_SetMarked stays separate.
        ImU32 m_colHighlight = IM_COL32(255, 80, 255, 220);
        // Loot ESP: keys vs money/bags vs chem (matches freecam: chem yellow, cash/bags white).
        ImU32 m_colKeyItems = IM_COL32(0, 255, 0, 255);
        ImU32 m_colMoney = IM_COL32(255, 255, 255, 255);
        ImU32 m_colChem = IM_COL32(255, 220, 0, 255);
        ImU32 m_colDetection = IM_COL32(255, 180, 40, 230);
        ImU32 m_colBagZone = IM_COL32(80, 220, 255, 230);
        ImU32 m_colSuspicious = IM_COL32(255, 160, 40, 255);
        ImU32 m_colFovCircle = IM_COL32(255, 0, 0, 100);
    };

    struct EnemyESP{
        bool m_bBox = false;
        bool m_bHealth = false;
        bool m_bArmor = false;
        bool m_bName = false;
        bool m_bFlags = true;
        bool m_bSkeleton = false;
        bool m_bOutline = true;
    };

    struct CivilianESP{
        bool m_bBox = false;
        bool m_bFlags = false;
        bool m_bSkeleton = false;
        bool m_bOutline = false;
        bool m_bOnlyWhenSpecial = false;
    };

    struct Config {
        bool bESP = false;

        EnemyESP m_stNormalEnemies{};
        EnemyESP m_stSpecialEnemies{};
        CivilianESP m_stCivilians{};
        Colors m_colors{};

        // Stealth / heist helpers (independent of Enable ESP where noted)
        bool bPagerHud = false;         // AnswerPagerCount from heist GameState
        bool bBagZones = false;         // ASBZBagTriggerVolume drop-off markers

        bool bDebugSkeleton = false;
        bool bDebugDrawBoneIndices = false;
        bool bDebugDrawBoneNames = false;
        bool bDebugSkeletonDrawBoneIndices = false;
        bool bDebugSkeletonDrawBoneNames = false;
        bool bDebugESP = false;

        bool bDrawFovCircle = false;
    };

    inline Config& GetConfig() {
        static Config config{};
        return config;
    }

    void Render(SDK::UWorld* pGWorld, SDK::APlayerController* pPlayerController);
    void RenderDebugESP(SDK::ULevel* pPersistentLevel, SDK::APlayerController* pPlayerController);
}

namespace LootESP
{
    // Loot ESP — keycards / RFID / cash / bags / chem / USB / etc.
    struct Config {
        bool bLootESP = false;
        bool bOutline = false; // ImGui corners — extra, hitchy
        bool bStrongGlow = false; // F4-style through-wall glow — extra, hitchy
        bool bLabels = false;
    };

    inline Config& GetConfig() {
        static Config config{};
        return config;
    }

    // Game-thread, parent-only loot glow. Must NOT run from Present.
    void TickCheapGlow(SDK::UWorld* pGWorld);
}
