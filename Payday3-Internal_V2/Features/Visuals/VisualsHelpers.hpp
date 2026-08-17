#pragma once
#include "pch.h"
#include "Types.hpp"

namespace VisualsHelpers
{
	std::optional<ImVec4> CalculateScreenBoxFromTopBottom(SDK::APlayerController* pPlayerController, SDK::AActor* pActor,
		SDK::FVector vecTop, SDK::FVector vecBottom);

	std::optional<ImVec4> CalculateScreenBoxForCharacter(SDK::USkeletalMeshComponent* pMeshComponent,
		SDK::APlayerController* pPlayerController, SDK::AActor* pActor);

	void BuildBoneCache(SDK::USkeletalMeshComponent* mesh, Types::BoneCache& cache);
	void DrawBone(ImDrawList* pDrawList, SDK::APlayerController* pPlayerController, SDK::USkeletalMeshComponent* mesh,
		int parent, int child, ImU32 color);

	// PD3 native outline ColorIndex palette (through-wall only):
	// combo 0=Red(3) 1=Yellow(10) 2=White(9) 3=Pink(11)
	int8_t ColorIndexFromPaletteCombo(int comboIndex);
	void ApplyActorOutline(SDK::AActor* pActor, int8_t colorIndex);

}