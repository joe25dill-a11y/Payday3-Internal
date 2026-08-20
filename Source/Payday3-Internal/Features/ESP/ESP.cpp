#include <imgui.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <climits>
#include <cstring>
#include "../../Dumper-7/SDK.hpp"
#include "../../Utils/Logging.hpp"
#include "ESP.hpp"
#include "LootClassify.hpp"
#include "../Features.hpp"
#include "../../Menu.hpp"
#include "../../Features/FNames.hpp"

#undef min
#undef max

namespace
{
    // PD3 through-wall ColorIndex palette (same as freecam F4):
    // 3=red always | 9=white always | 10=yellow always | 11=pink
    // 2=orange is LOS-only — never use for ESP glow.
    constexpr int8_t kOutlineRed = 3;
    constexpr int8_t kOutlineWhite = 9;
    constexpr int8_t kOutlineYellow = 10;
    constexpr int8_t kOutlinePink = 11;
    // F4 keep-alive: pulse occasionally, never every render frame.
    constexpr auto kOutlinePulseGap = std::chrono::milliseconds(450);

    static SDK::USBZOutlineAsset* s_outlineByColor[32]{};
    static SDK::USBZOutlineAsset* s_pTemplateAsset = nullptr;
    static bool s_bOutlineAssetScanDone = false;
    static std::unordered_map<uintptr_t, std::chrono::steady_clock::time_point> s_outlinePulseAt;
    static std::unordered_map<uintptr_t, bool> s_lastSecondaryType;

    static void BeginOutlineFrame()
    {
    }

    static std::string ToLowerAscii(std::string s)
    {
        for (char& c : s)
        {
            if (c >= 'A' && c <= 'Z')
                c = static_cast<char>(c - 'A' + 'a');
        }
        return s;
    }

    static int OutlineTemplateScore(SDK::USBZOutlineAsset* pAsset)
    {
        if (!pAsset)
            return -1;
        int score = 0;
        const std::string name = ToLowerAscii(pAsset->GetName());
        if (name.find("nonoccluded") != std::string::npos || name.find("placeable") != std::string::npos)
            score += 50;
        else if (name.find("sensor") != std::string::npos || name.find("released") != std::string::npos)
            score += 30;
        else if (name.find("marked") != std::string::npos)
            score += 10;
        if (pAsset->ColorIndex == 2)
            score -= 80;
        if (pAsset->ColorIndex == kOutlineYellow || pAsset->ColorIndex == kOutlineWhite
            || pAsset->ColorIndex == kOutlineRed || pAsset->ColorIndex == kOutlinePink)
            score += 20;
        return score;
    }

    static void ScanOutlineAssetsOnce()
    {
        if (s_bOutlineAssetScanDone)
            return;
        s_bOutlineAssetScanDone = true;
        if (!SDK::UObject::GObjects)
            return;

        std::vector<SDK::USBZOutlineAsset*> candidates;
        candidates.reserve(64);
        const int32_t n = SDK::UObject::GObjects->Num();
        for (int32_t i = 0; i < n; ++i)
        {
            SDK::UObject* pObj = SDK::UObject::GObjects->GetByIndex(i);
            if (!pObj || !pObj->Class || !pObj->IsA(SDK::USBZOutlineAsset::StaticClass()))
                continue;
            candidates.push_back(reinterpret_cast<SDK::USBZOutlineAsset*>(pObj));
        }

        std::sort(candidates.begin(), candidates.end(), [](SDK::USBZOutlineAsset* a, SDK::USBZOutlineAsset* b)
        {
            return OutlineTemplateScore(a) > OutlineTemplateScore(b);
        });
        if (!candidates.empty())
            s_pTemplateAsset = candidates.front();

        const int8_t want[] = { kOutlineRed, kOutlineYellow, kOutlineWhite, kOutlinePink };
        size_t next = 0;
        for (int8_t idx : want)
        {
            SDK::USBZOutlineAsset* pick = nullptr;
            for (auto* a : candidates)
            {
                if (a && a->ColorIndex == idx)
                {
                    pick = a;
                    break;
                }
            }
            if (!pick)
            {
                while (next < candidates.size())
                {
                    auto* a = candidates[next++];
                    bool used = false;
                    for (int8_t w : want)
                    {
                        if (s_outlineByColor[w] == a)
                        {
                            used = true;
                            break;
                        }
                    }
                    if (!used)
                    {
                        pick = a;
                        break;
                    }
                }
            }
            if (!pick)
                pick = s_pTemplateAsset;
            if (!pick)
                continue;
            pick->ColorIndex = idx;
            pick->Distance = 50000000.f;
            pick->Priority = 255;
            s_outlineByColor[idx] = pick;
        }
    }

    // Map ImGui picker RGB → nearest PD3 through-wall ColorIndex.
    static int8_t ColorIndexFromImU32(ImU32 col)
    {
        const int r = static_cast<int>(col & 0xFF);
        const int g = static_cast<int>((col >> 8) & 0xFF);
        const int b = static_cast<int>((col >> 16) & 0xFF);

        struct Cand { int8_t idx; int r, g, b; };
        // Through-wall capable only (matches freecam notes).
        const Cand cands[] = {
            { kOutlineRed,    255, 40, 40 },
            { kOutlineWhite,  255, 255, 255 },
            { kOutlineYellow, 255, 220, 0 },
            { kOutlinePink,   255, 80, 220 },
        };
        int best = 0;
        int bestDist = INT_MAX;
        for (int i = 0; i < 4; ++i)
        {
            const int dr = r - cands[i].r;
            const int dg = g - cands[i].g;
            const int db = b - cands[i].b;
            const int d = dr * dr + dg * dg + db * db;
            if (d < bestDist)
            {
                bestDist = d;
                best = i;
            }
        }
        return cands[best].idx;
    }

    static void AttachActorMeshesToOutline(SDK::USBZOutlineComponent* pOutline, SDK::AActor* pActor);
    static void ApplyFarRenderForOutline(SDK::AActor* pActor);
    static void ApplyActorOutline(SDK::AActor* pActor, int8_t colorIndex);

    static SDK::USBZOutlineAsset* ResolveOutlineAssetForColor(int8_t colorIndex)
    {
        ScanOutlineAssetsOnce();
        if (colorIndex < 0 || colorIndex >= 32)
            colorIndex = kOutlineYellow;

        if (s_outlineByColor[colorIndex])
        {
            s_outlineByColor[colorIndex]->ColorIndex = colorIndex;
            s_outlineByColor[colorIndex]->Distance = 50000000.f;
            s_outlineByColor[colorIndex]->Priority = 255;
            return s_outlineByColor[colorIndex];
        }

        if (s_pTemplateAsset)
        {
            s_pTemplateAsset->ColorIndex = colorIndex;
            s_pTemplateAsset->Distance = 50000000.f;
            s_pTemplateAsset->Priority = 255;
            return s_pTemplateAsset;
        }
        return nullptr;
    }

    static void SoftEnableOutlineComp(SDK::USBZOutlineComponent* pOutline, SDK::USBZOutlineAsset* pAsset, bool bPulseMulticast, bool bAttachMeshes, SDK::AActor* pActor)
    {
        if (!pOutline || !pAsset)
            return;
        pOutline->bIsHiddenManagedByInteractable = false;
        pOutline->bIsReplicatedHidden = false;
        pOutline->SetReplicatedHidden(false);
        pOutline->ActiveAsset = pAsset;
        pOutline->ActiveReplicated = pAsset;
        if (bAttachMeshes && pActor)
            AttachActorMeshesToOutline(pOutline, pActor);
        if (!bPulseMulticast)
            return;
        pOutline->Multicast_SetActiveReplicated(pAsset);
        pOutline->OnRep_ActiveReplicated();
        pOutline->Multicast_SetReplicatedHidden(false);
        pOutline->Multicast_SetActiveReplicated(pAsset);
        pOutline->OnRep_ActiveReplicated();
    }

    static void AttachActorMeshesToOutline(SDK::USBZOutlineComponent* pOutline, SDK::AActor* pActor)
    {
        if (!pOutline || !pActor)
            return;
        auto comps = pActor->K2_GetComponentsByClass(SDK::UMeshComponent::StaticClass());
        for (int i = 0; i < comps.Num(); ++i)
        {
            if (!comps.IsValidIndex(i))
                break;
            auto* pMesh = reinterpret_cast<SDK::UMeshComponent*>(comps[i]);
            if (pMesh)
                pOutline->AddMesh(pMesh, true);
        }
    }

    static void ApplyFarRenderForOutline(SDK::AActor* pActor)
    {
        if (!pActor)
            return;
        auto comps = pActor->K2_GetComponentsByClass(SDK::UPrimitiveComponent::StaticClass());
        for (int i = 0; i < comps.Num(); ++i)
        {
            if (!comps.IsValidIndex(i))
                break;
            auto* pPrim = reinterpret_cast<SDK::UPrimitiveComponent*>(comps[i]);
            if (!pPrim)
                continue;
            pPrim->bNeverDistanceCull = 1;
            pPrim->bAllowCullDistanceVolume = 0;
            pPrim->LDMaxDrawDistance = 0.f;
            pPrim->SetCullDistance(0.f);
        }
    }

    static SDK::USBZOutlineComponent* GetPrimaryOutline(SDK::AActor* pActor)
    {
        if (!pActor)
            return nullptr;
        // Phones / keycards / RFID expose SBZOutline (not InstantLoot OutlineComponent).
        if (pActor->IsA(SDK::ABP_QRPhone_C::StaticClass()))
        {
            if (auto* p = reinterpret_cast<SDK::ABP_QRPhone_C*>(pActor)->SBZOutline)
                return p;
        }
        if (pActor->IsA(SDK::ABP_KeycardBase_C::StaticClass()))
        {
            if (auto* p = reinterpret_cast<SDK::ABP_KeycardBase_C*>(pActor)->SBZOutline)
                return p;
        }
        if (pActor->IsA(SDK::ABP_RFIDTagBase_C::StaticClass()))
        {
            if (auto* p = reinterpret_cast<SDK::ABP_RFIDTagBase_C*>(pActor)->SBZOutline)
                return p;
        }
        if (pActor->IsA(SDK::ASBZInstantLoot::StaticClass()))
        {
            if (auto* p = reinterpret_cast<SDK::ASBZInstantLoot*>(pActor)->OutlineComponent)
                return p;
        }
        if (pActor->IsA(SDK::ASBZSingleBagGenerator::StaticClass()))
        {
            if (auto* p = reinterpret_cast<SDK::ASBZSingleBagGenerator*>(pActor)->Outline)
                return p;
        }
        if (pActor->IsA(SDK::ASBZMultiBagGenerator::StaticClass()))
        {
            if (auto* p = reinterpret_cast<SDK::ASBZMultiBagGenerator*>(pActor)->OutlineComponent)
                return p;
        }
        return reinterpret_cast<SDK::USBZOutlineComponent*>(
            pActor->GetComponentByClass(SDK::USBZOutlineComponent::StaticClass()));
    }

