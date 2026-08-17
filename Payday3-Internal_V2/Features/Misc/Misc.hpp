#pragma once
#include "pch.h"

class Misc : public BaseFeature
{
private:
	inline static uint8_t s_iMiscPageId = ElementBase::AddPage("MISC_BUTTON"Hashed, ICON_FA_GHOST);

	std::unique_ptr<RadioButtonIcon> m_pMenuButton = std::make_unique<RadioButtonIcon>(
		std::string("MISC_BUTTON"),
		"MISC_BUTTON"Hashed,
		ElementBase::Style_t({ .vec2Size = ImVec2(-0.1f, 0) }),
		ICON_FA_GHOST,
		s_iMiscPageId);

	std::unique_ptr<Page> m_pTab1Page = std::make_unique<Page>("MISC_TAB1_PAGE", ElementBase::Style_t(), s_iMiscPageId, 0);
	std::unique_ptr<Page> m_pTab2Page = std::make_unique<Page>("MISC_TAB2_PAGE", ElementBase::Style_t(), s_iMiscPageId, 1);
	std::unique_ptr<Page> m_pTab3Page = std::make_unique<Page>("MISC_TAB3_PAGE", ElementBase::Style_t(), s_iMiscPageId, 2);

	std::unique_ptr<Group> m_pTab1Group = std::make_unique<Group>("MISC_TAB1_GROUP", ElementBase::Style_t{});
	std::unique_ptr<Group> m_pTab2Group = std::make_unique<Group>("MISC_TAB2_GROUP", ElementBase::Style_t{});
	std::unique_ptr<Group> m_pTab3Group = std::make_unique<Group>("MISC_TAB3_GROUP", ElementBase::Style_t{});

	std::unique_ptr<GroupChild> m_pTab1Left = std::make_unique<GroupChild>(
		"MISC_TAB1_LEFT", "MISC_TAB1_LEFT"Hashed, ElementBase::Style_t(), ImGuiChildFlags_Border);
	std::unique_ptr<GroupChild> m_pTab1Right = std::make_unique<GroupChild>(
		"MISC_TAB1_RIGHT", "MISC_TAB1_RIGHT"Hashed, ElementBase::Style_t{ .eSameLine = ElementBase::ESameLine::Same }, ImGuiChildFlags_Border);

	std::unique_ptr<GroupChild> m_pTab2Left = std::make_unique<GroupChild>(
		"MISC_TAB2_LEFT", "MISC_TAB2_LEFT"Hashed, ElementBase::Style_t(), ImGuiChildFlags_Border);

	std::unique_ptr<GroupChild> m_pTab3Left = std::make_unique<GroupChild>(
		"MISC_TAB3_LEFT", "MISC_TAB3_LEFT"Hashed, ElementBase::Style_t(), ImGuiChildFlags_Border);
	std::unique_ptr<GroupChild> m_pTab3Right = std::make_unique<GroupChild>(
		"MISC_TAB3_RIGHT", "MISC_TAB3_RIGHT"Hashed, ElementBase::Style_t{ .eSameLine = ElementBase::ESameLine::Same }, ImGuiChildFlags_Border);

	// Tools
	std::unique_ptr<Checkbox> m_pGhost = std::make_unique<Checkbox>("MISC_GHOST", "MISC_GHOST"Hashed);
	std::unique_ptr<Hotkey> m_pGhostKey = std::make_unique<Hotkey>("MISC_GHOST_KEY", "MISC_GHOST_KEY"Hashed);
	std::unique_ptr<_Text> m_pGhostStatus = std::make_unique<_Text>("MISC_GHOST_STATUS", std::string("Ghost off"));

	std::unique_ptr<Checkbox> m_pSilentBury = std::make_unique<Checkbox>("MISC_SILENT_BURY", "MISC_SILENT_BURY"Hashed);
	std::unique_ptr<Hotkey> m_pSilentBuryKey = std::make_unique<Hotkey>("MISC_SILENT_BURY_KEY", "MISC_SILENT_BURY_KEY"Hashed);
	std::unique_ptr<_Text> m_pSilentBuryStatus = std::make_unique<_Text>("MISC_SILENT_BURY_STATUS", std::string("Silent bury off"));

