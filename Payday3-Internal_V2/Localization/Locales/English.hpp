#pragma once
#include "../Localization.hpp"

Locale_t localeEnglish{
	.sKey = "English",
	.ullKeyHash = "ENG"Hashed,
	.hMenuFont = &TahomaFont,
	.hFeatureFont = &TahomaFontFeature,
	.umLocalizedStrings = std::unordered_map<size_t, std::string>({
		{ "SIDEBAR"Hashed, "Sidebar" },
		{ "PLAYER_SEPERATOR"Hashed, "Player" },
		{ "MISC_SEPERATOR"Hashed, "Misc" },
		{ "CONFIG_BUTTON"Hashed, "Config" },

		//-----------------------------------------------------------------

		{ "PLAYER_BUTTON"Hashed, "Player" },
		{ "PLAYER_TAB1"Hashed, "Local Player" },
		{ "PLAYER_TAB2"Hashed, "Players" },
		{ "PLAYER_TAB3"Hashed, "Weapon Mods" },

		{ "PLAYER_TAB1_LEFT"Hashed, "General" },
		{ "PLAYER_TAB1_RIGHT"Hashed, "Options" },

		{ "PLAYER_TAB2_LEFT"Hashed, "Details" },

		{ "PLAYER_TAB3_LEFT"Hashed, "Mods" },
		{ "PLAYER_TAB3_RIGHT"Hashed, "Options" },

		//Tab 1
		{ "PLAYER_GODMODE_TYPE"Hashed, "Godmode Type" },
		{ "PLAYER_GODMODE"Hashed, "Godmode" },
		{ "PLAYER_INF_STAMINA"Hashed, "Infinite Stamina" },
		{ "PLAYER_INSTA_MELEE"Hashed, "Instant Melee" },
		{ "PLAYER_NO_SCREENSHAKE"Hashed, "No Screenshake" },
		{ "PLAYER_NO_FALLDAMAGE"Hashed, "No Fall Damage" },
		{ "PLAYER_NO_DETECTION"Hashed, "No Detection" },

		{ "PLAYER_FREECAM"Hashed, "Freecam" },
		{ "PLAYER_FREECAM_KEY"Hashed, "Freecam Key" },
		{ "PLAYER_FREECAM_FASTER_KEY"Hashed, "Fly Faster Key" },
		{ "PLAYER_FREECAM_SPEED"Hashed, "Fly Speed" },

		//Tab2
		{ "PLAYER_TABLE"Hashed, "Players" },
		{ "PLAYER_TABLE_ROW1"Hashed, "Row 1" },

		//Tab3
		{ "PLAYER_INSTA_RELOAD"Hashed, "Instant Reload" },
		{ "PLAYER_INF_AMMO"Hashed, "Infinite Ammo" },
		{ "PLAYER_NO_RECOIL"Hashed, "No Recoil" },
		{ "PLAYER_NO_SPREAD"Hashed, "No Spread" },
		{ "PLAYER_FIRE_RATE_SLIDER"Hashed, "Fire Rate" },
		{ "PLAYER_FIRE_RATE"Hashed, "Fire Rate" },

		//-----------------------------------------------------------------

		{ "VISUALS_BUTTON"Hashed, "Visuals" },
        { "VISUALS_TAB1"Hashed, "ESP" },
        { "VISUALS_TAB1_LEFT"Hashed, "ESP" },
        { "VISUALS_TAB1_RIGHT"Hashed, "Colors" },
        { "VISUALS_TAB1_BOTTOM"Hashed, "Filters" },

        { "VISUALS_BOUNDING_BOX"Hashed, "Bounding Box" },
        { "VISUALS_BOUNDING_BOX_COP_COLOR"Hashed, "Bounding Box Cop Color" },
        { "VISUALS_BOUNDING_BOX_CIVILIAN_COLOR"Hashed, "Bounding Box Civilian Color" },

        { "VISUALS_NAME"Hashed, "Name" },
        { "VISUALS_NAME_COP_COLOR"Hashed, "Name Cop Color" },
        { "VISUALS_NAME_CIVILIAN_COLOR"Hashed, "Name Civilian Color" },

        { "VISUALS_DISTANCE"Hashed, "Distance" },
        { "VISUALS_DISTANCE_COP_COLOR"Hashed, "Distance Cop Color" },
        { "VISUALS_DISTANCE_CIVILIAN_COLOR"Hashed, "Distance Civilian Color" },

        { "VISUALS_HEALTH_BAR"Hashed, "Health Bar" },
        { "VISUALS_HEALTH_BAR_COP_COLOR"Hashed, "Health Bar Cop Color" },
        { "VISUALS_HEALTH_BAR_CIVILIAN_COLOR"Hashed, "Health Bar Civilian Color" },

        { "VISUALS_ARMOR_BAR"Hashed, "Armor Bar" },
        { "VISUALS_ARMOR_BAR_COLOR"Hashed, "Armor Bar Color" },

        { "VISUALS_SKELETON"Hashed, "Skeleton" },
        { "VISUALS_SKELETON_COP_COLOR"Hashed, "Skeleton Cop Color" },
        { "VISUALS_SKELETON_CIVILIAN_COLOR"Hashed, "Skeleton Civilian Color" },

        { "VISUALS_HIGHLIGHT"Hashed, "Highlight" },

        { "VISUALS_OUTLINE"Hashed, "Outline" },
        { "VISUALS_OUTLINE_COP_COLOR"Hashed, "Outline Cop" },
        { "VISUALS_OUTLINE_CIVILIAN_COLOR"Hashed, "Outline Civilian" },
        { "VISUALS_OUTLINE_CASH_COLOR"Hashed, "Outline Cash" },
        { "VISUALS_OUTLINE_DEPOSITBOX_COLOR"Hashed, "Outline Deposit Box" },
        { "VISUALS_OUTLINE_KEYCARD_COLOR"Hashed, "Outline Keycard" },

        { "VISUALS_ITEM"Hashed, "Items" },
        { "VISUALS_ITEM_CASH_COLOR"Hashed, "Cash Color" },
        { "VISUALS_ITEM_DEPOSITBOX_COLOR"Hashed, "Deposit Box Color" },
        { "VISUALS_ITEM_KEYCARD_COLOR"Hashed, "Keycard Color" },

        { "VISUALS_FILTERS"Hashed, "" },
        { "VISUALS_ITEM_FILTERS"Hashed, "" },

		//------------------------------------------------------------

		{ "AIMBOT_BUTTON"Hashed, "Aimbot" },
		{ "AIMBOT_TAB1"Hashed, "Main" },
		{ "AIMBOT_TAB1_LEFT"Hashed, "Aimbot" },
		{ "AIMBOT_TAB1_RIGHT"Hashed, "Options" },

		{ "AIMBOT_ENABLED"Hashed, "Aimbot" },
		{ "AIMBOT_VISIBLE_CHECK"Hashed, "Visible Check" },
		{ "AIMBOT_FOV_ENABLED"Hashed, "FOV Circle" },

		{ "AIMBOT_HOTKEY"Hashed, "Aimbot Hotkey" },
		{ "AIMBOT_TYPE"Hashed, "Aimbot Type" },
		{ "AIMBOT_TARGET"Hashed, "Target" },
		{ "AIMBOT_FOV_COLOR"Hashed, "FOV Color" },
		{ "AIMBOT_FOV"Hashed, "FOV" },
		{ "AIMBOT_SMOOTHING"Hashed, "Smoothing" },

		//------------------------------------------------------------

		{ "MISC_BUTTON"Hashed, "Heist" },
		{ "MISC_TAB1"Hashed, "Tools" },
		{ "MISC_TAB2"Hashed, "Teleport" },
		{ "MISC_TAB3"Hashed, "Vault / Spawn" },
		{ "MISC_TAB1_LEFT"Hashed, "Ghost / Kill / Drill" },
		{ "MISC_TAB1_RIGHT"Hashed, "Grab" },
		{ "MISC_TAB2_LEFT"Hashed, "Presets" },
		{ "MISC_TAB3_LEFT"Hashed, "Vault Codes" },
		{ "MISC_TAB3_RIGHT"Hashed, "Spawners" },

		{ "MISC_GHOST"Hashed, "Ghost Mode" },
		{ "MISC_GHOST_KEY"Hashed, "Ghost Key" },
		{ "MISC_SILENT_BURY"Hashed, "Silent Bury Kill" },
		{ "MISC_SILENT_BURY_KEY"Hashed, "Silent Bury Key" },
		{ "MISC_GRAB_LOOT"Hashed, "Grab All Loot" },
		{ "MISC_GRAB_LOOT_KEY"Hashed, "Grab Loot Key" },
		{ "MISC_GRAB_KEYS"Hashed, "Grab All Keycards" },
		{ "MISC_GRAB_KEYS_KEY"Hashed, "Grab Keycards Key" },
		{ "MISC_CARRY_BAGS"Hashed, "Carry 50 Bags (You + AI + Players)" },
		{ "MISC_CARRY_BAGS_KEY"Hashed, "Carry Bags Key" },
		{ "MISC_NO_CIV"Hashed, "No Civ / Custody Penalty" },
		{ "MISC_NO_CIV_KEY"Hashed, "No Civ Key" },
		{ "MISC_INSTA_DRILL"Hashed, "Insta Drill (Drills / PCs / Thermite)" },
		{ "MISC_INSTA_DRILL_KEY"Hashed, "Insta Drill Key" },

		{ "MISC_TP_LOAD"Hashed, "Load JSON" },
		{ "MISC_TP_PREV"Hashed, "Prev" },
		{ "MISC_TP_NEXT"Hashed, "Next" },
		{ "MISC_TP_HERE"Hashed, "TP Here" },
		{ "MISC_TP_ALL"Hashed, "TP All" },
		{ "MISC_TP_TOUR"Hashed, "Tour Spots" },
		{ "MISC_TP_STOP"Hashed, "Stop" },
		{ "MISC_TP_HERE_KEY"Hashed, "TP Here Key" },
		{ "MISC_TP_ALL_KEY"Hashed, "TP All Key" },
		{ "MISC_TP_PREV_KEY"Hashed, "Prev Spot Key" },
		{ "MISC_TP_NEXT_KEY"Hashed, "Next Spot Key" },
		{ "MISC_TP_TOUR_KEY"Hashed, "Tour Key" },
		{ "MISC_TP_DELAY"Hashed, "Tour Delay" },
		{ "MISC_TP_SPOTS"Hashed, "Spot" },

		{ "MISC_VAULT_SCAN"Hashed, "Scan Vault Codes" },
		{ "MISC_VAULT_SCAN_KEY"Hashed, "Vault Codes Key" },
		{ "MISC_KEYPAD_HELPER"Hashed, "Keypad Helper (Only Correct Digit)" },
		{ "MISC_SPAWN_METH"Hashed, "Spawn Meth Bags" },
		{ "MISC_SPAWN_VAN"Hashed, "Call Escape Van" },
		{ "MISC_SPAWN_EXIT"Hashed, "Green Exit / Arm Leave" },
		{ "MISC_SPAWN_MONEY"Hashed, "Money Screen / End Heist" },

		//------------------------------------------------------------

		{ "UI"Hashed, "UI" },
		{ "UI_BUTTON"Hashed, "UI" },
		{ "UI_MAIN"Hashed, "Main" },
		{ "UI_UNLOAD_BUTTON"Hashed, "Unload" },
		{ "UI_CONSOLE_SHOW"Hashed, "Show Console" },
		{ "UI_CONSOLE_HIDE"Hashed, "Hide Console" },
		{ "UI_LANGUAGE"Hashed, "Language" },

		//------------------------------------------------------------

		{ "CHEAT"Hashed, "Cheat"},
		{ "SAVE_CONFIG"Hashed, "Save Config" },
		{ "LOAD_CONFIG"Hashed, "Load Config" },
	}),
};