    static void ApplyActorOutline(SDK::AActor* pActor, int8_t colorIndex)
    {
        if (!pActor || pActor->IsActorBeingDestroyed())
            return;

        SDK::USBZOutlineAsset* pAsset = ResolveOutlineAssetForColor(colorIndex);
        if (!pAsset)
            return;

        const bool bIsCharacter = pActor->IsA(SDK::ASBZCharacter::StaticClass());
        if (!bIsCharacter)
            ApplyFarRenderForOutline(pActor);

        const uintptr_t key = reinterpret_cast<uintptr_t>(pActor);
        const auto now = std::chrono::steady_clock::now();
        bool bPulse = true;
        auto it = s_outlinePulseAt.find(key);
        if (it != s_outlinePulseAt.end() && now < it->second)
            bPulse = false;
        else
            s_outlinePulseAt[key] = now + kOutlinePulseGap;

        if (bIsCharacter)
        {
            auto* pChar = reinterpret_cast<SDK::ASBZCharacter*>(pActor);
            if (pChar->OutlineComponent)
                SoftEnableOutlineComp(pChar->OutlineComponent, pAsset, bPulse, false, pActor);

            auto* pByClass = reinterpret_cast<SDK::USBZOutlineComponent*>(
                pActor->GetComponentByClass(SDK::USBZOutlineComponent::StaticClass()));
            if (pByClass && pByClass != pChar->OutlineComponent)
                SoftEnableOutlineComp(pByClass, pAsset, bPulse, false, pActor);

            pChar->Multicast_SetMarked(true);
            return;
        }

        auto SoftLoot = [&](SDK::USBZOutlineComponent* pOc)
        {
            SoftEnableOutlineComp(pOc, pAsset, bPulse, true, pActor);
        };

        if (pActor->IsA(SDK::ASBZInstantLoot::StaticClass()))
            SoftLoot(reinterpret_cast<SDK::ASBZInstantLoot*>(pActor)->OutlineComponent);
        if (pActor->IsA(SDK::ASBZSingleBagGenerator::StaticClass()))
        {
            auto* pGen = reinterpret_cast<SDK::ASBZSingleBagGenerator*>(pActor);
            SoftLoot(pGen->Outline);
            if (bPulse)
            {
                pGen->SetEnabled(true);
                if (pGen->Interactable)
                    pGen->Interactable->SetLocalEnabled(true);
            }
        }
        if (pActor->IsA(SDK::ASBZMultiBagGenerator::StaticClass()))
            SoftLoot(reinterpret_cast<SDK::ASBZMultiBagGenerator*>(pActor)->OutlineComponent);

        SoftLoot(reinterpret_cast<SDK::USBZOutlineComponent*>(
            pActor->GetComponentByClass(SDK::USBZOutlineComponent::StaticClass())));

        auto allOc = pActor->K2_GetComponentsByClass(SDK::USBZOutlineComponent::StaticClass());
        for (int i = 0; i < allOc.Num(); ++i)
        {
            if (!allOc.IsValidIndex(i))
                break;
            SoftLoot(reinterpret_cast<SDK::USBZOutlineComponent*>(allOc[i]));
        }
    }

    static void ApplyCheapLootOutline(SDK::AActor* pActor)
    {
        ApplyActorOutline(pActor, kOutlineYellow);
    }

    static bool FastIsLootActor(SDK::AActor* pActor)
    {
        if (!pActor || !pActor->Class)
            return false;
        if (pActor->IsA(SDK::ASBZInstantLoot::StaticClass())
            || pActor->IsA(SDK::ASBZSingleBagGenerator::StaticClass())
            || pActor->IsA(SDK::ASBZMultiBagGenerator::StaticClass())
            || pActor->IsA(SDK::ASBZBagItem::StaticClass())
            || pActor->IsA(SDK::ABP_KeycardBase_C::StaticClass())
            || pActor->IsA(SDK::ABP_RFIDTagBase_C::StaticClass())
            || pActor->IsA(SDK::ABP_QRPhone_C::StaticClass())
            || pActor->IsA(SDK::ABP_WoodenCrate_C::StaticClass())
            || pActor->IsA(SDK::ABP_LiquidNitrogenCanister_C::StaticClass())
            || pActor->IsA(SDK::ABP_CarriedInteractableBase_C::StaticClass())
            || pActor->IsA(SDK::ABP_MethIngredientBase_C::StaticClass())
            || pActor->IsA(SDK::ABP_BaseValuableBag_C::StaticClass()))
            return true;
        const auto n = pActor->Class->Name;
        return n == FNames::BP_FOR_USBDrive_C
            || n == FNames::BP_Plankspile_C
            || n == FNames::BP_DAT_C4Explosive_01_Pickup_C;
    }

    // F4-style mesh glow: color-mapped through-wall asset + mesh attach + far render.
    static void ApplyFreecamStyleLootOutline(SDK::AActor* pActor, int8_t colorIndex)
    {
        if (!pActor)
            return;

        const uintptr_t key = reinterpret_cast<uintptr_t>(pActor);
        const auto now = std::chrono::steady_clock::now();

        bool bForce = false;
        if (pActor->IsA(SDK::ASBZSingleBagGenerator::StaticClass()))
        {
            auto* pGen = reinterpret_cast<SDK::ASBZSingleBagGenerator*>(pActor);
            const bool bSec = pGen->bIsSecondaryTypeUsed;
            auto itSec = s_lastSecondaryType.find(key);
            if (itSec == s_lastSecondaryType.end() || itSec->second != bSec)
            {
                s_lastSecondaryType[key] = bSec;
                bForce = true;
            }
        }

        auto it = s_outlinePulseAt.find(key);
        if (!bForce && it != s_outlinePulseAt.end() && now < it->second)
            return;
        s_outlinePulseAt[key] = now + kOutlinePulseGap;

        SDK::USBZOutlineComponent* pOc = GetPrimaryOutline(pActor);
        if (!pOc)
            return;

        SDK::USBZOutlineAsset* pAsset = ResolveOutlineAssetForColor(colorIndex);
        if (!pAsset)
            pAsset = pOc->InteractableFocusAsset;
        if (!pAsset)
            pAsset = pOc->DefaultAsset;
        if (!pAsset)
            pAsset = pOc->ActiveAsset;
        if (!pAsset)
            pAsset = pOc->ActiveReplicated;
        if (!pAsset)
            return;

        pAsset->Distance = 50000000.f;
        pAsset->Priority = 255;
        AttachActorMeshesToOutline(pOc, pActor);
        ApplyFarRenderForOutline(pActor);
        SoftEnableOutlineComp(pOc, pAsset, true, false, nullptr);

        if (pActor->IsA(SDK::ASBZSingleBagGenerator::StaticClass()))
            reinterpret_cast<SDK::ASBZSingleBagGenerator*>(pActor)->SetEnabled(true);
    }
}

// Bone pair structure for skeleton rendering
struct BonePair {
    int32_t ChildIndex;
    int32_t ParentIndex;
};

// Cache bone pairs per skeletal mesh
static std::unordered_map<SDK::USkeletalMesh*, std::vector<BonePair>> g_SkeletonCache;
static std::unordered_map<SDK::USkeletalMesh*, std::vector<BonePair>> g_GuardBonePairsCache;

// Calculate guard bone pairs from skeletal mesh and cache them
const std::vector<BonePair>& CalculateGuardBonePairs(SDK::USkeletalMeshComponent* pMeshComponent) {
    static std::vector<BonePair> empty;
    
    if (!pMeshComponent || !pMeshComponent->SkeletalMesh)
        return empty;

    SDK::USkeletalMesh* pMesh = pMeshComponent->SkeletalMesh;
    
    // Return cached if exists
    auto it = g_GuardBonePairsCache.find(pMesh);
    if (it != g_GuardBonePairsCache.end())
        return it->second;

    // Calculate bone pairs by following parent hierarchy from specific end bones
    std::vector<BonePair> pairs;
    std::unordered_set<int32_t> processedBones;

    SDK::FName head = SDK::UKismetStringLibrary::Conv_StringToName(L"Head");
    SDK::FName rightHand = SDK::UKismetStringLibrary::Conv_StringToName(L"RightHand");
    SDK::FName leftHand = SDK::UKismetStringLibrary::Conv_StringToName(L"LeftHand");
    SDK::FName rightFoot = SDK::UKismetStringLibrary::Conv_StringToName(L"RightFloor");
    SDK::FName leftFoot = SDK::UKismetStringLibrary::Conv_StringToName(L"LeftFloor");

    // Key end bones to trace back from (head, hands, feet)
    std::vector<int32_t> endBones = {pMeshComponent->GetBoneIndex(head), pMeshComponent->GetBoneIndex(rightHand), pMeshComponent->GetBoneIndex(leftHand), pMeshComponent->GetBoneIndex(rightFoot), pMeshComponent->GetBoneIndex(leftFoot)}; // Head, Right Hand, Left Hand, Right Foot, Left Foot
    
    for (int32_t endBone : endBones) {
        int32_t current = endBone;
        while (current >= 0) {
            SDK::FName boneName = pMeshComponent->GetBoneName(current);
            SDK::FName parentBoneName = pMeshComponent->GetParentBone(boneName);
            int32_t parentIndex = pMeshComponent->GetBoneIndex(parentBoneName);
            
            if (parentIndex >= 0 && parentIndex != current) {
                // Avoid duplicate pairs
                if (processedBones.find(current) == processedBones.end()) {
                    pairs.push_back({current, parentIndex});
                    processedBones.insert(current);
                }
                current = parentIndex;
            } else {
                break;
            }
        }
    }
    
    // Cache and return
    g_GuardBonePairsCache[pMesh] = pairs;
    return g_GuardBonePairsCache[pMesh];
}

// Calculate and cache bone pairs for a skeletal mesh
const std::vector<BonePair>& GetOrCalculateBonePairs(SDK::USkeletalMeshComponent* pMeshComponent) {
    static std::vector<BonePair> empty;
    
    if (!pMeshComponent || !pMeshComponent->SkeletalMesh)
        return empty;

    SDK::USkeletalMesh* pMesh = pMeshComponent->SkeletalMesh;
    
    // Return cached if exists
    auto it = g_SkeletonCache.find(pMesh);
    if (it != g_SkeletonCache.end())
        return it->second;

    // Calculate bone pairs from reference skeleton
    std::vector<BonePair> pairs;
    
    int32_t BoneCount = pMeshComponent->GetNumBones();
    for (int32_t i = 0; i < BoneCount; i++) {
        // Get parent index from bone hierarchy
        SDK::FName boneName = pMeshComponent->GetParentBone(pMeshComponent->GetBoneName(i));
        int32_t ParentIndex = pMeshComponent->GetBoneIndex(boneName);
        
        // Skip root bone (parent index -1)
        if (ParentIndex >= 0 && ParentIndex != i) {
            pairs.push_back({i, ParentIndex});
        }
    }
    
    // Cache and return
    g_SkeletonCache[pMesh] = pairs;
    return g_SkeletonCache[pMesh];
}

std::optional<ImVec4> CalculateScreenBoxFromBBox(SDK::APlayerController* pPlayerController,SDK::FVector vecOrigin, SDK::FVector vecExtent)
{
    SDK::FVector aBox[]{
        vecOrigin + SDK::FVector(vecExtent.X, vecExtent.Y, vecExtent.Z),
        vecOrigin + SDK::FVector(-vecExtent.X, vecExtent.Y, vecExtent.Z),
        vecOrigin + SDK::FVector(vecExtent.X, -vecExtent.Y, vecExtent.Z),
        vecOrigin + SDK::FVector(-vecExtent.X, -vecExtent.Y, vecExtent.Z),
        vecOrigin + SDK::FVector(vecExtent.X, vecExtent.Y, -vecExtent.Z),
        vecOrigin + SDK::FVector(-vecExtent.X, vecExtent.Y, -vecExtent.Z),
        vecOrigin + SDK::FVector(vecExtent.X, -vecExtent.Y, -vecExtent.Z),
        vecOrigin + SDK::FVector(-vecExtent.X, -vecExtent.Y, -vecExtent.Z)
    };

    float flMinX = std::numeric_limits<float>::max(), flMaxX = std::numeric_limits<float>::lowest(), flMinY = std::numeric_limits<float>::max(), flMaxY = std::numeric_limits<float>::lowest();

    for(size_t i = 0; i < 8; ++i){
        SDK::FVector2D vec2ScreenPos;
        if(!pPlayerController->ProjectWorldLocationToScreen(aBox[i], &vec2ScreenPos, false))
            continue;

        flMinX = std::min(flMinX, vec2ScreenPos.X);
        flMaxX = std::max(flMaxX, vec2ScreenPos.X);
        flMinY = std::min(flMinY, vec2ScreenPos.Y);
        flMaxY = std::max(flMaxY, vec2ScreenPos.Y);
    }

    if(flMinX == std::numeric_limits<float>::max() || flMaxX == std::numeric_limits<float>::lowest() || flMinY == std::numeric_limits<float>::max() || flMaxY == std::numeric_limits<float>::lowest() ||
        flMinX >= flMaxX || flMinY >= flMaxY)
        return{};

    return ImVec4{ flMinX, flMinY, flMaxX, flMaxY };
}

