#include "pch.h"
#include "Visuals/Visuals.hpp"
#include "Helpers.hpp"
#include "Visuals/VisualsHelpers.hpp"
#include "Visuals/LootClassify.hpp"
#include <unordered_set>

namespace
{
	bool TryReadPawnStats(SDK::ASBZCharacter* character, float& health, float& healthMax, float& armor, float& armorMax)
	{
		if (!character || !IsValidObjectPtr(character))
			return false;

		auto* abilitySystem = character->AbilitySystem;
		if (!abilitySystem || !IsValidObjectPtr(abilitySystem))
			return false;

		UC::TArray<SDK::UAttributeSet*>* spawnedAttrs = (UC::TArray<SDK::UAttributeSet*>*)((uintptr_t)abilitySystem + 0x0150);
		if (!spawnedAttrs)
			return false;

		for (int i = 0; i < spawnedAttrs->Num(); ++i)
		{
			auto* attrSet = (*spawnedAttrs)[i];
			if (attrSet && attrSet->IsA(SDK::USBZPawnAttributeSet::StaticClass()))
			{
				auto* pawnAttrs = static_cast<SDK::USBZPawnAttributeSet*>(attrSet);
				health = pawnAttrs->Health.CurrentValue;
				healthMax = pawnAttrs->HealthMax.CurrentValue;
				armor = pawnAttrs->Armor.CurrentValue;
				armorMax = pawnAttrs->ArmorMax.CurrentValue;
				return true;
			}
		}

		return false;
	}

	void PushItemUnique(std::vector<Types::ItemData>& out, std::unordered_set<uintptr_t>& seen, SDK::AActor* pActor, const LootClassify::Result& loot)
	{
		if (!pActor || loot.Type == Types::ItemType::None)
			return;
		const uintptr_t key = reinterpret_cast<uintptr_t>(pActor);
		if (!seen.insert(key).second)
			return;

		out.push_back({
			SDK::FVector2D{},
			pActor->K2_GetActorLocation(),
			std::string(loot.Label),
			loot.Type,
			pActor
		});
	}

	bool PassesItemFilter(Types::ItemType type, bool showCash, bool showDeposit, bool showKey)
	{
		return (type == Types::ItemType::Cash && showCash)
			|| (type == Types::ItemType::DepositBox && showDeposit)
			|| (type == Types::ItemType::Keycard && showKey);
	}
}

Types::EnemyType Visuals::GetEnemyType(SDK::AActor* actor)
{
	return Helpers::ResolveEnemyType(m_ClassCache, actor);
}

Types::ItemType Visuals::GetItemType(SDK::AActor* actor)
{
	return LootClassify::Classify(actor).Type;
}

