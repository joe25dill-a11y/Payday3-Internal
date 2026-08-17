#include "pch.h"
#include "VisualsHelpers.hpp"
#include <algorithm>
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

#undef min
#undef max

namespace VisualsHelpers
{
	namespace
	{
		// PD3 through-wall ColorIndex palette (same as freecam F4):
		// 3=red | 9=white | 10=yellow | 11=pink — ColorIndex 2 (orange) is LOS-only.
		constexpr int8_t kOutlineRed = 3;
		constexpr int8_t kOutlineWhite = 9;
		constexpr int8_t kOutlineYellow = 10;
		constexpr int8_t kOutlinePink = 11;
		constexpr auto kOutlinePulseGap = std::chrono::milliseconds(450);

		SDK::USBZOutlineAsset* s_outlineByColor[32]{};
		SDK::USBZOutlineAsset* s_pTemplateAsset = nullptr;
		bool s_bOutlineAssetScanDone = false;
		std::unordered_map<uintptr_t, std::chrono::steady_clock::time_point> s_outlinePulseAt;

		std::string ToLowerAscii(std::string s)
		{
			for (char& c : s)
			{
				if (c >= 'A' && c <= 'Z')
					c = static_cast<char>(c - 'A' + 'a');
			}
			return s;
		}

		int OutlineTemplateScore(SDK::USBZOutlineAsset* pAsset)
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

		void ScanOutlineAssetsOnce()
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
				auto* pAsset = reinterpret_cast<SDK::USBZOutlineAsset*>(pObj);
				candidates.push_back(pAsset);
			}

			std::sort(candidates.begin(), candidates.end(), [](SDK::USBZOutlineAsset* a, SDK::USBZOutlineAsset* b)
			{
				return OutlineTemplateScore(a) > OutlineTemplateScore(b);
			});

			if (!candidates.empty())
				s_pTemplateAsset = candidates.front();