std::optional<ImVec4> CalculateScreenBoxFromTopBottom(SDK::APlayerController* pPlayerController, SDK::AActor* pActor, SDK::FVector vecTop, SDK::FVector vecBottom, float flCrouchingHeight = 80.f, float flBaseWidthRatio = 0.25f, float flSidewaysWidthRatioAdd = 0.15f, float flCrouchingWidthRatioMultiplier = 1.2f)
{
    SDK::FVector2D vec2Top, vec2Bottom;
    if (!pPlayerController->ProjectWorldLocationToScreen(vecBottom, &vec2Bottom, false) ||
        !pPlayerController->ProjectWorldLocationToScreen(vecTop, &vec2Top, false))
        return {};

    float flHeight = std::abs(vec2Top.Y - vec2Bottom.Y);
    float flYawRad = pActor->K2_GetActorRotation().Yaw * (3.14159265358979323846f / 180.0f);
    float flWidthRatio = flBaseWidthRatio + (std::abs(SDK::FVector(std::cos(flYawRad), std::sin(flYawRad), 0.f).GetNormalized().Dot((pActor->K2_GetActorLocation() - pPlayerController->PlayerCameraManager->GetCameraLocation()).GetNormalized())) * flSidewaysWidthRatioAdd); // Range: 0.25 to 0.4
    
    if (flHeight < flCrouchingHeight)
        flWidthRatio *= flCrouchingWidthRatioMultiplier;

    float flWidth = flHeight * flWidthRatio;
    float flCenter = (vec2Bottom.X + vec2Top.X) / 2.0f;
    return ImVec4{ flCenter - (flWidth / 2.0f), std::min(vec2Bottom.Y, vec2Top.Y), flCenter + (flWidth / 2.0f), std::max(vec2Bottom.Y, vec2Top.Y) };
}

std::optional<ImVec4> CalculateScreenBoxForGuard(SDK::USkeletalMeshComponent* pMeshComponent, SDK::APlayerController* pPlayerController, SDK::AActor* pActor)
{
    if (!pMeshComponent || !pPlayerController || !pActor)
        return {};

    return CalculateScreenBoxFromTopBottom(
        pPlayerController, 
        pActor, 
        pMeshComponent->GetSocketLocation(SDK::UKismetStringLibrary::Conv_StringToName(L"HeadEnd")), 
        pMeshComponent->GetSocketLocation(SDK::UKismetStringLibrary::Conv_StringToName(L"Reference"))
    );
}

// Left, Top, Right, Bottom
std::optional<ImVec4> CalculateScreenBoxUsingBounds(SDK::APlayerController* pPlayerController, SDK::AActor* pActor)
{
    if (!pPlayerController || !pActor)
        return {};

    //auto bbox = pMeshComponent->GetTightBounds(false);
    SDK::FVector vecOrigin{};
    SDK::FVector vecExtent{};

    pActor->GetActorBounds(true, &vecOrigin, &vecExtent, false);
    return CalculateScreenBoxFromBBox(pPlayerController, vecOrigin, vecExtent);
}

namespace ESP
{
    // ESP Configuration
    enum class EActorType{
        // tazer mine, objective items, ammo boxes, fbi van, drones
        Other,
        Guard,
        Shield,
        Dozer,
        Cloaker,
        Sniper,
        Grenadier,
        Taser,
        Techie,
        Civilian,
        ObjectiveItem,
        InteractableItem,
        LootBag,
        
        Max
    };

    
    EActorType DetermineActorType(SDK::AActor* pActor){
        if (pActor->IsA(SDK::ACH_SWAT_SHIELD_C::StaticClass()))
            return EActorType::Shield;
        else if (pActor->IsA(SDK::ACH_Dozer_C::StaticClass()))
            return EActorType::Dozer;
        else if (pActor->IsA(SDK::ACH_Cloaker_C::StaticClass()))
            return EActorType::Cloaker;
        else if (pActor->IsA(SDK::ACH_Sniper_C::StaticClass()))
            return EActorType::Sniper;
        else if (pActor->IsA(SDK::ACH_Grenadier_C::StaticClass()))
            return EActorType::Grenadier;
        else if (pActor->IsA(SDK::ACH_Taser_C::StaticClass()))
            return EActorType::Taser;
        else if (pActor->IsA(SDK::ACH_Tower_C::StaticClass()))
            return EActorType::Techie;

        if(pActor->IsA(SDK::ACH_BaseCop_C::StaticClass()))
            return EActorType::Guard;

        if(pActor->IsA(SDK::ACH_BaseCivilian_C::StaticClass()))
            return EActorType::Civilian;

        static std::vector<SDK::UClass*> aObjectiveItemClasses = {
            SDK::ABP_RFIDTagBase_C::StaticClass(),
            SDK::ABP_KeycardBase_C::StaticClass(),
            SDK::ABP_CarriedInteractableBase_C::StaticClass(),
//            SDK::UGE_CarKeys_C::StaticClass(),
            SDK::UGA_Phone_C::StaticClass()
        };
        if(!std::none_of(aObjectiveItemClasses.begin(), aObjectiveItemClasses.end(), [pActor](SDK::UClass* pClass) { return pActor->IsA(pClass); }))
            return EActorType::ObjectiveItem;
        
        if(pActor->IsA(SDK::ASBZInteractionActor::StaticClass()))
            return EActorType::InteractableItem;

        if(pActor->IsA(SDK::ABP_BaseValuableBag_C::StaticClass()))
            return EActorType::LootBag;

        return EActorType::Other;        
    }

    void DrawSkeleton(SDK::USkeletalMeshComponent* pMeshComponent, SDK::APlayerController* pPlayerController, ImDrawList* pDrawList, ImU32 colSkeleton) {
        if (!pMeshComponent || !pPlayerController || !pDrawList)
            return;

        // Get calculated bone pairs
        const auto& bonePairs = CalculateGuardBonePairs(pMeshComponent);

        // Draw bone indices/names
        if (GetConfig().bDebugDrawBoneIndices) {
            // Create set of bone indices that appear in bone pairs
            std::unordered_set<int32_t> usedBones;
            for (const auto& pair : bonePairs) {
                // Exclude bone 0
                if (pair.ChildIndex == 0 || pair.ParentIndex == 0)
                    continue;
                    
                usedBones.insert(pair.ChildIndex);
                usedBones.insert(pair.ParentIndex);
            }
            
            for (int32_t i : usedBones) {
                SDK::FVector BonePos = pMeshComponent->GetSocketLocation(pMeshComponent->GetBoneName(i));
                SDK::FVector2D ScreenPos;
                if (pPlayerController->ProjectWorldLocationToScreen(BonePos, &ScreenPos, false)) {
                    if (GetConfig().bDebugDrawBoneNames) {
                        SDK::FName boneName = pMeshComponent->GetBoneName(i);
                        std::string nameStr = boneName.ToString();
                        pDrawList->AddText(
                            ImVec2(ScreenPos.X, ScreenPos.Y),
                            IM_COL32(0, 255, 0, 255),
                            nameStr.c_str()
                        );
                        continue;
                    }

                    char szIndex[16];
                    sprintf_s(szIndex, "%d", i);
                    pDrawList->AddText(
                        ImVec2(ScreenPos.X, ScreenPos.Y),
                        IM_COL32(0, 255, 0, 255),
                        szIndex
                    );
                }
            }
        }

        for (const auto& pair : bonePairs) {
            // Exclude bone 0
            if (pair.ChildIndex == 0 || pair.ParentIndex == 0)
                continue;

            // Get bone transforms in world space
            SDK::FVector ChildPos = pMeshComponent->GetSocketLocation(pMeshComponent->GetBoneName(pair.ChildIndex));
            SDK::FVector ParentPos = pMeshComponent->GetSocketLocation(pMeshComponent->GetBoneName(pair.ParentIndex));
            
            // Project to screen
            SDK::FVector2D ChildScreen, ParentScreen;
            if (pPlayerController->ProjectWorldLocationToScreen(ChildPos, &ChildScreen, false) &&
                pPlayerController->ProjectWorldLocationToScreen(ParentPos, &ParentScreen, false)) {
                
                // Draw line between bones
                pDrawList->AddLine(
                    ImVec2(ParentScreen.X, ParentScreen.Y),
                    ImVec2(ChildScreen.X, ChildScreen.Y),
                    colSkeleton,
                    2.0f
                );
            }
        }
    }

    void DrawDebugSkeleton(SDK::USkeletalMeshComponent* pMeshComponent, SDK::APlayerController* pPlayerController, ImDrawList* pDrawList) {
        if (!pMeshComponent || !pPlayerController || !pDrawList)
            return;
        
        // Draw bone indices/names
        if (GetConfig().bDebugSkeletonDrawBoneIndices) {
            int32_t BoneCount = pMeshComponent->GetNumBones();
            for (int32_t i = 0; i < BoneCount; i++) {
                SDK::FVector BonePos = pMeshComponent->GetSocketLocation(pMeshComponent->GetBoneName(i));
                SDK::FVector2D ScreenPos;
                if (pPlayerController->ProjectWorldLocationToScreen(BonePos, &ScreenPos, false)) {
                    if (GetConfig().bDebugSkeletonDrawBoneNames) {
                        SDK::FName boneName = pMeshComponent->GetBoneName(i);
                        std::string nameStr = boneName.ToString();
                        pDrawList->AddText(
                            ImVec2(ScreenPos.X, ScreenPos.Y),
                            IM_COL32(0, 255, 0, 255),
                            nameStr.c_str()
                        );
                        continue;
                    }

                    char szIndex[16];
                    sprintf_s(szIndex, "%d", i);
                    pDrawList->AddText(
                        ImVec2(ScreenPos.X, ScreenPos.Y),
                        IM_COL32(0, 255, 0, 255),
                        szIndex
                    );
                }
            }
        }
        
        // Draw debug skeleton using dynamically calculated bone pairs
        if (GetConfig().bDebugSkeleton) {
            const auto& pairs = GetOrCalculateBonePairs(pMeshComponent);
            
            for (const auto& pair : pairs) {
                // Get bone transforms in world space
                SDK::FVector ChildPos = pMeshComponent->GetSocketLocation(pMeshComponent->GetBoneName(pair.ChildIndex));
                SDK::FVector ParentPos = pMeshComponent->GetSocketLocation(pMeshComponent->GetBoneName(pair.ParentIndex));
                
                // Project to screen
                SDK::FVector2D ChildScreen, ParentScreen;
                if (pPlayerController->ProjectWorldLocationToScreen(ChildPos, &ChildScreen, false) &&
                    pPlayerController->ProjectWorldLocationToScreen(ParentPos, &ParentScreen, false)) {
                    
                    // Draw line between bones
                    pDrawList->AddLine(
                        ImVec2(ParentScreen.X, ParentScreen.Y),
                        ImVec2(ChildScreen.X, ChildScreen.Y),
                        IM_COL32(255, 0, 255, 255),
                        2.0f
                    );
                }
            }
        }
    }