	std::unique_ptr<Checkbox> m_pCarryBags = std::make_unique<Checkbox>("MISC_CARRY_BAGS", "MISC_CARRY_BAGS"Hashed);
	std::unique_ptr<Hotkey> m_pCarryBagsKey = std::make_unique<Hotkey>("MISC_CARRY_BAGS_KEY", "MISC_CARRY_BAGS_KEY"Hashed);
	std::unique_ptr<_Text> m_pCarryBagsStatus = std::make_unique<_Text>("MISC_CARRY_BAGS_STATUS", std::string("Carry More Bags off"));

	std::unique_ptr<Checkbox> m_pNoCiv = std::make_unique<Checkbox>("MISC_NO_CIV", "MISC_NO_CIV"Hashed);
	std::unique_ptr<Hotkey> m_pNoCivKey = std::make_unique<Hotkey>("MISC_NO_CIV_KEY", "MISC_NO_CIV_KEY"Hashed);
	std::unique_ptr<_Text> m_pNoCivStatus = std::make_unique<_Text>("MISC_NO_CIV_STATUS", std::string("No Civ/Custody Penalty off"));

	std::unique_ptr<Checkbox> m_pInstaDrill = std::make_unique<Checkbox>("MISC_INSTA_DRILL", "MISC_INSTA_DRILL"Hashed);
	std::unique_ptr<Hotkey> m_pInstaDrillKey = std::make_unique<Hotkey>("MISC_INSTA_DRILL_KEY", "MISC_INSTA_DRILL_KEY"Hashed);
	std::unique_ptr<_Text> m_pInstaDrillStatus = std::make_unique<_Text>("MISC_INSTA_DRILL_STATUS", std::string("InstaDrill off"));

	std::unique_ptr<Checkbox> m_pGrabLoot = std::make_unique<Checkbox>("MISC_GRAB_LOOT", "MISC_GRAB_LOOT"Hashed);
	std::unique_ptr<Hotkey> m_pGrabLootKey = std::make_unique<Hotkey>("MISC_GRAB_LOOT_KEY", "MISC_GRAB_LOOT_KEY"Hashed);
	std::unique_ptr<_Text> m_pGrabLootStatus = std::make_unique<_Text>("MISC_GRAB_LOOT_STATUS", std::string("Grab loot off"));

	std::unique_ptr<Checkbox> m_pGrabKeys = std::make_unique<Checkbox>("MISC_GRAB_KEYS", "MISC_GRAB_KEYS"Hashed);
	std::unique_ptr<Hotkey> m_pGrabKeysKey = std::make_unique<Hotkey>("MISC_GRAB_KEYS_KEY", "MISC_GRAB_KEYS_KEY"Hashed);
	std::unique_ptr<_Text> m_pGrabKeysStatus = std::make_unique<_Text>("MISC_GRAB_KEYS_STATUS", std::string("Grab keys off"));

	// Teleport
	std::unique_ptr<Button> m_pLoadJson = std::make_unique<Button>("MISC_TP_LOAD", "MISC_TP_LOAD"Hashed);
	std::unique_ptr<Button> m_pPrev = std::make_unique<Button>("MISC_TP_PREV", "MISC_TP_PREV"Hashed, ElementBase::Style_t{ .eSameLine = ElementBase::ESameLine::Same });
	std::unique_ptr<Button> m_pNext = std::make_unique<Button>("MISC_TP_NEXT", "MISC_TP_NEXT"Hashed, ElementBase::Style_t{ .eSameLine = ElementBase::ESameLine::Same });
	std::unique_ptr<Button> m_pTpHere = std::make_unique<Button>("MISC_TP_HERE", "MISC_TP_HERE"Hashed);
	std::unique_ptr<Button> m_pTpAll = std::make_unique<Button>("MISC_TP_ALL", "MISC_TP_ALL"Hashed, ElementBase::Style_t{ .eSameLine = ElementBase::ESameLine::Same });
	std::unique_ptr<Button> m_pTour = std::make_unique<Button>("MISC_TP_TOUR", "MISC_TP_TOUR"Hashed, ElementBase::Style_t{ .eSameLine = ElementBase::ESameLine::Same });
	std::unique_ptr<Button> m_pStop = std::make_unique<Button>("MISC_TP_STOP", "MISC_TP_STOP"Hashed, ElementBase::Style_t{ .eSameLine = ElementBase::ESameLine::Same });