			// Claim dedicated assets for each through-wall color so Red/Yellow/White/Pink actually differ.
			const int8_t want[] = { kOutlineRed, kOutlineYellow, kOutlineWhite, kOutlinePink };
			size_t next = 0;
			for (int8_t idx : want)
			{
				SDK::USBZOutlineAsset* pick = nullptr;
				// Prefer one that already has this ColorIndex
				for (auto* a : candidates)
				{
					if (a && a->ColorIndex == idx)
					{
						pick = a;
						break;
					}
				}
				// Else take next best unused candidate and assign ColorIndex once
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

		SDK::USBZOutlineAsset* ResolveOutlineAssetForColor(int8_t colorIndex)
		{
			ScanOutlineAssetsOnce();
			if (colorIndex < 0 || colorIndex >= 32)
				colorIndex = kOutlineYellow;

			if (s_outlineByColor[colorIndex])
			{
				// Keep ColorIndex pinned (v1 mutates before multicast — required for color swaps)
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

		void AttachActorMeshesToOutline(SDK::USBZOutlineComponent* pOutline, SDK::AActor* pActor);
		void SoftEnableOutlineComp(SDK::USBZOutlineComponent* pOutline, SDK::USBZOutlineAsset* pAsset, bool bPulseMulticast, bool bAttachMeshes, SDK::AActor* pActor);
		void ApplyFarRenderForOutline(SDK::AActor* pActor);

		void AttachActorMeshesToOutline(SDK::USBZOutlineComponent* pOutline, SDK::AActor* pActor)
		{
			if (!pOutline || !pActor)
				return;
			auto comps = pActor->K2_GetComponentsByClass(SDK::UMeshComponent::StaticClass());
			for (int i = 0; i < comps.Num(); ++i)
			{
				if (!comps.IsValidIndex(i))
					break;
				auto* pMesh = reinterpret_cast<SDK::UMeshComponent*>(comps[i]);
				if (!pMesh || !IsValidObjectPtr(pMesh))
					continue;
				pOutline->AddMesh(pMesh, true);
			}
		}

		void SoftEnableOutlineComp(SDK::USBZOutlineComponent* pOutline, SDK::USBZOutlineAsset* pAsset, bool bPulseMulticast, bool bAttachMeshes, SDK::AActor* pActor)
		{
			if (!pOutline || !pAsset)
				return;
			if (!IsValidObjectPtr(pOutline) || !IsValidObjectPtr(pAsset))
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

		void ApplyFarRenderForOutline(SDK::AActor* pActor)
		{
			if (!pActor)
				return;
			auto Touch = [](SDK::UPrimitiveComponent* pPrim)
			{
				if (!pPrim || !IsValidObjectPtr(pPrim))
					return;
				pPrim->bNeverDistanceCull = 1;
				pPrim->bAllowCullDistanceVolume = 0;
				pPrim->LDMaxDrawDistance = 0.f;
				pPrim->SetCullDistance(0.f);
			};

			auto comps = pActor->K2_GetComponentsByClass(SDK::UPrimitiveComponent::StaticClass());
			for (int i = 0; i < comps.Num(); ++i)
			{
				if (!comps.IsValidIndex(i))
					break;
				Touch(reinterpret_cast<SDK::UPrimitiveComponent*>(comps[i]));
			}
		}
	}

	int8_t ColorIndexFromPaletteCombo(int comboIndex)
	{
		switch (comboIndex)
		{
		case 0: return kOutlineRed;
		case 1: return kOutlineYellow;
		case 2: return kOutlineWhite;
		case 3: return kOutlinePink;
		default: return kOutlineYellow;
		}
	}

	void ApplyActorOutline(SDK::AActor* pActor, int8_t colorIndex)
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

		// Cops/civs: v1 SetMarked + soft outline, NEVER AddMesh (that was crashing)
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

	std::optional<ImVec4> CalculateScreenBoxFromTopBottom(SDK::APlayerController* pPlayerController, SDK::AActor* pActor,
		SDK::FVector vecTop, SDK::FVector vecBottom)
	{
		SDK::FVector2D vec2Top, vec2Bottom;
		if (!pPlayerController->ProjectWorldLocationToScreen(vecBottom, &vec2Bottom, false) ||
			!pPlayerController->ProjectWorldLocationToScreen(vecTop, &vec2Top, false))
			return {};

		float flHeight = std::abs(vec2Top.Y - vec2Bottom.Y);
		float flYawRad = pActor->K2_GetActorRotation().Yaw * (3.14159265358979323846f / 180.0f);
		float flWidthRatio = 0.25f + (std::abs(SDK::FVector(std::cos(flYawRad), std::sin(flYawRad), 0.f).GetNormalized().Dot(
			(pActor->K2_GetActorLocation() - pPlayerController->PlayerCameraManager->GetCameraLocation()).GetNormalized())) * 0.15f);

		float flWidth = flHeight * flWidthRatio;
		float flCenter = (vec2Bottom.X + vec2Top.X) / 2.0f;
		return ImVec4{ flCenter - (flWidth / 2.0f), std::min(vec2Bottom.Y, vec2Top.Y),
					   flCenter + (flWidth / 2.0f), std::max(vec2Bottom.Y, vec2Top.Y) };
	}

	std::optional<ImVec4> CalculateScreenBoxForCharacter(SDK::USkeletalMeshComponent* pMeshComponent,
		SDK::APlayerController* pPlayerController, SDK::AActor* pActor)
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

	void BuildBoneCache(SDK::USkeletalMeshComponent* mesh, Types::BoneCache& cache)
	{
		if (!mesh)
			return;

		const int numBones = mesh->GetNumBones();

		for (int i = 0; i < numBones; ++i)
		{
			const std::string bone = mesh->GetBoneName(i).ToString();

			if (bone == "Hips")
				cache.Hips = i;

			else if (bone == "Spine")
				cache.Spine = i;

			else if (bone == "Spine1")
				cache.Spine1 = i;

			else if (bone == "Spine2")
				cache.Spine2 = i;

			else if (bone == "Spine3")
				cache.Spine3 = i;

			else if (bone == "Neck")
				cache.Neck = i;

			else if (bone == "Head")
				cache.Head = i;

			else if (bone == "LeftShoulder")
				cache.LeftShoulder = i;

			else if (bone == "LeftArm")
				cache.LeftUpperArm = i;

			else if (bone == "LeftForeArm")
				cache.LeftForeArm = i;

			else if (bone == "LeftHand")
				cache.LeftHand = i;

			else if (bone == "RightShoulder")
				cache.RightShoulder = i;

			else if (bone == "RightArm")
				cache.RightUpperArm = i;

			else if (bone == "RightForeArm")
				cache.RightForeArm = i;

			else if (bone == "RightHand")
				cache.RightHand = i;

			else if (bone == "LeftUpLeg")
				cache.LeftUpperLeg = i;

			else if (bone == "LeftLeg")
				cache.LeftLowerLeg = i;

			else if (bone == "LeftFoot")
				cache.LeftFoot = i;

			else if (bone == "LeftToeBase")
				cache.LeftToe = i;

			else if (bone == "RightUpLeg")
				cache.RightUpperLeg = i;

			else if (bone == "RightLeg")
				cache.RightLowerLeg = i;

			else if (bone == "RightFoot")
				cache.RightFoot = i;

			else if (bone == "RightToeBase")
				cache.RightToe = i;
		}

		cache.Initialized = true;
	}

	void DrawBone(ImDrawList* pDrawList, SDK::APlayerController* pPlayerController, SDK::USkeletalMeshComponent* mesh,
		int parent, int child, ImU32 color)
	{
		if (parent < 0 || child < 0)
			return;

		auto parentWorld = mesh->GetSocketLocation(mesh->GetBoneName(parent));
		auto childWorld = mesh->GetSocketLocation(mesh->GetBoneName(child));
		SDK::FVector2D p0, p1;

		if (pPlayerController->ProjectWorldLocationToScreen(parentWorld, &p0, false) && pPlayerController->ProjectWorldLocationToScreen(childWorld, &p1, false))
		{
			pDrawList->AddLine(ImVec2(p0.X - 1, p0.Y - 1), ImVec2(p1.X - 1, p1.Y - 1), IM_COL32(0, 0, 0, 255), 1.1f);
			pDrawList->AddLine(ImVec2(p0.X, p0.Y), ImVec2(p1.X, p1.Y), color, 1.0f);
		}
	}

}