    void DrawBar(ImDrawList* pDrawList, ImVec4 vec4ScreenBox, float flPercentage, ImU32 color, float flOffset = 4.f, float flWidth = 5.f)
    {
        if (flPercentage > 1.f)
            flPercentage = 1.f;
        if (flPercentage <= 0.f)
            flPercentage = 0.f;

        float flHeight = vec4ScreenBox.w - vec4ScreenBox.y;
        float flBarX = vec4ScreenBox.x - (flWidth + flOffset);

        // Background
        pDrawList->AddRectFilled(
            ImVec2(flBarX - 1, vec4ScreenBox.y - 1),
            ImVec2(flBarX + flWidth + 1, vec4ScreenBox.y + flHeight + 1),
            IM_COL32(30, 30, 30, 55)
        );

        // Fill
        pDrawList->AddRectFilled(
            ImVec2(flBarX, vec4ScreenBox.y + flHeight * (1.0f - flPercentage)),
            ImVec2(flBarX + flWidth, vec4ScreenBox.y + flHeight),
            color
        );
    }

    void DrawName(ImDrawList* pDrawList, ImVec4 vec4ScreenBox, SDK::AActor* pActor, EActorType eType) {
        std::string sCharacterName = "";
        switch(eType){
        case EActorType::Guard:
            sCharacterName = "Pig";
            break;
        case EActorType::Shield:
            sCharacterName = "Shield";
            break;
        case EActorType::Dozer:
            sCharacterName = "Dozer";
            break;
        case EActorType::Cloaker:
            sCharacterName = "Cloaker";
            break;
        case EActorType::Sniper:
            sCharacterName = "Sniper";
            break;
        case EActorType::Grenadier:
            sCharacterName = "Grenadier";
            break;
        case EActorType::Taser:
            sCharacterName = "Taser";
            break;
        case EActorType::Techie:
            sCharacterName = "Techie";
            break;
        default:
            sCharacterName = pActor->GetName();
            break;
        }

        if (!sCharacterName.size())
            return;

        ImVec2 vec2TextSize = ImGui::CalcTextSize(sCharacterName.c_str());
        ImVec2 vec2TextPos = ImVec2{(vec4ScreenBox.z - vec4ScreenBox.x - vec2TextSize.x) / 2.f + vec4ScreenBox.x, vec4ScreenBox.y - vec2TextSize.y - 4.f};
        
        pDrawList->AddText(ImVec2{ vec2TextPos.x + 1.f, vec2TextPos.y + 1.f }, IM_COL32(30, 30, 30, 55), sCharacterName.c_str());
        pDrawList->AddText(vec2TextPos, IM_COL32(255, 255, 255, 200), sCharacterName.c_str());
    };

    void DrawCarryableFlags(ImDrawList* pDrawList, SDK::AActor* pActor, ImVec4 vec4ScreenBox, float& flFlagsOffset){
        static auto nameBP_CarriedBlueKeycard_C = SDK::UKismetStringLibrary::Conv_StringToName(L"BP_CarriedBlueKeycard_C");
        static auto nameBP_CarriedRedKeycard_C = SDK::UKismetStringLibrary::Conv_StringToName(L"BP_CarriedRedKeycard_C");
        static auto nameBP_CarriedFakeID_C = SDK::UKismetStringLibrary::Conv_StringToName(L"BP_CarriedFakeID_C");
        static auto nameBP_CarriedHackablePhone_C = SDK::UKismetStringLibrary::Conv_StringToName(L"BP_CarriedHackablePhone_C");

        auto& aChildren = pActor->Children;
        for(int i = 0; i < aChildren.Num(); i++){
            auto pChild = reinterpret_cast<SDK::ASBZCarriedStaticInteractionActor*>(aChildren[i]);
            if(!pChild || !pChild->IsA(SDK::ASBZCarriedStaticInteractionActor::StaticClass()))
                continue;

            auto nameItem = pChild->Class->Name;
            pDrawList->AddText(
                ImVec2(vec4ScreenBox.z + 5.f, vec4ScreenBox.y + flFlagsOffset),
                IM_COL32(0, 255, 0, 255),
                ((nameItem == nameBP_CarriedBlueKeycard_C) ? "Blue Keycard" :
                (nameItem == nameBP_CarriedRedKeycard_C) ? "Red Keycard" :
                (nameItem == nameBP_CarriedFakeID_C) ? "Fake ID" :
                (nameItem == nameBP_CarriedHackablePhone_C) ? "Phone" :
                nameItem.ToString()).c_str());
            flFlagsOffset += 15.f;
        }
    }

    // Colored corner brackets (ImGui "highlight") — game Multicast_SetMarked has no color.
    void DrawHighlightCorners(ImDrawList* pDrawList, const ImVec4& box, ImU32 col, float thickness = 2.0f, float lenScale = 0.25f)
    {
        const float w = box.z - box.x;
        const float h = box.w - box.y;
        const float len = (std::min)(w, h) * lenScale;
        const float t = thickness;
        // TL
        pDrawList->AddLine(ImVec2(box.x, box.y), ImVec2(box.x + len, box.y), col, t);
        pDrawList->AddLine(ImVec2(box.x, box.y), ImVec2(box.x, box.y + len), col, t);
        // TR
        pDrawList->AddLine(ImVec2(box.z, box.y), ImVec2(box.z - len, box.y), col, t);
        pDrawList->AddLine(ImVec2(box.z, box.y), ImVec2(box.z, box.y + len), col, t);
        // BL
        pDrawList->AddLine(ImVec2(box.x, box.w), ImVec2(box.x + len, box.w), col, t);
        pDrawList->AddLine(ImVec2(box.x, box.w), ImVec2(box.x, box.w - len), col, t);
        // BR
        pDrawList->AddLine(ImVec2(box.z, box.w), ImVec2(box.z - len, box.w), col, t);
        pDrawList->AddLine(ImVec2(box.z, box.w), ImVec2(box.z, box.w - len), col, t);
    }

    // Freecam-style strong loot frame: dim fill + thick corners + thin full rect.
    void DrawStrongLootFrame(ImDrawList* pDrawList, const ImVec4& box, ImU32 col)
    {
        const ImU32 fill = (col & 0x00FFFFFF) | 0x28000000;
        pDrawList->AddRectFilled(ImVec2(box.x, box.y), ImVec2(box.z, box.w), fill);
        pDrawList->AddRect(ImVec2(box.x, box.y), ImVec2(box.z, box.w), col, 0.f, 0, 1.5f);
        DrawHighlightCorners(pDrawList, box, col, 3.5f, 0.35f);
    }

    void DrawEnemyESP(ImDrawList* pDrawList, SDK::APlayerController* pPlayerController, SDK::ACH_BaseCop_C* pGuard, EActorType eType, EnemyESP& stSettings){
        static auto nameCH_SecurityGuard_Lead_C = SDK::UKismetStringLibrary::Conv_StringToName(L"CH_SecurityGuard_Lead_C"); 
        pGuard->Multicast_SetMarked(stSettings.m_bOutline);
        if (stSettings.m_bOutline)
            ApplyActorOutline(pGuard, kOutlineRed);

        const auto& cols = GetConfig().m_colors;

        SDK::USkeletalMeshComponent* pSkeletalMesh = pGuard->Mesh;
        if (!pSkeletalMesh)
            return;

        // Draw ESP features
        if (stSettings.m_bSkeleton) {
            DrawSkeleton(pSkeletalMesh, pPlayerController, pDrawList, cols.m_colSkeleton);
            DrawDebugSkeleton(pSkeletalMesh, pPlayerController, pDrawList);
        }
        
        if (auto optScreenBox = CalculateScreenBoxForGuard(pSkeletalMesh, pPlayerController, pGuard); optScreenBox.has_value())
        {
            auto vec4ScreenBox = optScreenBox.value();
            if (stSettings.m_bBox){
                pDrawList->AddRect(ImVec2(vec4ScreenBox.x - 1, vec4ScreenBox.y - 1), ImVec2(vec4ScreenBox.z + 1, vec4ScreenBox.w + 1), IM_COL32(30, 30, 30, 55), 0.0f, 0, 2.0f);
                pDrawList->AddRect(ImVec2(vec4ScreenBox.x, vec4ScreenBox.y), ImVec2(vec4ScreenBox.z, vec4ScreenBox.w), cols.m_colBox, 0.0f, 0, 2.0f);
            }

            if (stSettings.m_bOutline)
                DrawHighlightCorners(pDrawList, vec4ScreenBox, cols.m_colHighlight);
                
            if (stSettings.m_bName)
                DrawName(pDrawList, vec4ScreenBox, pGuard, eType);

            float flBarOffset = 4.f;
            if (stSettings.m_bHealth){
                DrawBar(pDrawList, vec4ScreenBox, pGuard->AttributeSet->Health.CurrentValue / pGuard->AttributeSet->HealthMax.CurrentValue, cols.m_colHealth, flBarOffset);
                flBarOffset += 8.f;
            }

            if (stSettings.m_bArmor && pGuard->AttributeSet->Armor.CurrentValue > std::numeric_limits<float>::epsilon()){
                DrawBar(pDrawList, vec4ScreenBox, pGuard->AttributeSet->Armor.CurrentValue / pGuard->AttributeSet->ArmorMax.CurrentValue, cols.m_colArmor, flBarOffset);
                flBarOffset += 8.f;
            }

            if (!stSettings.m_bFlags)
                return;
            
            float flFlagsOffset = 0.f;
            if (stSettings.m_bFlags)
                DrawCarryableFlags(pDrawList, pGuard, vec4ScreenBox, flFlagsOffset);

            if(stSettings.m_bFlags && pGuard->Class->Name == nameCH_SecurityGuard_Lead_C){
                pDrawList->AddText(ImVec2(vec4ScreenBox.z + 5.f, vec4ScreenBox.y + flFlagsOffset), cols.m_colKeyItems, "Lead");
                flFlagsOffset += 15.f;
            }
        }  
    };

    void DrawInteractableESP(ImDrawList* pDrawList, SDK::APlayerController* pPlayerController, SDK::ASBZInteractionActor* pItem, EActorType eType){
        if (!pItem)
            return;

        if (auto optScreenBox = CalculateScreenBoxUsingBounds(pPlayerController, pItem); optScreenBox.has_value())
        {
            auto vec4ScreenBox = optScreenBox.value();
            DrawName(pDrawList, vec4ScreenBox, pItem, eType);
        }
    };

