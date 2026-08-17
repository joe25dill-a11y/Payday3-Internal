#pragma once
#include "pch.h"

namespace Types
{
	// This is only used for the multiselect combo.
	enum class EnemyCategory
	{
		None,
		Cop,
		Civilian
	};

	struct EnemyInfo
	{
		const char* Name;
		EnemyCategory Category;
	};

	// The order between this and g_EnemyInfo must match.
	enum class EnemyType
	{
		None,
		Security,
		ArmedCop,
		Shield,
		Dozer,
		Cloaker,
		Sniper,
		Taser,
		Tower,
		Shotgun,
		AR,
		SMG,
		Grenadier,
		Civilian,
		SentryGun
	};

	struct EnemyLookup
	{
		std::string_view Keyword;
		EnemyType Type;
	};

	inline constexpr EnemyLookup g_EnemyLookup[] =
	{
		{ "security",   EnemyType::Security },
		{ "armedcop",   EnemyType::ArmedCop },
		{ "shield",     EnemyType::Shield },
		{ "dozer",      EnemyType::Dozer },
		{ "cloaker",    EnemyType::Cloaker },
		{ "sniper",     EnemyType::Sniper },
		{ "taser",      EnemyType::Taser },
		{ "tower",      EnemyType::Tower },
		{ "shotgun",    EnemyType::Shotgun },
		{ "grenadier",  EnemyType::Grenadier },
		{ "civilian",   EnemyType::Civilian },
		{ "hostage",    EnemyType::Civilian },
		{ "smg",        EnemyType::SMG },
		{ "ar_",         EnemyType::AR },
		{ "swat",       EnemyType::ArmedCop },
		{ "guard",      EnemyType::Security },
		{ "cop",        EnemyType::ArmedCop },
		{ "enemy",      EnemyType::ArmedCop },
		{ "sbzai",      EnemyType::ArmedCop }
	};

	inline constexpr EnemyInfo g_EnemyInfo[] =
	{
		{ "Unknown",    EnemyCategory::None },
		{ "Security",   EnemyCategory::Cop },
		{ "Armed Cop",  EnemyCategory::Cop },
		{ "Shield",     EnemyCategory::Cop },
		{ "Dozer",      EnemyCategory::Cop },
		{ "Cloaker",    EnemyCategory::Cop },
		{ "Sniper",     EnemyCategory::Cop },
		{ "Taser",      EnemyCategory::Cop },
		{ "Tower",      EnemyCategory::Cop },
		{ "Shotgun",    EnemyCategory::Cop },
		{ "AR",         EnemyCategory::Cop },
		{ "SMG",        EnemyCategory::Cop },
		{ "Grenadier",  EnemyCategory::Cop },
		{ "Civilian",   EnemyCategory::Civilian },
		{ "Sentry Gun", EnemyCategory::Cop }
	};

	struct BoneCache
	{
		bool Initialized = false;

		// Core body
		int Hips = -1;
		int Spine = -1;
		int Spine1 = -1;
		int Spine2 = -1;
		int Spine3 = -1;
		int Neck = -1;
		int Head = -1;

		// Left arm
		int LeftShoulder = -1;
		int LeftUpperArm = -1;
		int LeftForeArm = -1;
		int LeftHand = -1;

		// Right arm
		int RightShoulder = -1;
		int RightUpperArm = -1;
		int RightForeArm = -1;
		int RightHand = -1;

		// Left leg
		int LeftUpperLeg = -1;
		int LeftLowerLeg = -1;
		int LeftFoot = -1;
		int LeftToe = -1;

		// Right leg
		int RightUpperLeg = -1;
		int RightLowerLeg = -1;
		int RightFoot = -1;
		int RightToe = -1;
	};

	// This is only used for the multiselect combo.
	enum class ItemCategory
	{
		None,
		Cash,
		DepositBox,
		Keycard
	};

	enum class ItemType
	{
		None,
		Cash,
		DepositBox,
		Keycard
	};

	struct ItemLookup
	{
		std::string_view Keyword;
		ItemType Type;
	};

	inline constexpr ItemLookup g_ItemLookup[] =
	{
		{ "instantloot_money_", ItemType::Cash },
		{ "depositbox",        ItemType::DepositBox },
		{ "keycard",           ItemType::Keycard }
	};

	struct ESPData
	{
		ImVec4 Box;
		std::string Name;
		SDK::USkeletalMeshComponent* Mesh;
		SDK::ASBZCharacter* Character;
		float Health;
		float HealthMax;
		float Armor;
		float ArmorMax;
		float Distance;
		bool IsCop;
		bool IsCivilian;
	};

	struct ItemData
	{
		SDK::FVector2D ScreenLocation;
		SDK::FVector WorldLocation;
		std::string Name;
		ItemType Type;
		SDK::AActor* Actor = nullptr;
	};

}