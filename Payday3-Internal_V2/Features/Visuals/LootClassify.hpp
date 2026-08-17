#pragma once
#include "pch.h"
#include "Types.hpp"
#include <string>

// Port of v1 ESP LabelForActor / loot classification — maps into V2 Cash / DepositBox / Keycard filters.
namespace LootClassify
{
	inline std::string ToLower(std::string s)
	{
		for (char& c : s)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return s;
	}

	inline bool Contains(const std::string& s, const char* needle)
	{
		return s.find(needle) != std::string::npos;
	}

	inline bool IsJunk(const std::string& s)
	{
		return Contains(s, "bucket") || Contains(s, "gallon") || Contains(s, "toast")
			|| Contains(s, "mop") || Contains(s, "broom") || Contains(s, "trash")
			|| Contains(s, "debris") || Contains(s, "decal") || Contains(s, "clutter")
			|| Contains(s, "prop_tool") || Contains(s, "cleaning")
			|| Contains(s, "brush") || Contains(s, "roller") || Contains(s, "tray")
			|| Contains(s, "paintbrush") || Contains(s, "paintroller") || Contains(s, "painttray")
			|| Contains(s, "paintcan") || Contains(s, "paint_can") || Contains(s, "ladder")
			|| Contains(s, "scaffold") || Contains(s, "tarp") || Contains(s, "dropcloth")
			|| Contains(s, "light") || Contains(s, "lamp") || Contains(s, "fixture")
			|| Contains(s, "sconce") || Contains(s, "bulb") || Contains(s, "spotlight")
			|| Contains(s, "luminaire") || Contains(s, "deco") || Contains(s, "decoration")
			|| Contains(s, "ornament") || Contains(s, "furniture")
			|| Contains(s, "computer") || Contains(s, "cooking") || Contains(s, "cookstation")
			|| Contains(s, "methcook") || Contains(s, "camera") || Contains(s, "securitycam");
	}

	struct Result
	{
		Types::ItemType Type = Types::ItemType::None;
		const char* Label = "Loot";
	};