    void DrawCivilianESP(ImDrawList* pDrawList, SDK::APlayerController* pPlayerController, SDK::ACH_BaseCivilian_C* pCivilian, CivilianESP& stSettings){
        static auto nameCH_Civilian_Employee_Male_01_C = SDK::UKismetStringLibrary::Conv_StringToName(L"CH_Civilian_Bank_Manager_male_01_C");
        static auto nameCH_Civilian_Employee_Female_01_C = SDK::UKismetStringLibrary::Conv_StringToName(L"CH_Civilian_Bank_Manager_female_01_C");
        static auto nameCH_Civilian_PH_VIP_C = SDK::UKismetStringLibrary::Conv_StringToName(L"CH_Civilian_PH_VIP_C");

        auto nameCiv = pCivilian->Class->Name;
        bool bIsVIP = nameCiv == nameCH_Civilian_Employee_Male_01_C || nameCiv == nameCH_Civilian_Employee_Female_01_C || nameCiv == nameCH_Civilian_PH_VIP_C;
        bool bShouldDraw = true;
        if(stSettings.m_bOnlyWhenSpecial){
            bShouldDraw = false;

            auto& aChildren = pCivilian->Children;
            for(int i = 0; i < aChildren.Num(); i++){
                auto pChild = reinterpret_cast<SDK::ASBZCarriedStaticInteractionActor*>(aChildren[i]);
                if(!pChild || !pChild->IsA(SDK::ASBZCarriedStaticInteractionActor::StaticClass()))
                    continue;

                bShouldDraw = true;
                break;
            }

            bShouldDraw |= bIsVIP;
        }

        pCivilian->Multicast_SetMarked(stSettings.m_bOutline && bShouldDraw);
        if (stSettings.m_bOutline && bShouldDraw)
            ApplyActorOutline(pCivilian, kOutlineWhite);
        
        const auto& cols = GetConfig().m_colors;

        SDK::USkeletalMeshComponent* pSkeletalMesh = pCivilian->Mesh;
        if (!pSkeletalMesh || !bShouldDraw)
            return;

        // Draw ESP features
        if (stSettings.m_bSkeleton) {
            DrawSkeleton(pSkeletalMesh, pPlayerController, pDrawList, cols.m_colSkeleton);
            DrawDebugSkeleton(pSkeletalMesh, pPlayerController, pDrawList);
        }
        
        if (auto optScreenBox = CalculateScreenBoxForGuard(pSkeletalMesh, pPlayerController, pCivilian); optScreenBox.has_value())
        {
            auto vec4ScreenBox = optScreenBox.value();
            if (stSettings.m_bBox){
                pDrawList->AddRect(ImVec2(vec4ScreenBox.x - 1, vec4ScreenBox.y - 1), ImVec2(vec4ScreenBox.z + 1, vec4ScreenBox.w + 1), IM_COL32(30, 30, 30, 55), 0.0f, 0, 2.0f);
                pDrawList->AddRect(ImVec2(vec4ScreenBox.x, vec4ScreenBox.y), ImVec2(vec4ScreenBox.z, vec4ScreenBox.w), cols.m_colBox, 0.0f, 0, 2.0f);
            }

            if (stSettings.m_bOutline)
                DrawHighlightCorners(pDrawList, vec4ScreenBox, cols.m_colHighlight);

            if (!stSettings.m_bFlags)
                return;
            
            float flFlagsOffset = 0.f;
            if(bIsVIP){
                pDrawList->AddText(ImVec2(vec4ScreenBox.z + 5.f, vec4ScreenBox.y + flFlagsOffset), IM_COL32(0, 200, 255, 255), "VIP");
                flFlagsOffset += 15.f;
            }

            DrawCarryableFlags(pDrawList, pCivilian, vec4ScreenBox, flFlagsOffset);
        }  
    };

    bool ShouldSkipActor(SDK::AActor* pActor, EActorType eType){
        switch(eType){
        case EActorType::Guard:
        case EActorType::Shield:
        case EActorType::Dozer:
        case EActorType::Cloaker:
        case EActorType::Sniper:
        case EActorType::Grenadier:
        case EActorType::Taser:
        case EActorType::Techie:
        case EActorType::Civilian:
            return !reinterpret_cast<SDK::ASBZCharacter*>(pActor)->bIsAlive;

        default:
            break;
        }
        return false;
    }

    struct ActorInfo{
        EActorType m_eType;
        float m_flDistance;
        SDK::AActor* m_pActor;
    };

    SDK::FGameplayAbilitySpec* GetAbilitySpec(SDK::USBZPlayerAbilitySystemComponent* pAbilitySystem, const SDK::FName& nameAbility){
        if (!pAbilitySystem)
            return nullptr;

        auto& aAbilities = pAbilitySystem->ActivatableAbilities.Items;
        for (int i = 0; i < aAbilities.Num(); ++i) {
            if (!aAbilities.IsValidIndex(i))
                continue;

            auto pAbility = aAbilities[i].Ability;
            if (!pAbility)
                continue;

            if (pAbility->Name == nameAbility)
                return &aAbilities[i];
        }

        return nullptr;
    }

    void DrawFovCircle(SDK::UWorld* pGWorld, SDK::APlayerController* pPlayerController, ImDrawList* pDrawList){
        const auto& stConfig = CheatConfig::Get().m_aimbot;
        ImVec2 vec2ScreenSize = CheatConfig::Get().m_misc.vec2ScreenSize;

        if(stConfig.m_flAimFOV <= 0.f)
            return;

        ImVec2 screenCenter = ImVec2(vec2ScreenSize.x / 2.f, vec2ScreenSize.y / 2.f);

        // At 90 FOV, the edge of the screen is at screenWidth/2 pixels from center.
        // So radius = (aimFOV / 90.f) * (screenWidth / 2.f)
        float flRadius = (stConfig.m_flAimFOV / 90.f) * (vec2ScreenSize.x / 2.f);

        pDrawList->AddCircle(screenCenter, flRadius, GetConfig().m_colors.m_colFovCircle, 64, 2.0f);
    }

