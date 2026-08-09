#include "CarryBags.hpp"
#include "../../Menu.hpp"

#include <Windows.h>
#include <chrono>
#include <string>

namespace Cheat::CarryBags
{
    std::string g_sStatus = "Carry More Bags off";

    namespace
    {
        constexpr int32_t kCarryCap = 50;
        constexpr auto kRescanGap = std::chrono::milliseconds(1500);

        static bool s_bWasOn = false;
        static std::chrono::steady_clock::time_point s_timeScan{};

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

        static bool BumpCarrySeh(SDK::ASBZCharacter* pChar)
        {
            __try
            {
                if (!pChar)
                    return false;
                if (pChar->MaxCarryBagCount < kCarryCap)
                    pChar->MaxCarryBagCount = kCarryCap;
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }
    }

    void OnPlayerControllerTick(
        SDK::UWorld* pGWorld,
        SDK::ASBZPlayerController* /*pLocalController*/,
        SDK::ASBZPlayerCharacter* pLocalPlayer)
    {
        const bool bOn = CheatConfig::Get().m_misc.m_bCarryMoreBags;
        if (!bOn)
        {
            if (s_bWasOn)
            {
                s_bWasOn = false;
                g_sStatus = "Carry More Bags off";
            }
            return;
        }

        s_bWasOn = true;
        if (!pGWorld || !ActorOkSeh(pLocalPlayer))
        {
            g_sStatus = "Carry More Bags ON — wait for heist";
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - s_timeScan < kRescanGap && g_sStatus.find("you=") != std::string::npos)
            return;
        s_timeScan = now;

        int you = 0;
        int ai = 0;

        if (BumpCarrySeh(pLocalPlayer))
            you = 1;

        if (auto* cls = SDK::ASBZAICrewCharacter::StaticClass())
        {
            SDK::TArray<SDK::AActor*> crew{};
            SDK::UGameplayStatics::GetAllActorsOfClass(pGWorld, cls, &crew);
            for (int i = 0; i < crew.Num(); ++i)
            {
                auto* pCrew = reinterpret_cast<SDK::ASBZAICrewCharacter*>(crew[i]);
                if (!ActorOkSeh(pCrew))
                    continue;
                if (BumpCarrySeh(pCrew))
                    ++ai;
            }
        }

        g_sStatus = "Carry More Bags ON (cap " + std::to_string(kCarryCap)
            + ") you=" + std::to_string(you)
            + " AI=" + std::to_string(ai);
    }
}