void Visuals::CollectFrameData(SDK::UWorld* pGWorld, SDK::APlayerController* pPlayerController, SDK::APlayerCameraManager* pCameraManager, SDK::AActor* pLocalPlayer, const FrameSettings& settings)
{
	m_vESPData.clear();
	m_vESPData.reserve(256);
	m_vItemData.clear();

	const bool needsEnemyBoxData = settings.DrawBox || settings.DrawName || settings.DrawDistance || settings.DrawHealthBar || settings.DrawArmorBar;
	const bool needsEnemyStats = settings.DrawHealthBar || settings.DrawArmorBar;
	const bool needsEnemies = settings.ShowCops || settings.ShowCivilians;
	const bool needsEnemyDraw = settings.DrawBox || settings.DrawName || settings.DrawDistance || settings.DrawHealthBar
		|| settings.DrawArmorBar || settings.DrawSkeleton || settings.DrawHighlight || settings.DrawOutline;
	const bool needsItems = settings.DrawItems || (settings.DrawOutline && (settings.ShowCash || settings.ShowDepositBox || settings.ShowKeycards));

	if (!pGWorld || !pPlayerController || !pCameraManager)
		return;

	SDK::FVector vecCameraLocation = pCameraManager->GetCameraLocation();
	SDK::USBZWorldRuntime* pWorldRuntime = SDK::USBZWorldRuntime::Get(pGWorld);
	if (!pWorldRuntime || !pWorldRuntime->AllPawns)
		return;

	UC::TArray<SDK::UObject*>& actors = pWorldRuntime->AllPawns->Objects;

	if (needsEnemies && needsEnemyDraw)
	{
		for (int i = 0; i < actors.Num(); ++i)
		{
			if (!actors.IsValidIndex(i))
				break;

			auto* pActor = reinterpret_cast<SDK::AActor*>(actors[i]);
			if (!pActor || pActor == pLocalPlayer || pActor->IsActorBeingDestroyed())
				continue;
			if (!pActor->IsA(SDK::ASBZCharacter::StaticClass()))
				continue;

			auto* pCharacter = reinterpret_cast<SDK::ASBZCharacter*>(pActor);
			if (!pCharacter->bIsAlive || !pCharacter->Mesh)
				continue;

			// Skip local heisters / crew if they resolve as non-enemy
			if (pCharacter->IsA(SDK::ASBZPlayerCharacter::StaticClass()))
				continue;

			Types::EnemyType type = GetEnemyType(pActor);
			const Types::EnemyInfo& info = Types::g_EnemyInfo[static_cast<size_t>(type)];
			bool bIsCop = info.Category == Types::EnemyCategory::Cop;
			bool bIsCivilian = info.Category == Types::EnemyCategory::Civilian;

			// Fallback: AI characters that didn't match keywords still count as cops
			if (!bIsCop && !bIsCivilian)
			{
				if (pActor->IsA(SDK::ASBZAICharacter::StaticClass()))
				{
					bIsCop = true;
					type = Types::EnemyType::ArmedCop;
				}
				else
				{
					continue;
				}
			}

			if (bIsCop && !settings.ShowCops)
				continue;
			if (bIsCivilian && !settings.ShowCivilians)
				continue;

			ImVec4 screenBox{};
			if (needsEnemyBoxData)
			{
				auto optScreenBox = VisualsHelpers::CalculateScreenBoxForCharacter(pCharacter->Mesh, pPlayerController, pActor);
				if (!optScreenBox.has_value())
					continue;

				screenBox = optScreenBox.value();
			}

			float flHealth = 0.0f;
			float flHealthMax = 0.0f;
			float flArmor = 0.0f;
			float flArmorMax = 0.0f;
			if (needsEnemyStats)
				TryReadPawnStats(pCharacter, flHealth, flHealthMax, flArmor, flArmorMax);

			float flDistance = 0.0f;
			if (settings.DrawDistance)
				flDistance = (pActor->K2_GetActorLocation() - vecCameraLocation).Magnitude() / 100.0f;

			const Types::EnemyInfo& pushInfo = Types::g_EnemyInfo[static_cast<size_t>(type)];
			m_vESPData.push_back({
				screenBox,
				std::string(pushInfo.Name),
				pCharacter->Mesh,
				pCharacter,
				flHealth,
				flHealthMax,
				flArmor,
				flArmorMax,
				flDistance,
				bIsCop,
				bIsCivilian
			});

			if (settings.DrawSkeleton)
			{
				auto [cacheIt, inserted] = m_BoneCache.try_emplace(pCharacter->Mesh);
				if (!cacheIt->second.Initialized)
					VisualsHelpers::BuildBoneCache(pCharacter->Mesh, cacheIt->second);
			}
		}
	}

	if (!needsItems)
		return;

	std::unordered_set<uintptr_t> seenItems;
	seenItems.reserve(512);
	m_vItemData.reserve(256);

	auto Consider = [&](SDK::AActor* pActor)
	{
		if (!pActor || pActor->IsActorBeingDestroyed())
			return;
		auto loot = LootClassify::Classify(pActor);
		if (loot.Type == Types::ItemType::None)
			return;
		if (!PassesItemFilter(loot.Type, settings.ShowCash, settings.ShowDepositBox, settings.ShowKeycards))
			return;
		PushItemUnique(m_vItemData, seenItems, pActor, loot);
	};

	// Level actor walk (v1 style)
	for (SDK::ULevel* pLevel : pGWorld->Levels)
	{
		if (!pLevel || !pLevel->Actors)
			continue;

		for (SDK::AActor* pActor : pLevel->Actors)
			Consider(pActor);
	}

	// Hard pass like v1 — catch streamed money piles Levels can miss
	{
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

		if (auto* pDyedClass = SDK::UObject::FindClassFast("BP_MoneyBagDyed_C"))
		{
			found = {};
			SDK::UGameplayStatics::GetAllActorsOfClass(pGWorld, pDyedClass, &found);
			for (int i = 0; i < found.Num(); ++i)
				Consider(found[i]);
		}
	}
}