    void Render(SDK::UWorld* pGWorld, SDK::APlayerController* pPlayerController) {
        if(!Cheat::g_bIsInGame)
            return;

        const bool bCops = GetConfig().bESP;
        const bool bLootImGui = false;
        const bool bHelpers = GetConfig().bPagerHud
            || GetConfig().bBagZones || GetConfig().bDrawFovCircle;
        // Loot glow is V2 ApplyActorOutline on the game thread (TickCheapGlow). Do not walk loot from Present.
        if (!bCops && !bLootImGui && !bHelpers)
            return;

        SDK::USBZWorldRuntime* pWorldRuntime = reinterpret_cast<SDK::USBZWorldRuntime*>(SDK::USBZWorldRuntime::GetWorldRuntime(pGWorld));
        if (!pWorldRuntime || !pGWorld->PersistentLevel || !pGWorld->PersistentLevel->Actors)
            return;

        if(!pPlayerController->PlayerCameraManager)
            return;

        SDK::FRotator rotCameraRotation = pPlayerController->PlayerCameraManager->GetCameraRotation().Normalize();
        ImDrawList* pDrawList = ImGui::GetBackgroundDrawList();
        
        UC::TArray<SDK::UObject*>& actors = pWorldRuntime->AllPawns->Objects;
        UC::TArray<SDK::UObject*>& aliveActors = pWorldRuntime->AllAlivePawns->Objects;

        SDK::FVector vecCameraLocation = pPlayerController->PlayerCameraManager->GetCameraLocation();
        std::vector<ActorInfo> vecActors{};

        static auto nameHead = SDK::UKismetStringLibrary::Conv_StringToName(L"Head");
        
        vecActors.reserve(actors.Num());
        for (int i = 0; i < actors.Num(); ++i){
            if (!actors.IsValidIndex(i))
                break;

            auto pActor = reinterpret_cast<SDK::AActor*>(actors[i]);
            if(!pActor)
                continue;

            auto eType = DetermineActorType(pActor);
            if (ShouldSkipActor(pActor, eType))
                continue;

            vecActors.emplace_back(ActorInfo{
                .m_eType = eType,
                .m_flDistance = (pActor->K2_GetActorLocation() - vecCameraLocation).Magnitude(),
                .m_pActor = pActor
            });
        }

        std::sort(vecActors.begin(), vecActors.end(), [](ActorInfo& lhs, ActorInfo& rhs) {
            if (lhs.m_flDistance == rhs.m_flDistance)
                rhs.m_flDistance += 0.001f;

            return lhs.m_flDistance > rhs.m_flDistance;
        });

        if (GetConfig().bDrawFovCircle)
        {
            DrawFovCircle(pGWorld, pPlayerController, pDrawList);
        }

        // Loot glow is TickCheapGlow (V2 outlines). Never walk loot actors from Present.
        if (false && LootESP::GetConfig().bLootESP)
        {
            const auto& lootCfg = LootESP::GetConfig();
            const bool bDoGlow = lootCfg.bStrongGlow;
            const bool bDoImGui = lootCfg.bOutline || lootCfg.bLabels;
            if (bDoGlow || bDoImGui)
            {
            const auto& cols = GetConfig().m_colors;
            BeginOutlineFrame();

            enum class ELootColorKind { Key, Money, Chem };

            auto ToLower = [](std::string s)
            {
                for (char& c : s)
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                return s;
            };

            auto GetInteractable = [](SDK::AActor* pActor) -> SDK::USBZInteractableComponent*
            {
                if (!pActor)
                    return nullptr;
                if (pActor->IsA(SDK::ASBZInstantLoot::StaticClass()))
                    return reinterpret_cast<SDK::ASBZInstantLoot*>(pActor)->Interactable;
                if (pActor->IsA(SDK::ASBZSingleBagGenerator::StaticClass()))
                    return reinterpret_cast<SDK::ASBZSingleBagGenerator*>(pActor)->Interactable;
                if (pActor->IsA(SDK::ASBZMultiBagGenerator::StaticClass()))
                    return reinterpret_cast<SDK::ASBZMultiBagGenerator*>(pActor)->InteractableComponent;
                if (pActor->IsA(SDK::ASBZBagItem::StaticClass()))
                    return reinterpret_cast<SDK::ASBZBagItem*>(pActor)->Interactable;
                if (pActor->IsA(SDK::ABP_WoodenCrate_C::StaticClass()))
                    return reinterpret_cast<SDK::ABP_WoodenCrate_C*>(pActor)->SBZInteractable;
                if (pActor->IsA(SDK::ABP_LiquidNitrogenCanister_C::StaticClass()))
                    return reinterpret_cast<SDK::ABP_LiquidNitrogenCanister_C*>(pActor)->Interactable;
                // Keycards / RFID / phones inherit this (Interactable at 0x2A8).
                if (pActor->IsA(SDK::ASBZInteractionActor::StaticClass()))
                    return reinterpret_cast<SDK::ASBZInteractionActor*>(pActor)->Interactable;
                return reinterpret_cast<SDK::USBZInteractableComponent*>(
                    pActor->GetComponentByClass(SDK::USBZInteractableComponent::StaticClass()));
            };

            // Hard rule: no ESP unless the game says you can interact with it right now.
            auto IsCurrentlyInteractable = [&](SDK::AActor* pActor) -> bool
            {
                SDK::USBZInteractableComponent* pInt = GetInteractable(pActor);
                return pInt && pInt->bInteractionEnabled;
            };

            auto LabelForActor = [&](SDK::AActor* pActor) -> const char*
            {
                if (!pActor || !pActor->Class)
                    return nullptr;

                const std::string low = ToLower(pActor->Class->Name.ToString());
                const std::string actorLow = ToLower(pActor->GetName());

                auto IsJunk = [&](const std::string& s) -> bool
                {
                    return s.contains("bucket") || s.contains("gallon") || s.contains("toast")
                        || s.contains("mop") || s.contains("broom") || s.contains("trash")
                        || s.contains("debris") || s.contains("decal") || s.contains("clutter")
                        || s.contains("prop_tool") || s.contains("cleaning")
                        || s.contains("brush") || s.contains("roller") || s.contains("tray")
                        || s.contains("paintbrush") || s.contains("paintroller") || s.contains("painttray")
                        || s.contains("paintcan") || s.contains("paint_can") || s.contains("ladder")
                        || s.contains("scaffold") || s.contains("tarp") || s.contains("dropcloth")
                        || s.contains("light") || s.contains("lamp") || s.contains("fixture")
                        || s.contains("sconce") || s.contains("bulb") || s.contains("spotlight")
                        || s.contains("luminaire") || s.contains("walllight") || s.contains("ceilinglight")
                        || s.contains("picturelight") || s.contains("gallerylight") || s.contains("tracklight")
                        || s.contains("ringlight") || s.contains("deco") || s.contains("decoration")
                        || s.contains("ornament") || s.contains("furniture");
                };
                if (IsJunk(low) || IsJunk(actorLow))
                    return nullptr;
                if (low.contains("computer") || low.contains("cooking") || low.contains("cookstation")
                    || low.contains("methcook"))
                    return nullptr;

                if (pActor->Class->Name == FNames::BP_QRPhone_C) return "Phone";
                if (pActor->Class->Name == FNames::BP_BlueKeycard_C) return "Blue Keycard";
                if (pActor->Class->Name == FNames::BP_RedKeycard_C) return "Red Keycard";
                if (pActor->Class->Name == FNames::BP_RFIDTagBlue_C) return "RFID Tag";
                if (pActor->Class->Name == FNames::BP_DAT_C4Explosive_01_Pickup_C) return "C4";
                if (pActor->Class->Name == FNames::BP_Meth_CausticSoda_C) return "Caustic Soda";
                if (pActor->Class->Name == FNames::BP_Meth_MuriaticAcid_C) return "Muriatic Acid";
                if (pActor->Class->Name == FNames::BP_Meth_HydrogenChloride_C) return "Hydrogen Chloride";
                if (pActor->Class->Name == FNames::BP_FOR_USBDrive_C) return "USB Drive";
                if (pActor->Class->Name == FNames::BP_Plankspile_C) return "Planks";

                // Access items freecam F4 tracks (green/yellow cards, RFID red, carried, etc.)
                if (low.contains("greenkeycard")) return "Green Keycard";
                if (low.contains("yellowkeycard")) return "Yellow Keycard";
                if (low.contains("bluekeycard") || low == "bp_carriedbluekeycard_c") return "Blue Keycard";
                if (low.contains("redkeycard") || low == "bp_carriedredkeycard_c") return "Red Keycard";
                if (low.contains("rfid") || low.contains("authenticator")) return "RFID Tag";
                if (low.contains("qrphone") || low.contains("hackphone")) return "Phone";
                if (low.contains("keycard") && !low.contains("reader") && !low.contains("requirement")
                    && !low.contains("cosmetic"))
                    return "Keycard";
                if (low.contains("usbdrive") || low.contains("usb_drive")) return "USB Drive";
                if (low.contains("c4") && (low.contains("pickup") || low.contains("explosive")))
                    return "C4";

                if (low.contains("interactablemoneypile") || low.contains("moneypile")
                    || low.contains("money_pile") || low.contains("moneycart")
                    || low.contains("loosecash") || low.contains("instantloot_money")
                    || low.contains("instantlootmoney") || low.contains("moneybagdyed")
                    || low.contains("dyed") || (low.contains("money") && low.contains("dye")))
                    return "Money";

                if (low.contains("liquidnitrogen") || (low.contains("nitrogen") && low.contains("canister")))
                    return "Liquid Nitrogen";
                if (low.contains("woodencrate") || low == "bp_woodencrate_c")
                    return "Crate";

                // Paintings: ONLY lootable painting classes — never wall art / picture lights.
                if (low.contains("interactablepainting") || low.contains("paintingbag")
                    || low == "bp_painting_c")
                    return "Painting";

                if (low.contains("cocaine") || low.contains("cokepile") || low.contains("cocainepile"))
                    return "Coke";

                // Pickup servers / evidence / rare mission loot (Surphaze, data centers, etc.)
                if (low.contains("interactableserver") || low.contains("datacenterserver")
                    || (low.contains("server") && low.contains("interactable")))
                    return "Server";
                if (low.contains("chus_evidence") || low.contains("evidence")
                    || low.contains("evidencebag"))
                    return "Evidence";
                if (low.contains("rarestone") || low.contains("raremineral") || low.contains("interactableopal")
                    || low.contains("satellitepart") || low.contains("satelliteprototype"))
                    return "Rare Loot";

                auto LooksLikeInstantValuable = [](const std::string& s) -> bool
                {
                    return s.contains("money") || s.contains("cash") || s.contains("jewel")
                        || s.contains("gold") || s.contains("diamond") || s.contains("watch")
                        || s.contains("necklace") || s.contains("bracelet") || s.contains("earring")
                        || s.contains("coke") || s.contains("cocaine")
                        || s.contains("jewelry") || s.contains("jewellery")
                        || s.contains("server") || s.contains("evidence")
                        || s.contains("rarestone") || s.contains("opal") || s.contains("satellite");
                };

                if (pActor->IsA(SDK::ASBZInstantLoot::StaticClass()))
                {
                    if (!LooksLikeInstantValuable(low) && !LooksLikeInstantValuable(actorLow))
                        return nullptr;
                    if (low.contains("coke") || low.contains("cocaine") || actorLow.contains("coke"))
                        return "Coke";
                    if (low.contains("jewel") || low.contains("diamond") || low.contains("watch")
                        || low.contains("necklace") || low.contains("bracelet") || low.contains("earring")
                        || low.contains("gold") || low.contains("jewelry"))
                        return "Jewelry";
                    return "Cash";
                }

                if (pActor->IsA(SDK::ASBZSingleBagGenerator::StaticClass()))
                {
                    if (low.contains("interactablepainting") || low == "bp_painting_c"
                        || low.contains("paintingbag"))
                        return "Painting";
                    if (low.contains("interactablemoneypile") || low.contains("moneypile")
                        || low.contains("moneycart") || low.contains("money") || low.contains("cash")
                        || low.contains("dyed") || low.contains("dye"))
                        return "Money";
                    if (low.contains("jewel") || low.contains("jewelry") || low.contains("gold"))
                        return "Jewelry";
                    if (low.contains("coke") || low.contains("cocaine"))
                        return "Coke";
                    if (low.contains("interactableserver") || low.contains("datacenterserver")
                        || (low.contains("server") && !low.contains("serverroom")))
                        return "Server";
                    if (low.contains("chus_evidence") || low.contains("evidence"))
                        return "Evidence";
                    if (low.contains("rarestone") || low.contains("raremineral") || low.contains("opal")
                        || low.contains("satellite"))
                        return "Rare Loot";
                    return nullptr;
                }

                if (pActor->IsA(SDK::ASBZMultiBagGenerator::StaticClass()))
                {
                    if (low.contains("cocaine") || low.contains("coke"))
                        return "Coke";
                    if (low.contains("money") || low.contains("cash"))
                        return "Money";
                    return nullptr;
                }

                if (pActor->IsA(SDK::ABP_BaseValuableBag_C::StaticClass()))
                {
                    if (low.contains("money") || low.contains("cash") || low.contains("dyed")
                        || low.contains("dye"))
                        return "Money Bag";
                    if (low.contains("coke") || low.contains("cocaine"))
                        return "Coke Bag";
                    if (low.contains("gold") || low.contains("jewelry") || low.contains("jewel"))
                        return "Jewelry";
                    if (low.contains("meth"))
                        return "Meth Bag";
                    if (low.contains("painting"))
                        return "Painting Bag";
                    if (low.contains("bag") || low.contains("valuable"))
                        return "Bag";
                    return nullptr;
                }
                // Loose name match for dyed cash piles freecam lists as BP_MoneyBagDyed_C
                // (may not be in this SDK dump but still exists in-heist).
                if (low.contains("moneybagdyed") || low.contains("dyedmoney")
                    || (low.contains("dyed") && (low.contains("money") || low.contains("cash") || low.contains("bag"))))
                    return "Money";
                return nullptr;
            };

            auto ColorKindFor = [&](SDK::AActor* pActor, const char* szLabel) -> ELootColorKind
            {
                if (pActor->Class->Name == FNames::BP_Meth_CausticSoda_C
                    || pActor->Class->Name == FNames::BP_Meth_MuriaticAcid_C
                    || pActor->Class->Name == FNames::BP_Meth_HydrogenChloride_C)
                    return ELootColorKind::Chem;

                const std::string low = ToLower(pActor->Class->Name.ToString());
                if (low.contains("nitrogen"))
                    return ELootColorKind::Chem;
                if (low.contains("meth") && !low.contains("money"))
                    return ELootColorKind::Chem;

                if (szLabel)
                {
                    const std::string lab = ToLower(szLabel);
                    if (lab.contains("nitrogen"))
                        return ELootColorKind::Chem;
                    if (lab.contains("money") || lab.contains("cash") || lab.contains("coke")
                        || lab.contains("bag") || lab.contains("jewelry") || lab.contains("painting")
                        || lab.contains("crate") || lab.contains("server") || lab.contains("evidence")
                        || lab.contains("rare"))
                        return ELootColorKind::Money;
                }
                return ELootColorKind::Key;
            };

            auto ColorForKind = [&](ELootColorKind kind) -> ImU32
            {
                switch (kind)
                {
                case ELootColorKind::Money: return cols.m_colMoney;
                case ELootColorKind::Chem: return cols.m_colChem;
                default: return cols.m_colKeyItems;
                }
            };

            auto StillLootable = [&](SDK::AActor* pActor) -> bool
            {
                if (!pActor || !pActor->Class)
                    return false;

                const std::string low = ToLower(pActor->Class->Name.ToString());

                if (pActor->IsA(SDK::ASBZInstantLoot::StaticClass()))
                {
                    auto* pLoot = reinterpret_cast<SDK::ASBZInstantLoot*>(pActor);
                    if (pLoot->bIsLooted)
                        return false;
                }
                if (pActor->IsA(SDK::ASBZMultiBagGenerator::StaticClass()))
                {
                    auto* pMulti = reinterpret_cast<SDK::ASBZMultiBagGenerator*>(pActor);
                    if (pMulti->NumberOfBags <= 0)
                        return false;
                }
                if (pActor->IsA(SDK::ASBZReplicatedBinaryStateActor::StaticClass()))
                {
                    if (low.contains("woodencrate") || low.contains("nitrogen"))
                    {
                        auto* pBin = reinterpret_cast<SDK::ASBZReplicatedBinaryStateActor*>(pActor);
                        if (pBin->bState)
                            return false;
                    }
                }

                // Non-negotiable for most loot: must be interactable right now.
                // Cash / bags / dye: interact flag is often false but still lootable.
                if (!IsCurrentlyInteractable(pActor))
                {
                    if (pActor->IsA(SDK::ASBZSingleBagGenerator::StaticClass())
                        || pActor->IsA(SDK::ASBZInstantLoot::StaticClass())
                        || pActor->IsA(SDK::ASBZMultiBagGenerator::StaticClass())
                        || pActor->IsA(SDK::ABP_BaseValuableBag_C::StaticClass())
                        || pActor->IsA(SDK::ASBZBagItem::StaticClass())
                        || pActor->IsA(SDK::ABP_KeycardBase_C::StaticClass())
                        || pActor->IsA(SDK::ABP_RFIDTagBase_C::StaticClass())
                        || pActor->IsA(SDK::ABP_QRPhone_C::StaticClass())
                        || pActor->IsA(SDK::ABP_CarriedInteractableBase_C::StaticClass()))
                        return true;
                    if (low.contains("money") || low.contains("cash") || low.contains("dye")
                        || low.contains("dyed") || low.contains("pile") || low.contains("bag")
                        || low.contains("loot") || low.contains("jewel") || low.contains("gold")
                        || low.contains("coke") || low.contains("server") || low.contains("evidence")
                        || low.contains("keycard") || low.contains("rfid") || low.contains("phone")
                        || low.contains("usb") || low.contains("c4") || low.contains("plank"))
                        return true;
                    return false;
                }

                return true;
            };

            auto TryApplyGameOutline = [](SDK::AActor* pActor, bool bStrong, int8_t colorIndex)
            {
                if (bStrong)
                {
                    ApplyFreecamStyleLootOutline(pActor, colorIndex);
                    return;
                }

                SDK::USBZOutlineComponent* pOutline = nullptr;
                if (pActor->IsA(SDK::ASBZInstantLoot::StaticClass()))
                    pOutline = reinterpret_cast<SDK::ASBZInstantLoot*>(pActor)->OutlineComponent;
                if (!pOutline && pActor->IsA(SDK::ASBZSingleBagGenerator::StaticClass()))
                    pOutline = reinterpret_cast<SDK::ASBZSingleBagGenerator*>(pActor)->Outline;
                if (!pOutline && pActor->IsA(SDK::ASBZMultiBagGenerator::StaticClass()))
                    pOutline = reinterpret_cast<SDK::ASBZMultiBagGenerator*>(pActor)->OutlineComponent;
                if (!pOutline)
                    pOutline = reinterpret_cast<SDK::USBZOutlineComponent*>(
                        pActor->GetComponentByClass(SDK::USBZOutlineComponent::StaticClass()));
                if (!pOutline)
                    return;

                SDK::USBZOutlineAsset* pAsset = pOutline->DefaultAsset;
                if (!pAsset)
                    pAsset = pOutline->InteractableFocusAsset;
                if (!pAsset)
                    pAsset = pOutline->ActiveAsset;
                if (!pAsset)
                    pAsset = pOutline->ActiveReplicated;
                if (pAsset)
                    pOutline->Multicast_SetActiveReplicated(pAsset);
            };

            // Auto-sized loot brackets: mesh bounds when possible, clamp huge collision,
            // then distance-scale so keys stay small and bags stay readable.
            auto ScreenBoxForLoot = [&](SDK::AActor* pActor, ELootColorKind kind) -> std::optional<ImVec4>
            {
                if (!pActor || !pPlayerController->PlayerCameraManager)
                    return std::nullopt;

                SDK::FVector origin = pActor->K2_GetActorLocation();
                SDK::FVector extent{ 18.f, 18.f, 22.f };
                bool bHaveMesh = false;

                // Prefer visible mesh tight bounds (actor bounds often include huge interact volumes).
                auto TryMeshClass = [&](SDK::UClass* pClass)
                {
                    if (!pClass)
                        return;
                    auto comps = pActor->K2_GetComponentsByClass(pClass);
                    for (int i = 0; i < comps.Num(); ++i)
                    {
                        if (!comps.IsValidIndex(i))
                            break;
                        auto* pScene = reinterpret_cast<SDK::USceneComponent*>(comps[i]);
                        if (!pScene || !pScene->IsVisible())
                            continue;

                        const SDK::FBoxSphereBounds tb = pScene->GetTightBounds(false);
                        const float mx = (std::max)(tb.BoxExtent.X, (std::max)(tb.BoxExtent.Y, tb.BoxExtent.Z));
                        if (mx < 2.f || mx > 220.f)
                            continue; // empty or nonsense collision-sized mesh

                        if (!bHaveMesh || mx < (std::max)(extent.X, (std::max)(extent.Y, extent.Z)))
                        {
                            origin = tb.Origin;
                            extent = tb.BoxExtent;
                            bHaveMesh = true;
                        }
                    }
                };
                TryMeshClass(SDK::UStaticMeshComponent::StaticClass());
                TryMeshClass(SDK::USkeletalMeshComponent::StaticClass());

                if (!bHaveMesh)
                {
                    SDK::FVector actorOrigin{}, actorExtent{};
                    pActor->GetActorBounds(true, &actorOrigin, &actorExtent, false);
                    const float mx = (std::max)(actorExtent.X, (std::max)(actorExtent.Y, actorExtent.Z));
                    if (mx >= 5.f && mx <= 160.f)
                    {
                        origin = actorOrigin;
                        extent = actorExtent;
                    }
                    else
                    {
                        // Type-based defaults when bounds are useless.
                        switch (kind)
                        {
                        case ELootColorKind::Money:
                            extent = SDK::FVector{ 35.f, 35.f, 40.f };
                            break;
                        case ELootColorKind::Chem:
                            extent = SDK::FVector{ 22.f, 22.f, 28.f };
                            break;
                        default:
                            extent = SDK::FVector{ 14.f, 14.f, 18.f };
                            break;
                        }
                    }
                }

                // Hard clamp world extent — keys/tools stay small, bags can be larger.
                float flMaxHalf = 28.f;
                float flMinHalf = 10.f;
                switch (kind)
                {
                case ELootColorKind::Money:
                    flMaxHalf = 70.f;
                    flMinHalf = 18.f;
                    break;
                case ELootColorKind::Chem:
                    flMaxHalf = 36.f;
                    flMinHalf = 12.f;
                    break;
                default:
                    flMaxHalf = 24.f;
                    flMinHalf = 10.f;
                    break;
                }

                auto ClampAxis = [&](float v) -> float
                {
                    if (v < flMinHalf)
                        return flMinHalf;
                    if (v > flMaxHalf)
                        return flMaxHalf;
                    return v;
                };
                extent.X = ClampAxis(extent.X);
                extent.Y = ClampAxis(extent.Y);
                extent.Z = ClampAxis(extent.Z);

                auto optBox = CalculateScreenBoxFromBBox(pPlayerController, origin, extent);
                if (!optBox.has_value())
                    return std::nullopt;

                ImVec4 box = optBox.value();

                // Distance-based screen size so far-away loot isn't a speck / close loot isn't a billboard.
                const float flDist = (origin - pPlayerController->PlayerCameraManager->GetCameraLocation()).Magnitude();
                const float flDistScale = (std::clamp)(1800.f / (std::max)(flDist, 80.f), 0.55f, 1.35f);

                float flMaxW = 56.f * flDistScale;
                float flMaxH = 64.f * flDistScale;
                float flMinW = 16.f * flDistScale;
                float flMinH = 18.f * flDistScale;
                if (kind == ELootColorKind::Money)
                {
                    flMaxW = 90.f * flDistScale;
                    flMaxH = 100.f * flDistScale;
                    flMinW = 24.f * flDistScale;
                    flMinH = 28.f * flDistScale;
                }
                else if (kind == ELootColorKind::Key)
                {
                    flMaxW = 40.f * flDistScale;
                    flMaxH = 44.f * flDistScale;
                    flMinW = 14.f * flDistScale;
                    flMinH = 16.f * flDistScale;
                }

                SDK::FVector2D screenCenter{};
                if (!pPlayerController->ProjectWorldLocationToScreen(origin, &screenCenter, false))
                {
                    screenCenter.X = (box.x + box.z) * 0.5f;
                    screenCenter.Y = (box.y + box.w) * 0.5f;
                }

                float w = box.z - box.x;
                float h = box.w - box.y;
                if (w > flMaxW || h > flMaxH)
                {
                    const float sx = (w > flMaxW) ? (flMaxW / w) : 1.f;
                    const float sy = (h > flMaxH) ? (flMaxH / h) : 1.f;
                    const float s = (std::min)(sx, sy);
                    w *= s;
                    h *= s;
                }
                if (w < flMinW)
                    w = flMinW;
                if (h < flMinH)
                    h = flMinH;

                return ImVec4{
                    screenCenter.X - w * 0.5f,
                    screenCenter.Y - h * 0.5f,
                    screenCenter.X + w * 0.5f,
                    screenCenter.Y + h * 0.5f };
            };

            UC::TArray<SDK::ULevel*> vecLevels = pGWorld->Levels;
            for (SDK::ULevel* pLevel : vecLevels)
            {
                if (!pLevel || !pLevel->Actors)
                    continue;

                for (SDK::AActor* pActor : pLevel->Actors)
                {
                    if (!FastIsLootActor(pActor))
                        continue;

                    const char* szLabel = LabelForActor(pActor);
                    if (!szLabel)
                        continue;
                    if (!StillLootable(pActor))
                        continue;

                    if (pActor->Class->Name == FNames::BP_QRPhone_C)
                    {
                        // Phone uses SBZOutline; don't require interact flag (same as F4).
                        auto* pPhone = reinterpret_cast<SDK::ABP_QRPhone_C*>(pActor);
                        if (pPhone->SBZInteractable && pPhone->SBZInteractable->bInteractionEnabled == 0
                            && !pPhone->SBZOutline)
                            continue;
                    }
                    // USB / meth / planks: still skip only when clearly dead (no interact + no outline).
                    if (pActor->Class->Name == FNames::BP_FOR_USBDrive_C)
                    {
                        auto* pUSB = reinterpret_cast<SDK::ABP_FOR_USBDrive_C*>(pActor);
                        if (pUSB->SBZInteractableObject && !pUSB->SBZInteractableObject->bInteractionEnabled)
                        {
                            // Still glow if an outline comp exists.
                            if (!pActor->GetComponentByClass(SDK::USBZOutlineComponent::StaticClass()))
                                continue;
                        }
                    }
                    if (pActor->Class->Name == FNames::BP_Meth_CausticSoda_C
                        || pActor->Class->Name == FNames::BP_Meth_MuriaticAcid_C
                        || pActor->Class->Name == FNames::BP_Meth_HydrogenChloride_C)
                    {
                        auto* pMeth = reinterpret_cast<SDK::ABP_MethIngredientBase_C*>(pActor);
                        if (pMeth->SBZInteractable && !pMeth->SBZInteractable->bInteractionEnabled)
                        {
                            if (!pActor->GetComponentByClass(SDK::USBZOutlineComponent::StaticClass()))
                                continue;
                        }
                    }
                    if (pActor->Class->Name == FNames::BP_Plankspile_C)
                    {
                        auto* pPlanks = reinterpret_cast<SDK::ABP_Plankspile_C*>(pActor);
                        if (pPlanks->Interactable && !pPlanks->Interactable->bInteractionEnabled)
                        {
                            if (!pActor->GetComponentByClass(SDK::USBZOutlineComponent::StaticClass()))
                                continue;
                        }
                    }

                    const ELootColorKind kind = ColorKindFor(pActor, szLabel);
                    const ImU32 colLoot = ColorForKind(kind);
                    const int8_t glowIdx = ColorIndexFromImU32(colLoot);

                    // Strong Glow = PD3 mesh outline only. ImGui brackets only if Loot Outline is on.
                    if (bDoGlow)
                        TryApplyGameOutline(pActor, true, glowIdx);
                    else if (lootCfg.bOutline)
                        TryApplyGameOutline(pActor, false, glowIdx);

                    if (!lootCfg.bOutline && !lootCfg.bLabels)
                        continue;

                    SDK::FVector2D vec2ScreenLocation{};
                    const bool bHasOrigin = pPlayerController->ProjectWorldLocationToScreen(
                        pActor->K2_GetActorLocation(), &vec2ScreenLocation, false);

                    if (lootCfg.bOutline)
                    {
                        ImVec4 box{};
                        bool bHaveBox = false;
                        if (auto optBox = ScreenBoxForLoot(pActor, kind); optBox.has_value())
                        {
                            box = optBox.value();
                            bHaveBox = true;
                        }
                        else if (bHasOrigin)
                        {
                            const float s = (kind == ELootColorKind::Key) ? 10.f
                                : (kind == ELootColorKind::Chem) ? 14.f : 18.f;
                            box = ImVec4{
                                vec2ScreenLocation.X - s, vec2ScreenLocation.Y - s,
                                vec2ScreenLocation.X + s, vec2ScreenLocation.Y + s };
                            bHaveBox = true;
                        }

                        if (bHaveBox)
                            DrawHighlightCorners(pDrawList, box, colLoot);
                    }

                    if (!lootCfg.bLabels || !bHasOrigin)
                        continue;

                    pDrawList->AddText(
                        ImVec2(vec2ScreenLocation.X + 1.f, vec2ScreenLocation.Y + 1.f),
                        IM_COL32(0, 0, 0, 200),
                        szLabel);
                    pDrawList->AddText(
                        ImVec2(vec2ScreenLocation.X, vec2ScreenLocation.Y),
                        colLoot,
                        szLabel);
                }
            }

            }
        }

        // Pager HUD / bag drop-off markers — independent of Enable ESP.
        {
            const auto& cfg = GetConfig();
            const auto& cols = cfg.m_colors;

            if (cfg.bPagerHud && pGWorld->GameState && pGWorld->GameState->IsA(SDK::APD3HeistGameState::StaticClass()))
            {
                auto* pGS = reinterpret_cast<SDK::APD3HeistGameState*>(pGWorld->GameState);
                const int answered = static_cast<int>(pGS->AnswerPagerCount);
                int maxAns = 0;
                for (int i = 0; i < 4; ++i)
                    maxAns = (std::max)(maxAns, static_cast<int>(pGS->PagerHeistDataArray[i].MaxAnswerCount));
                if (maxAns <= 0)
                    maxAns = 4;

                char szPager[64]{};
                sprintf_s(szPager, "Pagers %d / %d", answered, maxAns);
                const ImVec2 screen = CheatConfig::Get().m_misc.vec2ScreenSize;
                pDrawList->AddText(ImVec2(screen.x - 160.f, 48.f), IM_COL32(0, 0, 0, 200), szPager);
                pDrawList->AddText(ImVec2(screen.x - 161.f, 47.f), cols.m_colSuspicious, szPager);
            }

            if (cfg.bBagZones)
            {
                auto* cls = SDK::ASBZBagTriggerVolume::StaticClass();
                if (cls)
                {
                    SDK::TArray<SDK::AActor*> volumes{};
                    SDK::UGameplayStatics::GetAllActorsOfClass(pGWorld, cls, &volumes);
                    for (int i = 0; i < volumes.Num(); ++i)
                    {
                        if (!volumes.IsValidIndex(i))
                            break;
                        auto* pVol = reinterpret_cast<SDK::ASBZBagTriggerVolume*>(volumes[i]);
                        if (!pVol)
                            continue;

                        const std::string name = pVol->GetName();
                        std::string low = name;
                        for (char& c : low)
                            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

                        const bool bNameHit =
                            low.contains("escape") || low.contains("van") || low.contains("secure")
                            || low.contains("loot") || low.contains("bag") || low.contains("elevator")
                            || low.contains("deposit") || low.contains("drop") || low.contains("zipline")
                            || low.contains("lower");
                        if (!bNameHit && pVol->Target <= 0)
                            continue;
                        if (pVol->TriggerMode == SDK::ESBZBagTriggerMode::Nothing)
                            continue;

                        SDK::FVector2D scr{};
                        if (!pPlayerController->ProjectWorldLocationToScreen(pVol->K2_GetActorLocation(), &scr, false))
                            continue;

                        const int count = pVol->GetCount();
                        const int target = pVol->Target;
                        const char* szMode = "Secure";
                        if (pVol->TriggerMode == SDK::ESBZBagTriggerMode::DestroyOnly)
                            szMode = "Destroy";
                        else if (pVol->TriggerMode == SDK::ESBZBagTriggerMode::Teleport)
                            szMode = "Teleport";

                        char szLabel[96]{};
                        if (target > 0)
                            sprintf_s(szLabel, "Bag drop [%s] %d/%d", szMode, count, target);
                        else
                            sprintf_s(szLabel, "Bag drop [%s] %d", szMode, count);

                        pDrawList->AddText(ImVec2(scr.X + 1.f, scr.Y + 1.f), IM_COL32(0, 0, 0, 200), szLabel);
                        pDrawList->AddText(ImVec2(scr.X, scr.Y), cols.m_colBagZone, szLabel);
                        DrawHighlightCorners(
                            pDrawList,
                            ImVec4{ scr.X - 22.f, scr.Y - 22.f, scr.X + 22.f, scr.Y + 22.f },
                            cols.m_colBagZone);
                    }
                }
            }
        }
        
        if (!GetConfig().bESP)
            return;

        for (ActorInfo& infoActor : vecActors){
            auto pActor = infoActor.m_pActor;
            SDK::FVector2D vec2ScreenLocation;
            if (!pActor)
                continue;
            if(!pPlayerController->ProjectWorldLocationToScreen(pActor->K2_GetActorLocation(), &vec2ScreenLocation, false))
                continue;

            switch(infoActor.m_eType){
            case EActorType::Guard:
                DrawEnemyESP(pDrawList, pPlayerController, reinterpret_cast<SDK::ACH_BaseCop_C*>(pActor), infoActor.m_eType, GetConfig().m_stNormalEnemies);
                break;

            case EActorType::Shield:
            case EActorType::Dozer:
            case EActorType::Cloaker:
            case EActorType::Sniper:
            case EActorType::Grenadier:
            case EActorType::Taser:
            case EActorType::Techie:
                DrawEnemyESP(pDrawList, pPlayerController, reinterpret_cast<SDK::ACH_BaseCop_C*>(pActor), infoActor.m_eType, GetConfig().m_stSpecialEnemies);
                break;

            case EActorType::ObjectiveItem:
            case EActorType::InteractableItem:
                DrawInteractableESP(pDrawList, pPlayerController, reinterpret_cast<SDK::ASBZInteractionActor*>(pActor), infoActor.m_eType);
                break;

            case EActorType::Civilian:
                DrawCivilianESP(pDrawList, pPlayerController, reinterpret_cast<SDK::ACH_BaseCivilian_C*>(pActor), GetConfig().m_stCivilians);
                break;

            case EActorType::LootBag:
                break;

            default:
                break;
            }           
        }

        auto pLocalPlayer = reinterpret_cast<SDK::ASBZPlayerCharacter*>(pPlayerController->AcknowledgedPawn);
        if (!pLocalPlayer)
            return;

        auto pAbilitySystem = pLocalPlayer->PlayerAbilitySystem;
        if (!pAbilitySystem)
            return;        

        auto& aCameras = pWorldRuntime->AllSecurityCameras->Objects;
        for (int i = 0; i < aCameras.Num(); ++i) {
            if (!aCameras.IsValidIndex(i))
                break;

            auto pCamera = reinterpret_cast<SDK::ASBZSecurityCamera*>(aCameras[i]);
            if (!pCamera)
                continue;

            if (pCamera->SoundState == SDK::ESBZCameraSoundState::Suspiscious){
                // Use runtime on this camera
            }

            if (pCamera->OutlineAsset)
                pCamera->OutlineAsset->ColorIndex = 3;
            if (pCamera->OutlineComponent)
                pCamera->OutlineComponent->Multicast_SetActiveReplicated(pCamera->OutlineAsset);
        }
    }

