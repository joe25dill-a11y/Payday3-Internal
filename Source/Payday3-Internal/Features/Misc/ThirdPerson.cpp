#include "ThirdPerson.hpp"
#include "../../Menu.hpp"

#include <Windows.h>
#include <string>

namespace Cheat::ThirdPerson
{
    std::string g_sStatus = "3rd Person off";

    namespace
    {
        constexpr float kBack = 280.f;
        constexpr float kRight = 38.f;
        constexpr float kLift = 70.f;
        constexpr float kUp = 20.f;

        static bool s_bWasOn = false;

        static bool ObjectOkSeh(const SDK::UObject* pObj)
        {
            __try
            {
                return pObj && pObj->Class;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static void OwnerNoSeeInner(SDK::UPrimitiveComponent* pComp, bool bNoSee)
        {
            if (!pComp)
                return;
            pComp->SetOwnerNoSee(bNoSee);
        }

        static void OnlyOwnerSeeInner(SDK::UPrimitiveComponent* pComp, bool bOnlyOwnerSee)
        {
            if (!pComp)
                return;
            pComp->SetOnlyOwnerSee(bOnlyOwnerSee);
        }

        static void HiddenInGameInner(SDK::USceneComponent* pComp, bool bHidden)
        {
            if (!pComp)
                return;
            pComp->SetHiddenInGame(bHidden, true);
        }

        static void ForceTickInner(SDK::USkeletalMeshComponent* pMesh, bool bForce)
        {
            if (!pMesh)
                return;

            pMesh->VisibilityBasedAnimTickOption = bForce
                ? SDK::EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones
                : SDK::EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
            pMesh->bEnableUpdateRateOptimizations = bForce ? 0 : 1;
            pMesh->bPauseAnims = 0;
            pMesh->bNoSkeletonUpdate = 0;
            pMesh->bForceRefpose = 0;
            pMesh->bOnlyAllowAutonomousTickPose = 0;
            if (bForce)
            {
                pMesh->bRecentlyRendered = 1;
                pMesh->bPreviousRecentlyRendered = 1;
                pMesh->SetComponentTickEnabled(true);
            }

            if (pMesh->IsA(SDK::USkeletalMeshComponentBudgeted::StaticClass()))
            {
                auto* pBudg = reinterpret_cast<SDK::USkeletalMeshComponentBudgeted*>(pMesh);
                pBudg->bAutoRegisterWithBudgetAllocator = bForce ? 0 : 1;
                pBudg->bOptimizeUsingRenderedOnScreen = bForce ? 0 : 1;
            }
        }

        static void ApplyThirdBodyInner(SDK::USkeletalMeshComponent* pMesh, bool bThird)
        {
            if (!ObjectOkSeh(pMesh))
                return;
            OwnerNoSeeInner(pMesh, !bThird);
            ForceTickInner(pMesh, bThird);
        }

        static void ApplyMeshesInner(SDK::ASBZPlayerCharacter* pLocal, bool bThird)
        {
            if (!pLocal)
                return;

            // Hide 1P arms from owner. Do NOT touch FPCameraAttachment or weapons —
            // that is what left the red cross + broken gun after exit.
            if (ObjectOkSeh(pLocal->Mesh1P))
            {
                OwnerNoSeeInner(pLocal->Mesh1P, bThird);
                OnlyOwnerSeeInner(pLocal->Mesh1P, true);
                HiddenInGameInner(pLocal->Mesh1P, bThird);
            }
            if (ObjectOkSeh(pLocal->Mesh1PBody))
            {
                OwnerNoSeeInner(pLocal->Mesh1PBody, bThird);
                OnlyOwnerSeeInner(pLocal->Mesh1PBody, true);
                HiddenInGameInner(pLocal->Mesh1PBody, bThird);
            }
            if (ObjectOkSeh(pLocal->Mesh1PSuit))
            {
                OwnerNoSeeInner(pLocal->Mesh1PSuit, bThird);
                OnlyOwnerSeeInner(pLocal->Mesh1PSuit, true);
                HiddenInGameInner(pLocal->Mesh1PSuit, bThird);
            }
            if (ObjectOkSeh(pLocal->Mesh1PGloves))
            {
                OwnerNoSeeInner(pLocal->Mesh1PGloves, bThird);
                OnlyOwnerSeeInner(pLocal->Mesh1PGloves, true);
                HiddenInGameInner(pLocal->Mesh1PGloves, bThird);
            }

            // Make the 3P body visible to the owner — do NOT use SetHiddenInGame
            // here because propagation would hide child actors like the mask.
            if (ObjectOkSeh(pLocal->Mesh))
            {
                OwnerNoSeeInner(pLocal->Mesh, !bThird);
                ForceTickInner(pLocal->Mesh, bThird);
                if (pLocal->Mesh->IsA(SDK::USBZModularCharacterComponent::StaticClass()))
                {
                    auto* pMod = reinterpret_cast<SDK::USBZModularCharacterComponent*>(pLocal->Mesh);
                    if (ObjectOkSeh(pMod->SuitMeshComponent))
                    {
                        OwnerNoSeeInner(pMod->SuitMeshComponent, !bThird);
                        ForceTickInner(pMod->SuitMeshComponent, bThird);
                    }
                    if (ObjectOkSeh(pMod->GlovesMeshComponent))
                    {
                        OwnerNoSeeInner(pMod->GlovesMeshComponent, !bThird);
                        ForceTickInner(pMod->GlovesMeshComponent, bThird);
                    }
                    if (ObjectOkSeh(pMod->BodyMeshComponent))
                    {
                        OwnerNoSeeInner(pMod->BodyMeshComponent, !bThird);
                        ForceTickInner(pMod->BodyMeshComponent, bThird);
                    }
                }
            }

            // The currently equipped weapon mesh can sit in the wrong place for the
            // local owner in this fake 3P mode, so hide that stray owner view.
            if (ObjectOkSeh(pLocal->CurrentEquippable)
                && ObjectOkSeh(pLocal->CurrentEquippable->Mesh)
                && pLocal->CurrentEquippable->Mesh->IsA(SDK::UPrimitiveComponent::StaticClass()))
            {
                auto* pPrim = reinterpret_cast<SDK::UPrimitiveComponent*>(pLocal->CurrentEquippable->Mesh);
                OwnerNoSeeInner(pPrim, bThird);
                HiddenInGameInner(pPrim, bThird);
            }

            if (ObjectOkSeh(pLocal->FPCameraAttachment))
            {
                if (ObjectOkSeh(pLocal->FPCameraAttachment->EquippedWeapon)
                    && ObjectOkSeh(pLocal->FPCameraAttachment->EquippedWeapon->Mesh)
                    && pLocal->FPCameraAttachment->EquippedWeapon->Mesh->IsA(SDK::UPrimitiveComponent::StaticClass()))
                {
                    auto* pPrim = reinterpret_cast<SDK::UPrimitiveComponent*>(pLocal->FPCameraAttachment->EquippedWeapon->Mesh);
                    OwnerNoSeeInner(pPrim, bThird);
                    HiddenInGameInner(pPrim, bThird);
                }

                for (int i = 0; i < pLocal->FPCameraAttachment->TargetingHideMeshArray.Num(); ++i)
                {
                    if (!pLocal->FPCameraAttachment->TargetingHideMeshArray.IsValidIndex(i))
                        continue;
                    auto* pMesh = pLocal->FPCameraAttachment->TargetingHideMeshArray[i];
                    if (!ObjectOkSeh(pMesh))
                        continue;
                    HiddenInGameInner(pMesh, bThird);
                    if (pMesh->IsA(SDK::UPrimitiveComponent::StaticClass()))
                        OwnerNoSeeInner(reinterpret_cast<SDK::UPrimitiveComponent*>(pMesh), bThird);
                }
            }
        }

        static void ApplyMeshesSeh(SDK::ASBZPlayerCharacter* pLocal, bool bThird)
        {
            __try { ApplyMeshesInner(pLocal, bThird); }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }

        static bool ApplyCameraInner(SDK::ULocalPlayer* pLocalPlayer, SDK::FMinimalViewInfo* pView)
        {
            if (!pLocalPlayer || !pView)
                return false;

            SDK::APlayerController* pPC = pLocalPlayer->PlayerController;
            if (!pPC)
                return false;

            SDK::APawn* pPawn = pPC->AcknowledgedPawn;
            if (!pPawn)
                pPawn = pPC->K2_GetPawn();
            if (!pPawn)
                return false;

            SDK::FVector loc = pPawn->K2_GetActorLocation();
            const SDK::FRotator rot = pView->Rotation;
            const SDK::FVector fwd = SDK::UKismetMathLibrary::GetForwardVector(rot);
            const SDK::FVector right = SDK::UKismetMathLibrary::GetRightVector(rot);
            const SDK::FVector up = SDK::UKismetMathLibrary::GetUpVector(rot);
            const float side = static_cast<float>(CheatConfig::Get().m_misc.m_iThirdPersonSide);
            loc.Z += kLift;
            pView->Location = loc - (fwd * kBack) + (right * (kRight * side)) + (up * kUp);
            return true;
        }

        static bool ApplyCameraSeh(SDK::ULocalPlayer* pLocalPlayer, SDK::FMinimalViewInfo* pView)
        {
            __try { return ApplyCameraInner(pLocalPlayer, pView); }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        }
    }

    void ApplyCamera(SDK::ULocalPlayer* pLocalPlayer, SDK::FMinimalViewInfo* pView)
    {
        if (!CheatConfig::Get().m_misc.m_bThirdPerson)
            return;

        try { ApplyCameraSeh(pLocalPlayer, pView); }
        catch (...) {}
    }

    void OnPlayerControllerTick(
        SDK::UWorld* /*pGWorld*/,
        SDK::ASBZPlayerController* /*pLocalController*/,
        SDK::ASBZPlayerCharacter* pLocalPlayer)
    {
        const bool bOn = CheatConfig::Get().m_misc.m_bThirdPerson;
        if (!bOn)
        {
            if (s_bWasOn)
                ApplyMeshesSeh(pLocalPlayer, false);
            s_bWasOn = false;
            g_sStatus = "3rd Person off";
            return;
        }

        s_bWasOn = true;
        ApplyMeshesSeh(pLocalPlayer, true);
        switch (CheatConfig::Get().m_misc.m_iThirdPersonSide)
        {
        case -1:
            g_sStatus = "3rd Person on — left shoulder";
            break;
        case 1:
            g_sStatus = "3rd Person on — right shoulder";
            break;
        default:
            g_sStatus = "3rd Person on — centered";
            break;
        }
    }
}