	std::unique_ptr<Hotkey> m_pTpHereKey = std::make_unique<Hotkey>("MISC_TP_HERE_KEY", "MISC_TP_HERE_KEY"Hashed);
	std::unique_ptr<Hotkey> m_pTpAllKey = std::make_unique<Hotkey>("MISC_TP_ALL_KEY", "MISC_TP_ALL_KEY"Hashed);
	std::unique_ptr<Hotkey> m_pPrevKey = std::make_unique<Hotkey>("MISC_TP_PREV_KEY", "MISC_TP_PREV_KEY"Hashed);
	std::unique_ptr<Hotkey> m_pNextKey = std::make_unique<Hotkey>("MISC_TP_NEXT_KEY", "MISC_TP_NEXT_KEY"Hashed);
	std::unique_ptr<Hotkey> m_pTourKey = std::make_unique<Hotkey>("MISC_TP_TOUR_KEY", "MISC_TP_TOUR_KEY"Hashed);

	std::unique_ptr<SliderFloat> m_pTourDelay = std::make_unique<SliderFloat>("MISC_TP_DELAY", "MISC_TP_DELAY"Hashed, ElementBase::Style_t{}, 1.5f, 0.3f, 5.f, "%.1f");
	std::unique_ptr<Combo> m_pSpotCombo = std::make_unique<Combo>("MISC_TP_SPOTS", "MISC_TP_SPOTS"Hashed, ElementBase::Style_t{ .iFlags = ImGuiComboFlags_WidthFitPreview });
	std::unique_ptr<_Text> m_pTpStatus = std::make_unique<_Text>("MISC_TP_STATUS", std::string("No JSON loaded — click Load JSON"));

	// Vault / Spawn
	std::unique_ptr<Button> m_pVaultScan = std::make_unique<Button>("MISC_VAULT_SCAN", "MISC_VAULT_SCAN"Hashed);
	std::unique_ptr<Hotkey> m_pVaultScanKey = std::make_unique<Hotkey>("MISC_VAULT_SCAN_KEY", "MISC_VAULT_SCAN_KEY"Hashed);
	std::unique_ptr<_Text> m_pVaultStatus = std::make_unique<_Text>("MISC_VAULT_STATUS", std::string("Press F10 / Vault Codes to scan"));
	std::unique_ptr<Checkbox> m_pKeypadHelper = std::make_unique<Checkbox>("MISC_KEYPAD_HELPER", "MISC_KEYPAD_HELPER"Hashed);
	std::unique_ptr<_Text> m_pKeypadHelperStatus = std::make_unique<_Text>("MISC_KEYPAD_HELPER_STATUS", std::string("Keypad helper off"));

	std::unique_ptr<Button> m_pSpawnMeth = std::make_unique<Button>("MISC_SPAWN_METH", "MISC_SPAWN_METH"Hashed);
	std::unique_ptr<_Text> m_pSpawnMethStatus = std::make_unique<_Text>("MISC_SPAWN_METH_STATUS", std::string("idle"));
	std::unique_ptr<Button> m_pSpawnVan = std::make_unique<Button>("MISC_SPAWN_VAN", "MISC_SPAWN_VAN"Hashed);
	std::unique_ptr<_Text> m_pSpawnVanStatus = std::make_unique<_Text>("MISC_SPAWN_VAN_STATUS", std::string("idle"));
	std::unique_ptr<Button> m_pSpawnExit = std::make_unique<Button>("MISC_SPAWN_EXIT", "MISC_SPAWN_EXIT"Hashed);
	std::unique_ptr<_Text> m_pSpawnExitStatus = std::make_unique<_Text>("MISC_SPAWN_EXIT_STATUS", std::string("idle"));
	std::unique_ptr<Button> m_pSpawnMoney = std::make_unique<Button>("MISC_SPAWN_MONEY", "MISC_SPAWN_MONEY"Hashed);
	std::unique_ptr<_Text> m_pSpawnMoneyStatus = std::make_unique<_Text>("MISC_SPAWN_MONEY_STATUS", std::string("idle"));

	void PollHotkeyToggle(Checkbox* pBox, Hotkey* pKey);
	void PollHotkeyPress(Hotkey* pKey, const std::function<void()>& fn);
	void DrawVaultFlash();

public:
	void HandleMenu();
	void Render();
	void Run();
	RadioButtonIcon* GetMenuButton() const { return m_pMenuButton.get(); }
	std::string GetName() { return "Misc"; }
};

inline std::unique_ptr<Misc> pMisc = std::make_unique<Misc>();