    void RenderDebugESP(SDK::ULevel* pPersistentLevel, SDK::APlayerController* pPlayerController) {
        if (!GetConfig().bDebugESP)
            return;

        SDK::APlayerCameraManager* pCameraManager = pPlayerController->PlayerCameraManager;
        if (!pCameraManager)
            return;

        ImDrawList* pDrawList = ImGui::GetBackgroundDrawList();

        SDK::UWorld* pGWorld = SDK::UWorld::GetWorld();
        if (!pGWorld)
            return;

        UC::TArray<SDK::ULevel*> vecLevels = pGWorld->Levels;
        for (SDK::ULevel* pLevel : vecLevels) {
            if (!pLevel || !pLevel->Actors)
                continue;

            for (SDK::AActor* pActor : pLevel->Actors) {
                if (!pActor)
                    continue;

                float distance = pCameraManager->GetDistanceTo(pActor);
                if (distance > 1000.f)
                    continue;

                SDK::FVector ActorLocation = pActor->K2_GetActorLocation();
                SDK::FVector2D ScreenLocation;

                if (!pPlayerController->ProjectWorldLocationToScreen(ActorLocation, &ScreenLocation, false))
                    continue;

                char szName[64];
                for (SDK::UStruct* pStruct = static_cast<SDK::UStruct*>(pActor->Class); pStruct != nullptr; pStruct = static_cast<SDK::UStruct*>(pStruct->SuperStruct)) {

                    szName[pStruct->Name.GetRawString().copy(szName, 63)] = '\0';

                    ImVec2 vecTextSize = ImGui::CalcTextSize(szName);
                    pDrawList->AddText({ScreenLocation.X - vecTextSize.x / 2, ScreenLocation.Y - 8.f}, IM_COL32(255, 0, 0, 255), szName);
                    ScreenLocation.Y += vecTextSize.y + 2.f;
                }
            }
        }
    }
}

