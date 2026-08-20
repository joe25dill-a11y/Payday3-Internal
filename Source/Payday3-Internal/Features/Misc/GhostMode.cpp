#include "GhostMode.hpp"
#include "../../Menu.hpp"
#include "../../Utils/Logging.hpp"

#include <Windows.h>
#include <chrono>
#include <string>

namespace Cheat::GhostMode
{
    std::string g_sStatus = "Ghost Mode off";

    namespace
    {
        constexpr auto kCamBlindGap = std::chrono::milliseconds(1200);
        constexpr auto kCheatReapplyGap = std::chrono::milliseconds(1500);
        constexpr auto kDetectGap = std::chrono::milliseconds(250);
        constexpr auto kIllegalScrubGap = std::chrono::milliseconds(150);

        static bool s_bWasOn = false;
        static std::chrono::steady_clock::time_point s_timeNextCam{};
        static std::chrono::steady_clock::time_point s_timeNextCheat{};
        static std::chrono::steady_clock::time_point s_timeNextDetect{};
        static std::chrono::steady_clock::time_point s_timeNextIllegal{};

        // SEH-only helpers (no C++ objects with destructors).
        static SDK::USBZCheatManager* EnsureCheatManagerSeh(SDK::ASBZPlayerController* pPC)
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

        static void ApplyCheatsSeh(SDK::USBZCheatManager* pCM, bool bOn)
        {
            if (!pCM)
                return;
            __try { pCM->SetInvisiblePlayer(bOn, 0); }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
            __try { pCM->SetInaudiblePlayer(bOn, 0); }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
            __try { pCM->PerceptionOnAllAI(!bOn); }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }

        // Same path as working Lua F11 — zero local visual detection multipliers.
        static bool ApplyLocalDetectionSeh(SDK::ASBZPlayerCharacter* pLocal, bool bOn)
        {
            __try
            {
                if (!pLocal || !pLocal->Class)
                    return false;

                if (bOn)
                {
                    pLocal->VisualDetectionMultiplierStanding = 0.f;
                    pLocal->VisualDetectionMultiplierCrouched = 0.f;
                    pLocal->VisualDetectionMultiplierSprinting = 0.f;
                }
                else
                {
                    // Restore sane defaults if we turned them off.
                    if (pLocal->VisualDetectionMultiplierStanding <= 0.f)
                        pLocal->VisualDetectionMultiplierStanding = 1.f;
                    if (pLocal->VisualDetectionMultiplierCrouched <= 0.f)
                        pLocal->VisualDetectionMultiplierCrouched = 1.f;
                    if (pLocal->VisualDetectionMultiplierSprinting <= 0.f)
                        pLocal->VisualDetectionMultiplierSprinting = 1.f;
                }

                // Clear active detector targets (v2 "No Detection" path).
                auto& dets = pLocal->VisualDetectors;
                for (int i = 0; i < dets.Num(); ++i)
                {
                    auto* det = dets[i];
                    if (!det)
                        continue;
                    det->bMarkAsCriminalOnSearch = false;
                    det->bShouldDisplayDetectionBuildup = false;
                    det->IllegalActionGracePeriod = 999999.f;
                    det->bOnlyDetectMovement = true;
                    auto& vals = det->EnemyDetectionValue;
                    for (int e = 0; e < vals.Num(); ++e)
                        vals[e].Target = nullptr;
                }
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static int BlindCamerasSeh(SDK::UWorld* pGWorld)
        {
            if (!pGWorld)
                return 0;

            SDK::TArray<SDK::AActor*> cams{};
            __try
            {
                SDK::UGameplayStatics::GetAllActorsOfClass(
                    pGWorld, SDK::ASBZSecurityCamera::StaticClass(), &cams);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return 0;
            }

            int n = 0;
            for (int i = 0; i < cams.Num(); ++i)
            {
                auto* pCam = reinterpret_cast<SDK::ASBZSecurityCamera*>(cams[i]);
                __try
                {
                    if (!pCam || !pCam->Class || pCam->IsActorBeingDestroyed())
                        continue;
                    pCam->SightRadius = 0.f;
                    pCam->PeripheralVisionAngleDegrees = 0.f;
                    ++n;
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                }
            }
            return n;
        }

        // Scrub AI controllers' detection components targeting us.
        static void ScrubWorldDetectorsSeh(SDK::UWorld* pGWorld, SDK::ASBZPlayerCharacter* pLocal, bool bGhostOn)
        {
            if (!pGWorld || !pLocal)
                return;

            SDK::TArray<SDK::AActor*> controllers{};
            __try
            {
                SDK::UGameplayStatics::GetAllActorsOfClass(
                    pGWorld, SDK::ASBZAIController::StaticClass(), &controllers);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return;
            }

            for (int i = 0; i < controllers.Num(); ++i)
            {
                auto* pAC = reinterpret_cast<SDK::ASBZAIController*>(controllers[i]);
                __try
                {
                    if (!pAC || !pAC->Class)
                        continue;
                    auto* det = pAC->VisualDetectionComponent;
                    if (!det)
                        continue;
                    if (bGhostOn)
                    {
                        det->bMarkAsCriminalOnSearch = false;
                        det->bShouldDisplayDetectionBuildup = false;
                        det->IllegalActionGracePeriod = 999999.f;
                        det->bOnlyDetectMovement = true;
                        auto& vals = det->EnemyDetectionValue;
                        for (int e = 0; e < vals.Num(); ++e)
                        {
                            if (vals[e].Target == reinterpret_cast<SDK::AActor*>(pLocal))
                                vals[e].Target = nullptr;
                        }
                    }
                    else
                    {
                        // Undo ghost-only tweaks so cops behave normally again.
                        det->bOnlyDetectMovement = false;
                    }
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                }
            }
        }

        static void ClearIllegalFlagSeh(SDK::USBZBaseInteractableComponent* pBase)
        {
            if (!pBase)
                return;
            __try
            {
                // IsInteractionIllegal() reads per-mode data — this is what cops use for "observing".
                // Civ skill only skips civs; guards still check this path.
                auto& modes = pBase->AlternativeModeData;
                for (int m = 0; m < modes.Num(); ++m)
                {
                    modes[m].bIsIllegal = false;
                    modes[m].bIsAllowedInCasing = true;
                }

                if (!pBase->IsA(SDK::USBZInteractableComponent::StaticClass()))
                    return;
                auto* pInt = reinterpret_cast<SDK::USBZInteractableComponent*>(pBase);
                // Byte at 0x338: bit4 AllowedInCasing, bit5 Illegal
                auto* pBits = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(pInt) + 0x338);
                *pBits = static_cast<uint8_t>((*pBits | (1u << 4)) & ~(1u << 5));
                pInt->bIsIllegal = 0;
                pInt->bIsAllowedInCasing = 1;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }

        static void ScrubActorInteractableSeh(SDK::AActor* pActor)
        {
            if (!pActor)
                return;
            __try
            {
                // Named members first (glass / gates expose these directly).
                if (pActor->IsA(SDK::ASBZCuttableActor::StaticClass()))
                {
                    auto* pCut = reinterpret_cast<SDK::ASBZCuttableActor*>(pActor);
                    ClearIllegalFlagSeh(pCut->InteractableComponent);
                }
                if (pActor->IsA(SDK::ASBZInteractableGate::StaticClass()))
                {
                    auto* pGate = reinterpret_cast<SDK::ASBZInteractableGate*>(pActor);
                    ClearIllegalFlagSeh(pGate->Interactable);
                }

                auto* pComp = pActor->GetComponentByClass(SDK::USBZInteractableComponent::StaticClass());
                if (pComp)
                    ClearIllegalFlagSeh(reinterpret_cast<SDK::USBZBaseInteractableComponent*>(pComp));

                // Some actors only expose the base type.
                auto* pBase = pActor->GetComponentByClass(SDK::USBZBaseInteractableComponent::StaticClass());
                if (pBase)
                    ClearIllegalFlagSeh(reinterpret_cast<SDK::USBZBaseInteractableComponent*>(pBase));
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }

        static void ScrubIllegalNearClassSeh(
            SDK::UWorld* pGWorld, SDK::UClass* pClass, float originX, float originY, float originZ, float radiusSq)
        {
            if (!pGWorld || !pClass)
                return;

            SDK::TArray<SDK::AActor*> actors{};
            __try
            {
                SDK::UGameplayStatics::GetAllActorsOfClass(pGWorld, pClass, &actors);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return;
            }

            for (int i = 0; i < actors.Num(); ++i)
            {
                auto* pActor = actors[i];
                if (!pActor)
                    continue;
                __try
                {
                    const SDK::FVector aLoc = pActor->K2_GetActorLocation();
                    const float dx = aLoc.X - originX;
                    const float dy = aLoc.Y - originY;
                    const float dz = aLoc.Z - originZ;
                    if ((dx * dx + dy * dy + dz * dz) > radiusSq)
                        continue;
                    ScrubActorInteractableSeh(pActor);
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                }
            }
        }

        // Unmasked + ghost: casing "you're being watched" on illegal pickups/opens/cuts.
        // Mask-on already skips this — clear illegal mode data on what you're touching + nearby.
        static void SuppressCasingObserveSeh(SDK::UWorld* pGWorld, SDK::ASBZPlayerCharacter* pLocal)
        {
            if (!pLocal)
                return;

            __try
            {
                pLocal->Client_SetObserved(false);
                // Wipe packed detection so observe UI can't stick from a prior tick.
                pLocal->Client_SetDetectionData(0);
                for (uint8_t i = 0; i < 8; ++i)
                    pLocal->Client_SetDetector(i, nullptr);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }

            __try
            {
                auto* pInteractor = pLocal->Interactor;
                if (pInteractor)
                {
                    ClearIllegalFlagSeh(pInteractor->CurrentInteraction);
                    ClearIllegalFlagSeh(pInteractor->LastInteraction);
                    ClearIllegalFlagSeh(pInteractor->ServerCompletingInteractable);
                    ClearIllegalFlagSeh(pInteractor->GetCurrentInteraction());
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }

            if (!pGWorld)
                return;

            float ox = 0.f, oy = 0.f, oz = 0.f;
            __try
            {
                const SDK::FVector loc = pLocal->K2_GetActorLocation();
                ox = loc.X;
                oy = loc.Y;
                oz = loc.Z;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return;
            }

            constexpr float kNearSq = 1200.f * 1200.f;
            ScrubIllegalNearClassSeh(pGWorld, SDK::ASBZSingleBagGenerator::StaticClass(), ox, oy, oz, kNearSq);
            ScrubIllegalNearClassSeh(pGWorld, SDK::ASBZInstantLoot::StaticClass(), ox, oy, oz, kNearSq);
            ScrubIllegalNearClassSeh(pGWorld, SDK::ASBZMultiBagGenerator::StaticClass(), ox, oy, oz, kNearSq);
            // Locked doors / windows / glass cutters — cops observe these in casing.
            ScrubIllegalNearClassSeh(pGWorld, SDK::ASBZInteractableGate::StaticClass(), ox, oy, oz, kNearSq);
            ScrubIllegalNearClassSeh(pGWorld, SDK::ASBZCuttableActor::StaticClass(), ox, oy, oz, kNearSq);
            ScrubIllegalNearClassSeh(pGWorld, SDK::ASBZInteractableDoor::StaticClass(), ox, oy, oz, kNearSq);
            ScrubIllegalNearClassSeh(pGWorld, SDK::ASBZInteractableWindow::StaticClass(), ox, oy, oz, kNearSq);
        }

        static void ClearObservedSeh(SDK::ASBZPlayerCharacter* pLocal)
        {
            if (!pLocal)
                return;
            __try
            {
                pLocal->Client_SetObserved(false);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }
    }

    void OnPlayerControllerTick(
        SDK::UWorld* pGWorld,
        SDK::ASBZPlayerController* pLocalController,
        SDK::ASBZPlayerCharacter* pLocalPlayer)
    {
        const bool bOn = CheatConfig::Get().m_misc.m_bGhostMode;
        const auto now = std::chrono::steady_clock::now();

        if (!bOn)
        {
            if (s_bWasOn)
            {
                if (auto* pCM = EnsureCheatManagerSeh(pLocalController))
                    ApplyCheatsSeh(pCM, false);
                ApplyLocalDetectionSeh(pLocalPlayer, false);
                ScrubWorldDetectorsSeh(pGWorld, pLocalPlayer, false);
                s_bWasOn = false;
                g_sStatus = "Ghost Mode off";
                Utils::LogDebug("GhostMode: OFF");
            }
            return;
        }

        // Path that worked in Lua — does NOT need CheatManager.
        bool bDetect = false;
        if (now >= s_timeNextDetect)
        {
            bDetect = ApplyLocalDetectionSeh(pLocalPlayer, true);
            ScrubWorldDetectorsSeh(pGWorld, pLocalPlayer, true);
            s_timeNextDetect = now + kDetectGap;
        }
        else
        {
            bDetect = true; // assume still applying
        }

        if (now >= s_timeNextIllegal)
        {
            SuppressCasingObserveSeh(pGWorld, pLocalPlayer);
            s_timeNextIllegal = now + kIllegalScrubGap;
        }
        else
        {
            ClearObservedSeh(pLocalPlayer);
        }

        int cams = 0;
        if (now >= s_timeNextCam)
        {
            cams = BlindCamerasSeh(pGWorld);
            s_timeNextCam = now + kCamBlindGap;
        }

        bool bCm = false;
        auto* pCM = EnsureCheatManagerSeh(pLocalController);
        if (pCM && ( !s_bWasOn || now >= s_timeNextCheat))
        {
            ApplyCheatsSeh(pCM, true);
            s_timeNextCheat = now + kCheatReapplyGap;
            bCm = true;
        }
        else if (pCM)
        {
            bCm = true;
        }

        if (!s_bWasOn)
            Utils::LogDebug("GhostMode: ON");

        s_bWasOn = true;

        if (bCm)
            g_sStatus = "Ghost ON — invisible + no observe (mask or casing)";
        else if (bDetect)
            g_sStatus = "Ghost ON — detection 0 + no observe (no CheatManager)";
        else
            g_sStatus = "Ghost ON — cams only (enter heist / wait for pawn)";

        (void)cams;
    }
}