	inline Result Classify(SDK::AActor* pActor)
	{
		Result out{};
		if (!pActor || !pActor->Class || pActor->IsActorBeingDestroyed())
			return out;

		const std::string low = ToLower(pActor->Class->Name.ToString());
		const std::string actorLow = ToLower(pActor->GetName());
		if (IsJunk(low) || IsJunk(actorLow))
			return out;

		// Keycards / tools / phones → Keycard filter
		if (Contains(low, "keycard") || Contains(low, "rfid") || Contains(actorLow, "keycard"))
			return { Types::ItemType::Keycard, "Keycard" };
		if (Contains(low, "qrphone") || Contains(low, "hackphone") || (Contains(low, "phone") && Contains(low, "bp_")))
			return { Types::ItemType::Keycard, "Phone" };
		if (Contains(low, "usb") || Contains(low, "usbdrive"))
			return { Types::ItemType::Keycard, "USB" };
		if (Contains(low, "c4") || Contains(low, "explosive"))
			return { Types::ItemType::Keycard, "C4" };
		if (Contains(low, "meth_caustic") || Contains(low, "causticsoda"))
			return { Types::ItemType::Keycard, "Caustic Soda" };
		if (Contains(low, "muriatic"))
			return { Types::ItemType::Keycard, "Muriatic Acid" };
		if (Contains(low, "hydrogenchloride") || Contains(low, "hydrogen_chloride"))
			return { Types::ItemType::Keycard, "Hydrogen Chloride" };
		if (Contains(low, "liquidnitrogen") || (Contains(low, "nitrogen") && Contains(low, "canister")))
			return { Types::ItemType::Keycard, "Liquid Nitrogen" };
		if (Contains(low, "plank"))
			return { Types::ItemType::Keycard, "Planks" };

		// Deposit boxes
		if (Contains(low, "depositbox") || Contains(low, "deposit_box") || Contains(low, "safedeposit"))
			return { Types::ItemType::DepositBox, "Deposit Box" };

		// Money / bags / valuables → Cash filter
		if (Contains(low, "interactablemoneypile") || Contains(low, "moneypile")
			|| Contains(low, "money_pile") || Contains(low, "moneycart")
			|| Contains(low, "loosecash") || Contains(low, "instantloot_money")
			|| Contains(low, "instantlootmoney") || Contains(low, "moneybagdyed")
			|| Contains(low, "dyed") || (Contains(low, "money") && Contains(low, "dye")))
			return { Types::ItemType::Cash, "Money" };

		if (Contains(low, "woodencrate") || low == "bp_woodencrate_c")
			return { Types::ItemType::Cash, "Crate" };
		if (Contains(low, "interactablepainting") || Contains(low, "paintingbag") || low == "bp_painting_c")
			return { Types::ItemType::Cash, "Painting" };
		if (Contains(low, "cocaine") || Contains(low, "cokepile") || Contains(low, "cocainepile"))
			return { Types::ItemType::Cash, "Coke" };
		if (Contains(low, "interactableserver") || Contains(low, "datacenterserver")
			|| (Contains(low, "server") && Contains(low, "interactable")))
			return { Types::ItemType::Cash, "Server" };
		if (Contains(low, "chus_evidence") || Contains(low, "evidencebag") || Contains(low, "evidence"))
			return { Types::ItemType::Cash, "Evidence" };
		if (Contains(low, "rarestone") || Contains(low, "raremineral") || Contains(low, "interactableopal")
			|| Contains(low, "satellitepart") || Contains(low, "satelliteprototype"))
			return { Types::ItemType::Cash, "Rare Loot" };

		auto LooksLikeInstantValuable = [](const std::string& s) -> bool
		{
			return Contains(s, "money") || Contains(s, "cash") || Contains(s, "jewel")
				|| Contains(s, "gold") || Contains(s, "diamond") || Contains(s, "watch")
				|| Contains(s, "necklace") || Contains(s, "bracelet") || Contains(s, "earring")
				|| Contains(s, "coke") || Contains(s, "cocaine")
				|| Contains(s, "jewelry") || Contains(s, "jewellery")
				|| Contains(s, "server") || Contains(s, "evidence")
				|| Contains(s, "rarestone") || Contains(s, "opal") || Contains(s, "satellite")
				|| Contains(s, "instantloot");
		};

		if (pActor->IsA(SDK::ASBZInstantLoot::StaticClass()))
		{
			auto* pLoot = reinterpret_cast<SDK::ASBZInstantLoot*>(pActor);
			if (pLoot->bIsLooted)
				return out;
			if (!LooksLikeInstantValuable(low) && !LooksLikeInstantValuable(actorLow))
				return out;
			if (Contains(low, "coke") || Contains(low, "cocaine") || Contains(actorLow, "coke"))
				return { Types::ItemType::Cash, "Coke" };
			if (Contains(low, "jewel") || Contains(low, "diamond") || Contains(low, "watch")
				|| Contains(low, "necklace") || Contains(low, "bracelet") || Contains(low, "earring")
				|| Contains(low, "gold") || Contains(low, "jewelry"))
				return { Types::ItemType::Cash, "Jewelry" };
			return { Types::ItemType::Cash, "Cash" };
		}

		if (pActor->IsA(SDK::ASBZSingleBagGenerator::StaticClass()))
		{
			if (Contains(low, "interactablepainting") || low == "bp_painting_c" || Contains(low, "paintingbag"))
				return { Types::ItemType::Cash, "Painting" };
			if (Contains(low, "moneypile") || Contains(low, "moneycart") || Contains(low, "money")
				|| Contains(low, "cash") || Contains(low, "dyed") || Contains(low, "dye"))
				return { Types::ItemType::Cash, "Money" };
			if (Contains(low, "jewel") || Contains(low, "jewelry") || Contains(low, "gold"))
				return { Types::ItemType::Cash, "Jewelry" };
			if (Contains(low, "coke") || Contains(low, "cocaine"))
				return { Types::ItemType::Cash, "Coke" };
			if (Contains(low, "server") || Contains(low, "evidence") || Contains(low, "opal")
				|| Contains(low, "satellite") || Contains(low, "rarestone"))
				return { Types::ItemType::Cash, "Loot" };
			// Unknown bag gens — still treat as cash so glow works
			if (Contains(low, "bag") || Contains(low, "valuable") || Contains(low, "loot"))
				return { Types::ItemType::Cash, "Bag" };
			return out;
		}

		if (pActor->IsA(SDK::ASBZMultiBagGenerator::StaticClass()))
		{
			auto* pMulti = reinterpret_cast<SDK::ASBZMultiBagGenerator*>(pActor);
			if (pMulti->NumberOfBags <= 0)
				return out;
			if (Contains(low, "cocaine") || Contains(low, "coke"))
				return { Types::ItemType::Cash, "Coke" };
			if (Contains(low, "money") || Contains(low, "cash") || Contains(low, "bag"))
				return { Types::ItemType::Cash, "Money" };
			return out;
		}

		if (Contains(low, "basevaluablebag") || Contains(low, "valuablebag") || Contains(low, "moneybag"))
		{
			if (Contains(low, "money") || Contains(low, "cash") || Contains(low, "dyed") || Contains(low, "dye"))
				return { Types::ItemType::Cash, "Money Bag" };
			if (Contains(low, "coke") || Contains(low, "cocaine"))
				return { Types::ItemType::Cash, "Coke Bag" };
			if (Contains(low, "bag") || Contains(low, "valuable"))
				return { Types::ItemType::Cash, "Bag" };
		}

		if (Contains(low, "moneybagdyed") || Contains(low, "dyedmoney")
			|| (Contains(low, "dyed") && (Contains(low, "money") || Contains(low, "cash") || Contains(low, "bag"))))
			return { Types::ItemType::Cash, "Money" };

		return out;
	}
}