void LootESP::TickCheapGlow(SDK::UWorld* pGWorld)
{
    if (!pGWorld || !GetConfig().bLootESP)
        return;

    static std::vector<SDK::AActor*> s_loot;
    static auto s_timeRescan = std::chrono::steady_clock::time_point{};
    const auto now = std::chrono::steady_clock::now();

    if (now >= s_timeRescan)
    {
        s_timeRescan = now + std::chrono::seconds(2);
        s_loot.clear();
        ScanOutlineAssetsOnce();
        std::unordered_set<uintptr_t> seen;

        auto Consider = [&](SDK::AActor* pActor)
        {
            if (!pActor || pActor->IsActorBeingDestroyed())
                return;
            if (LootClassify::Classify(pActor).Type == LootClassify::ItemType::None)
                return;
            if (!seen.insert(reinterpret_cast<uintptr_t>(pActor)).second)
                return;
            s_loot.push_back(pActor);
        };

        SDK::TArray<SDK::AActor*> found{};
        SDK::UGameplayStatics::GetAllActorsOfClass(pGWorld, SDK::ASBZInstantLoot::StaticClass(), &found);
        for (int i = 0; i < found.Num(); ++i)
            Consider(found[i]);

        found = {};
        SDK::UGameplayStatics::GetAllActorsOfClass(pGWorld, SDK::ASBZSingleBagGenerator::StaticClass(), &found);
        for (int i = 0; i < found.Num(); ++i)
            Consider(found[i]);

        found = {};
        SDK::UGameplayStatics::GetAllActorsOfClass(pGWorld, SDK::ASBZMultiBagGenerator::StaticClass(), &found);
        for (int i = 0; i < found.Num(); ++i)
            Consider(found[i]);

        for (SDK::ULevel* pLevel : pGWorld->Levels)
        {
            if (!pLevel || !pLevel->Actors)
                continue;
            for (SDK::AActor* pActor : pLevel->Actors)
                Consider(pActor);
        }
    }

    for (SDK::AActor* pActor : s_loot)
    {
        if (!pActor || !pActor->Class || pActor->IsActorBeingDestroyed())
            continue;
        const auto loot = LootClassify::Classify(pActor);
        if (loot.Type == LootClassify::ItemType::None)
            continue;

        int8_t idx = kOutlineYellow;
        if (loot.Type == LootClassify::ItemType::Cash)
            idx = kOutlineWhite;
        else if (loot.Type == LootClassify::ItemType::DepositBox)
            idx = kOutlineRed;
        else if (loot.Type == LootClassify::ItemType::Keycard)
        {
            const char* lab = loot.Label ? loot.Label : "";
            if (std::strstr(lab, "Caustic") || std::strstr(lab, "Muriatic")
                || std::strstr(lab, "Hydrogen") || std::strstr(lab, "Nitrogen"))
                idx = kOutlineYellow;
            else
                idx = kOutlinePink;
        }
        ApplyActorOutline(pActor, idx);
    }
}
