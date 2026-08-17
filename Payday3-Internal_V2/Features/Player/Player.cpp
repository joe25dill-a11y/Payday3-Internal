#include "pch.h"
#include "Player.hpp"
#include "ClientMove.hpp"
#include <vector>
#include <algorithm>

bool Player::Setup()
{
	// Godmode toggled off so disable turn off whatever godmode type was selected
	m_pGodMode->SetOnValueChangedCallback([this](const bool, const bool bNewValue)
	{
		SDK::ASBZPlayerCharacter* localChar = Unreal::GetLocalCharacter();
		if (!localChar)
			return;

		const int currentType = m_pGodModeType->GetSelectedIndex();

		if (!bNewValue)
		{
			if (currentType == 0)
			{
				auto* playerAttributeSet = localChar->PlayerAttributeSet;
				if (!playerAttributeSet)
					return;

				auto backup = GetAttrBackupData(localChar);
				if (backup)
				{
					playerAttributeSet->Health.CurrentValue = backup->m_flHealth.CurrentValue;
				}
			}
			else if (currentType == 1)
			{
				blockDamage(false);
			}
		}
	});

	// Godmode still on but type is changed so disable previous type
	m_pGodModeType->SetOnValueChangedCallback([this](const int oldIndex, const int newIndex)
	{
		if (m_pGodMode->GetValue())
		{
			SDK::ASBZPlayerCharacter* localChar = Unreal::GetLocalCharacter();
			if (!localChar)
				return;

			if (oldIndex == 0)
			{
				auto* playerAttributeSet = localChar->PlayerAttributeSet;
				if (!playerAttributeSet)
					return;

				auto backup = GetAttrBackupData(localChar);
				if (backup)
				{
					playerAttributeSet->Health.CurrentValue = backup->m_flHealth.CurrentValue;
				}
			}
			else if (oldIndex == 1)
			{
				blockDamage(false);
			}
		}
	});

	m_pInfAmmo->SetOnValueChangedCallback([this](const bool, const bool bNewValue) {
		if (!bNewValue)
		{
			SDK::ASBZPlayerCharacter* localChar = Unreal::GetLocalCharacter();
			auto* PlayerAttributeSet = localChar->PlayerAttributeSet;
			if (!PlayerAttributeSet)
				return;

			auto backup = GetAttrBackupData(localChar);
			if(backup)
			{
				PlayerAttributeSet->PrimaryEquippableAmmoInventory.CurrentValue = backup->m_flPrimaryEquippableAmmoInventory.CurrentValue;
				PlayerAttributeSet->SecondaryEquippableAmmoInventory.CurrentValue = backup->m_flSecondaryEquippableAmmoInventory.CurrentValue;
				PlayerAttributeSet->TertiaryEquippableAmmoInventory.CurrentValue = backup->m_flTertiaryEquippableAmmoInventory.CurrentValue;

				PlayerAttributeSet->PrimaryThrowableAmmoInventory.CurrentValue = backup->m_flPrimaryThrowableAmmoInventory.CurrentValue;
				PlayerAttributeSet->SecondaryThrowableAmmoInventory.CurrentValue = backup->m_flSecondaryThrowableAmmoInventory.CurrentValue;
				PlayerAttributeSet->TertiaryThrowableAmmoInventory.CurrentValue = backup->m_flTertiaryThrowableAmmoInventory.CurrentValue;

				PlayerAttributeSet->PrimaryToolAmmoInventory.CurrentValue = backup->m_flPrimaryToolAmmoInventory.CurrentValue;
				PlayerAttributeSet->SecondaryToolAmmoInventory.CurrentValue = backup->m_flSecondaryToolAmmoInventory.CurrentValue;
				PlayerAttributeSet->TertiaryToolAmmoInventory.CurrentValue = backup->m_flTertiaryToolAmmoInventory.CurrentValue;
			}
		}
	});

	m_pNoRecoil->SetOnValueChangedCallback([this](const bool, const bool bNewValue) {
		if (!bNewValue)
		{
			noRecoil(false);
		}
	});

	m_pNoSpread->SetOnValueChangedCallback([this](const bool, const bool bNewValue) {
		if (!bNewValue)
		{
			noSpread(false);
		}
	});

	m_pFireRate->SetOnValueChangedCallback([this](const bool, const bool bNewValue) {
		if (!bNewValue)
		{
			fireRate(false);
		}
	});

	return true;
}
void Player::HandleMenu()
{
	static std::once_flag onceflag;

	std::call_once(onceflag, [this]() {
		auto pHeaderGroup = static_cast<HeaderGroup*>(Framework::menu->GetChild("HEADER_GROUP"));
		if (pHeaderGroup)
			pHeaderGroup->AddHeaders(Player::s_iPlayerPageId, { "PLAYER_TAB1"Hashed, "PLAYER_TAB2"Hashed, "PLAYER_TAB3"Hashed });

		m_pTab1Left->SetCallback([]() {
			return ImVec2((ImGui::GetWindowWidth() - 10.0f - 10.0f * 2) / 2, (ImGui::GetWindowHeight() - 20.0f));
		});
		m_pTab1Right->SetCallback([]() {
			return ImVec2((ImGui::GetWindowWidth() - 10.0f - 10.0f * 2) / 2, (ImGui::GetWindowHeight() - 20.0f));
		});
		m_pTab2Left->SetCallback([]() {
			return ImVec2((ImGui::GetWindowWidth() - 20.0f), (ImGui::GetWindowHeight() - 20.0f));
		});
		m_pTab3Left->SetCallback([]() {
			return ImVec2((ImGui::GetWindowWidth() - 10.0f - 10.0f * 2) / 2, (ImGui::GetWindowHeight() - 20.0f));
		});
		m_pTab3Right->SetCallback([]() {
			return ImVec2((ImGui::GetWindowWidth() - 10.0f - 10.0f * 2) / 2, (ImGui::GetWindowHeight() - 20.0f));
		});

		/////////////////Tab 1////////////////////

		// Godmode Toggle
		m_pTab1Left->AddElement(m_pGodMode.get());

		// Infinite Stamina Toggle
		m_pTab1Left->AddElement(m_pInfStamina.get());

		// Instant Melee Toggle
		m_pTab1Left->AddElement(m_pInstaMelee.get());
		m_pInstaMelee->SetOnValueChangedCallback([this](const bool, const bool bNewValue) {
			InstantMelee(bNewValue);
		});

		// No Screenshake Toggle
		m_pTab1Left->AddElement(m_pNoScreenshake.get());

		// No Fall Damage Toggle
		m_pTab1Left->AddElement(m_pNoFallDamage.get());

		// No Detection Toggle
		m_pTab1Left->AddElement(m_pNoDetection.get());

		// Freecam (ScoutFreecam ApplyFreecam, in-DLL — F7)
		m_pTab1Left->AddElement(m_pFreecam.get());
		m_pTab1Left->AddElement(m_pFreecamKey.get());
		m_pTab1Left->AddElement(m_pFreecamFasterKey.get());
		m_pTab1Right->AddElement(m_pFreecamSpeed.get());
		if (m_pFreecamKey->GetKey() == ImGuiKey_None)
			m_pFreecamKey->SetKey(ImGuiKey_F7);
		m_pFreecamKey->SetMode(Hotkey::EHotkeyMode::Toggle);
		if (m_pFreecamFasterKey->GetKey() == ImGuiKey_None)
			m_pFreecamFasterKey->SetKey(ImGuiKey_LeftShift);
		m_pFreecamFasterKey->SetMode(Hotkey::EHotkeyMode::Hold);
		m_pFreecam->SetValue(false);

		// Godmode Type
		m_pTab1Right->AddElement(m_pGodModeType.get());
		m_pGodModeType->AddOption("Set");
		m_pGodModeType->AddOption("Block");

		m_pTab1Group->AddElement(m_pTab1Left.get());
		m_pTab1Group->AddElement(m_pTab1Right.get());

		////////////////Tab 2///////////////////

		//Add a ?x? table then populate it with player(s) info
		//Need to figure out then best way to interact with the players in the table, maybe a left click player color highlight and then select an action from below the table ?
		//Edit: Implemented tables, just need to do the populate it with player info and then work on interacting with table rows and selecting actions for the selected player(s).
		//Edit2: I'll have to make the tables look better first before populating cus they look a lil ugly rn
		//Edit3: They're a bit better now and changed the functionality a bit to allow for more dynamic table creation and population.
		// I still need to make the color fill the rest of the table instead of just leaving blank space around the table
		m_pTab2Left->AddElement(m_pPlayerTable.get());

		m_pPlayerTable->SetColumn(0, "Name");
		m_pPlayerTable->SetColumn(1, "Health");
		m_pPlayerTable->SetColumn(2, "Distance");
		m_pPlayerTable->SetColumn(3, "Status");

		m_pPlayerTable->AddRow({
			"Omega",
			"100",
			"125m",
			"Alive"
		});
		m_pPlayerTable->AddRow({
			"Bob",
			"75",
			"84m",
			"Alive"
		});

		m_pTab2Group->AddElement(m_pTab2Left.get());

		////////////////Tab 3///////////////////

		m_pTab3Left->AddElement(m_pInstaReload.get());
		m_pTab3Left->AddElement(m_pInfAmmo.get());
		m_pTab3Left->AddElement(m_pNoRecoil.get());
		m_pTab3Left->AddElement(m_pNoSpread.get());
		m_pTab3Left->AddElement(m_pFireRate.get());
		m_pTab3Right->AddElement(m_pFireRateSlider.get());

		m_pTab3Group->AddElement(m_pTab3Left.get());
		m_pTab3Group->AddElement(m_pTab3Right.get());

		m_pTab1Page->AddElement(m_pTab1Group.get());
		m_pTab2Page->AddElement(m_pTab2Group.get());
		m_pTab3Page->AddElement(m_pTab3Group.get());

		Framework::menu->GetChild("HEADER_GROUP")->GetChild("BODY")->AddElement(m_pTab1Page.get());
		Framework::menu->GetChild("HEADER_GROUP")->GetChild("BODY")->AddElement(m_pTab2Page.get());
		Framework::menu->GetChild("HEADER_GROUP")->GetChild("BODY")->AddElement(m_pTab3Page.get());
	});
}

void Player::blockDamage(bool bEnabled)
{
	auto* localChar = Unreal::GetLocalCharacter();
	if (!localChar)
	{
		m_bBlockDamageApplied = false;
		return;
	}

	if (bEnabled) //I'm not kidding I had to design the method this way because bCanBeDamaged wasn't disabling for some reason. Digusting but it works.
	{
		if (!m_bBlockDamageApplied)
		{
			m_bOriginalCanBeDamaged = localChar->bCanBeDamaged;
			m_bBlockDamageApplied = false;
		}

		localChar->bCanBeDamaged = false;
		m_bBlockDamageApplied = true;
	}
	else if (m_bBlockDamageApplied)
	{
		localChar->bCanBeDamaged = m_bOriginalCanBeDamaged;
		m_bBlockDamageApplied = false;
	}
}

void Player::InstantMelee(bool bEnabled)
{
	if (auto* localChar = Unreal::GetLocalCharacter())
	{

		auto* montage = localChar->CurrentMeleeMontage;
		if (!montage)
		{
			m_pLastMeleeMontage = nullptr;
			m_bInstantMeleeApplied = false;
			return;
		}

		if (bEnabled)
		{
			if (montage != m_pLastMeleeMontage)
			{
				m_pLastMeleeMontage = montage;
				m_flOriginalMeleeRate = montage->RateScale;
				m_bInstantMeleeApplied = false;
			}

			if (!m_bInstantMeleeApplied || montage->RateScale != 10000.0f)
			{
				montage->RateScale = 10000.0f;

				if (montage->RateScale == 10000.0f)
					m_bInstantMeleeApplied = true;
			}
		}
		else
		{
			if (m_bInstantMeleeApplied)
			{
				montage->RateScale = m_flOriginalMeleeRate;

				m_bInstantMeleeApplied = false;
				m_pLastMeleeMontage = nullptr;
			}
		}
	}
}

void Player::InstantReload(bool bEnabled)
{
	if (auto* localChar = Unreal::GetLocalCharacter())
	{

		auto* montage = localChar->LastLocalReloadMontage;
		if (!montage)
		{
			m_pLastReloadMontage = nullptr;
			m_bInstantReloadApplied = false;
			return;
		}

		if (bEnabled)
		{
			if (montage != m_pLastReloadMontage)
			{
				m_pLastReloadMontage = montage;
				m_flOriginalReloadRate = montage->RateScale;
				m_bInstantReloadApplied = false;
			}

			// Keep trying until ratescale is applied
			if (!m_bInstantReloadApplied || montage->RateScale != 10000.0f)
			{
				montage->RateScale = 10000.0f;

				if (montage->RateScale == 10000.0f)
					m_bInstantReloadApplied = true;
			}
		}
		else
		{
			if (m_bInstantReloadApplied)
			{
				montage->RateScale = m_flOriginalReloadRate;

				m_bInstantReloadApplied = false;
				m_pLastReloadMontage = nullptr;
			}
		}
	}
}

bool BackupAttrData()
{
	auto* localChar = Unreal::GetLocalCharacter();
	if (!localChar)
		return false;
	
	auto* PlayerAttributeSet = localChar->PlayerAttributeSet;
	if (!PlayerAttributeSet)
		return false;

	g_attrDataBackup.try_emplace(std::hash<std::string>{}(localChar->Name.ToString()), AttrDataBackupEntry_t{
		.m_flHealth = PlayerAttributeSet->HealthMax,
		.m_flStamina = PlayerAttributeSet->Stamina,

		.m_flPrimaryEquippableAmmoInventory = PlayerAttributeSet->PrimaryEquippableAmmoInventory,
		.m_flSecondaryEquippableAmmoInventory = PlayerAttributeSet->SecondaryEquippableAmmoInventory,
		.m_flTertiaryEquippableAmmoInventory = PlayerAttributeSet->TertiaryEquippableAmmoInventory,

		.m_flPrimaryThrowableAmmoInventory = PlayerAttributeSet->PrimaryThrowableAmmoInventory,
		.m_flSecondaryThrowableAmmoInventory = PlayerAttributeSet->SecondaryThrowableAmmoInventory,
		.m_flTertiaryThrowableAmmoInventory = PlayerAttributeSet->TertiaryThrowableAmmoInventory,

		.m_flPrimaryToolAmmoInventory = PlayerAttributeSet->PrimaryToolAmmoInventory,
		.m_flSecondaryToolAmmoInventory = PlayerAttributeSet->SecondaryToolAmmoInventory,
		.m_flTertiaryToolAmmoInventory = PlayerAttributeSet->TertiaryToolAmmoInventory
	});
	return true;
}

AttrDataBackupEntry_t* Player::GetAttrBackupData(SDK::ASBZPlayerCharacter* pLocalChar)
{
	if (!pLocalChar)
		return nullptr;

	size_t hash = std::hash<std::string>{}(pLocalChar->Name.ToString());

	auto itrEntry = g_attrDataBackup.find(hash);
	if (itrEntry == g_attrDataBackup.end())
	{
		return nullptr;
	}

	return &itrEntry->second;
}

bool BackupWeaponData(){ //copied straight from v1 and modified to work with v2
	SDK::UWorld* pGWorld = SDK::UWorld::GetWorld();
	if (!pGWorld)
		return false;

	auto pGameInstance = reinterpret_cast<SDK::USBZGameInstance*>(pGWorld->OwningGameInstance);
	if (!pGameInstance || !pGameInstance->IsA(SDK::USBZGameInstance::StaticClass()))
		return false;

    auto pDatabase = pGameInstance->GlobalItemDatabase;
    if(!pDatabase)
        return false;

    SDK::USBZWeaponDatabase* aDatabases[3]{
        pDatabase->PrimaryWeapons.Get(),
        pDatabase->SecondaryWeapons.Get(),
        pDatabase->OverkillWeapons.Get()
    };

    if(!aDatabases[0] || !aDatabases[1] || !aDatabases[2])
        return false;

    for(int iDatabase = 0; iDatabase < 3; ++iDatabase){
        for(int i = 0; i < aDatabases[iDatabase]->Weapons.Num(); i++){
            if(!aDatabases[iDatabase]->Weapons.IsValidIndex(i))
                return false;

            auto pWeaponData = reinterpret_cast<SDK::USBZRangedWeaponData*>(aDatabases[iDatabase]->Weapons[i]);
            if(!pWeaponData)
                return false;

            if(!pWeaponData->IsA(SDK::USBZRangedWeaponData::StaticClass()))
                continue;

            if(!pWeaponData->SpreadData || !pWeaponData->RecoilData || !pWeaponData->FireData)
                return false;

            g_mapWeaponDataBackup.try_emplace(std::hash<std::string>{}(pWeaponData->Name.ToString().substr(14)), WeaponDataBackupEntry_t{
                .m_flViewSpeedDeflect = pWeaponData->RecoilData->ViewKick.SpeedDeflect,
				.m_flGunKickBackSpeedDeflect = pWeaponData->RecoilData->GunKickBack.SpeedDeflect,
                .m_flGunSpeedDeflect = pWeaponData->RecoilData->GunKickXY.SpeedDeflect,

                .m_flInnerClusterSpreadMultiplier = pWeaponData->SpreadData->InnerClusterSpreadMultiplier,
                .m_flFireSpreadStart = pWeaponData->SpreadData->FireSpreadStart,
                .m_flFireSpreadMinCap = pWeaponData->SpreadData->FireSpreadMinCap,
                .m_flFireSpreadCap = pWeaponData->SpreadData->FireSpreadCap,
                .m_flFireSpreadIncrease = pWeaponData->SpreadData->FireSpreadIncrease,

                .m_iProjectilesPerFiredRound = pWeaponData->FireData->ProjectilesPerFiredRound,
                .m_flRoundsPerMinute = pWeaponData->FireData->RoundsPerMinute,
                .m_eFireMode = pWeaponData->FireData->FireMode
            });
        }
    }
    return true;
}

WeaponDataBackupEntry_t* Player::GetWeaponBackupData(SDK::USBZRangedWeaponData* pWeaponData)
{
    if (!pWeaponData)
        return nullptr;

    auto* pEquippable = pWeaponData->EquippableClass.Get();
    if (!pEquippable)
        return nullptr;

    std::string sName = pEquippable->Name.ToString().substr(16);
    sName.resize(sName.size() - 2);

    size_t hash = std::hash<std::string>{}(sName);

    auto itrEntry = g_mapWeaponDataBackup.find(hash);
    if (itrEntry == g_mapWeaponDataBackup.end())
    {
        return nullptr;
    }

    return &itrEntry->second;
}

std::vector<SDK::USBZRangedWeaponData*> Player::GetCurrentWeaponData(SDK::ASBZPlayerCharacter* pLocalChar)
{
	std::vector<SDK::USBZRangedWeaponData*> weapons;

	// Get weapon from FPCameraAttachment (currently equipped)
	if (pLocalChar->FPCameraAttachment) {
		auto* equippedData = pLocalChar->FPCameraAttachment->EquippedWeaponData;
		if (equippedData && equippedData->IsA(SDK::USBZRangedWeaponData::StaticClass())) {
			weapons.push_back(reinterpret_cast<SDK::USBZRangedWeaponData*>(equippedData));
		}
	}

	// Get weapons from EquippableArray (all carried weapons)
	for (UC::int32 i = 0; i < pLocalChar->EquippableArray.Num(); ++i) {
		if (!pLocalChar->EquippableArray.IsValidIndex(i))
			continue;

		auto* equippable = pLocalChar->EquippableArray[i];
		if (!equippable)
			continue;

		auto* weaponData = equippable->EquippableConfig.EquippableData;
		if (weaponData && weaponData->IsA(SDK::USBZRangedWeaponData::StaticClass())) {
			auto* rangedData = reinterpret_cast<SDK::USBZRangedWeaponData*>(weaponData);
			if (std::find(weapons.begin(), weapons.end(), rangedData) == weapons.end()) {
				weapons.push_back(rangedData);
			}
		}
	}
	return weapons;
}

void Player::noRecoil(bool bEnabled)
{
	SDK::ASBZPlayerCharacter* localChar = Unreal::GetLocalCharacter();
	if (!localChar || !g_bDidBackupWeaponData)
		return;

	for (auto* weaponData : GetCurrentWeaponData(localChar)) {
		if (!weaponData || !weaponData->RecoilData)
			continue;

		if (bEnabled) {
			weaponData->RecoilData->ViewKick.SpeedDeflect = 0.f;
			weaponData->RecoilData->GunKickBack.SpeedDeflect = 0.f;
			weaponData->RecoilData->GunKickXY.SpeedDeflect = 0.f;
		} else {
			auto* backup = GetWeaponBackupData(weaponData);
			if (backup) {
				weaponData->RecoilData->ViewKick.SpeedDeflect = backup->m_flViewSpeedDeflect;
				weaponData->RecoilData->GunKickBack.SpeedDeflect = backup->m_flGunKickBackSpeedDeflect;
				weaponData->RecoilData->GunKickXY.SpeedDeflect = backup->m_flGunSpeedDeflect;
			}
		}
	}
}

void Player::noSpread(bool bEnabled)
{
	SDK::ASBZPlayerCharacter* localChar = Unreal::GetLocalCharacter();
	if (!localChar || !g_bDidBackupWeaponData)
		return;

	for (auto* weaponData : GetCurrentWeaponData(localChar)) {
		if (!weaponData || !weaponData->SpreadData)
			continue;

		if (bEnabled) {
			weaponData->SpreadData->InnerClusterSpreadMultiplier = 
				weaponData->SpreadData->FireSpreadStart = 
				weaponData->SpreadData->FireSpreadMinCap = 
				weaponData->SpreadData->FireSpreadCap = 
				weaponData->SpreadData->FireSpreadIncrease = 0.f;
		} else {
			auto* backup = GetWeaponBackupData(weaponData);
			if (backup) {
				weaponData->SpreadData->InnerClusterSpreadMultiplier = backup->m_flInnerClusterSpreadMultiplier;
				weaponData->SpreadData->FireSpreadStart = backup->m_flFireSpreadStart;
				weaponData->SpreadData->FireSpreadMinCap = backup->m_flFireSpreadMinCap;
				weaponData->SpreadData->FireSpreadCap = backup->m_flFireSpreadCap;
				weaponData->SpreadData->FireSpreadIncrease = backup->m_flFireSpreadIncrease;
			}
		}
	}
}

void Player::fireRate(bool bEnabled)
{
	SDK::ASBZPlayerCharacter* localChar = Unreal::GetLocalCharacter();
	if (!localChar || !g_bDidBackupWeaponData)
		return;

	for (auto* weaponData : GetCurrentWeaponData(localChar)) {
		if (!weaponData || !weaponData->FireData)
			continue;

		auto* backup = GetWeaponBackupData(weaponData);
		if (bEnabled) {
			weaponData->FireData->RoundsPerMinute = backup->m_flRoundsPerMinute * m_pFireRateSlider->GetValue();
			weaponData->FireData->FireMode = SDK::ESBZFireMode::Auto;
		} else {
			if (backup) {
				weaponData->FireData->RoundsPerMinute = backup->m_flRoundsPerMinute;
				weaponData->FireData->FireMode = backup->m_eFireMode;
			}
		}
	}
}

void Player::Run()
{
	if (!g_bDidBackupWeaponData)
	{
		g_bDidBackupWeaponData = BackupWeaponData();
	}

	if (!g_bDidBackupAttrData)
	{
		g_bDidBackupAttrData = BackupAttrData();
	}
	/////////////////Tab 1///////////////////

	// Godmode
	if (m_pGodMode->GetValue())
	{
		if (auto* localChar = Unreal::GetLocalCharacter())
		{
			if (m_pGodModeType->GetSelectedIndex() == 0)
			{
				auto* PlayerAttributeSet = localChar->PlayerAttributeSet;
				if (PlayerAttributeSet)
					PlayerAttributeSet->Health.CurrentValue = PlayerAttributeSet->HealthMax.CurrentValue * 10;
			}
			if (m_pGodModeType->GetSelectedIndex() == 1)
			{
				blockDamage(true);
			}
		}
	}

	//Infinite Stamina
	if (m_pInfStamina->GetValue())
	{
		if (auto* localChar = Unreal::GetLocalCharacter())
		{
			auto* PlayerAttributeSet = localChar->PlayerAttributeSet;
			if (PlayerAttributeSet)
				PlayerAttributeSet->Stamina.CurrentValue = 100.0f;
		}
	}

	//Instant Melee
	if (m_pInstaMelee->GetValue())
	{
		InstantMelee(m_pInstaMelee->GetValue());
	}

	//No Screen Shake
	if (m_pNoScreenshake->GetValue())
	{
	 	auto* camManager = Unreal::GetPlayerCameraManager();
	 	if (!camManager)
	 		return;

	 	camManager->StopAllCameraShakes(m_pNoScreenshake->GetValue()); // Doesn't need to be set false since cameramanager updates back without us asking.
	}

	//No Fall Damage
	if (m_pNoFallDamage->GetValue())
	{
		if (auto* localChar = Unreal::GetLocalCharacter())
		{
			localChar->FallingStartHeight = localChar->K2_GetActorLocation().Z;
		}
	}

	//No Detection
	if (m_pNoDetection->GetValue())
	{
		if (auto* localChar = Unreal::GetLocalCharacter())
		{
			for (UC::int32 i=0; i<localChar->VisualDetectors.Num(); ++i) {
				auto* det = localChar->VisualDetectors[i];
			if (!det) continue;

				for (UC::int32 e=0; e<det->EnemyDetectionValue.Num(); ++e) {
					auto& d = det->EnemyDetectionValue[e];
					d.Target = nullptr;
				}
				det->bMarkAsCriminalOnSearch = false;
				det->bShouldDisplayDetectionBuildup = false;
			}
		}
	}

	/////////////////Tab 2///////////////////

	//Populate the player table with player(s) info here

	/////////////////Tab 3///////////////////

	//Instant Reload
	if (m_pInstaReload->GetValue())
	{
		InstantReload(m_pInstaReload->GetValue());
	}

	//Infinite Ammo
	if (m_pInfAmmo->GetValue())
	{
		if(auto* localChar = Unreal::GetLocalCharacter())
		{
			auto* Att = localChar->PlayerAttributeSet;
			if (Att)
			{
				Att->PrimaryEquippableAmmoInventory.CurrentValue = 999;
				Att->SecondaryEquippableAmmoInventory.CurrentValue = 999;
				Att->TertiaryEquippableAmmoInventory.CurrentValue = 999;

				Att->PrimaryThrowableAmmoInventory.CurrentValue = 999;
				Att->SecondaryThrowableAmmoInventory.CurrentValue = 999;
				Att->TertiaryThrowableAmmoInventory.CurrentValue = 999;

				Att->PrimaryToolAmmoInventory.CurrentValue = 999;
				Att->SecondaryToolAmmoInventory.CurrentValue = 999;
				Att->TertiaryToolAmmoInventory.CurrentValue = 999;
			}
		}
	}

	//No Recoil
	if (m_pNoRecoil->GetValue())
	{
		noRecoil(true);
	}

	//No Spread
	if (m_pNoSpread->GetValue())
	{
		noSpread(true);
	}

	//Fire Rate
	if (m_pFireRate->GetValue())
	{
		fireRate(true);
	}

	FreecamFly::Tick(
		m_pFreecam->GetValue(),
		m_bFreecamFlying,
		m_pFreecamFasterKey->GetValue(),
		m_pFreecamSpeed->GetValue());
}

void Player::Render()
{
	if (m_pFreecamKey->IsCapturing() || m_pFreecamFasterKey->IsCapturing()
		|| m_pFreecamKey->ShouldSkipPoll() || m_pFreecamFasterKey->ShouldSkipPoll())
	{
		m_pFreecamKey->Update();
		m_pFreecamFasterKey->Update();
		return;
	}

	m_pFreecamKey->Update();
	m_pFreecamFasterKey->Update();

	bool bFlying = false;
	if (m_pFreecam->GetValue())
	{
		switch (m_pFreecamKey->GetMode())
		{
		case Hotkey::EHotkeyMode::AlwaysOn:
			bFlying = true;
			break;
		case Hotkey::EHotkeyMode::Hold:
		case Hotkey::EHotkeyMode::HoldOff:
			bFlying = m_pFreecamKey->GetValue();
			break;
		default:
		{
			const bool bKeyDown = ImGui::IsKeyDown(m_pFreecamKey->GetKey());
			if (bKeyDown && !m_bFreecamKeyWasDown)
				m_bFreecamToggled = !m_bFreecamToggled;
			m_bFreecamKeyWasDown = bKeyDown;
			bFlying = m_bFreecamToggled;
			break;
		}
		}
	}
	else
	{
		m_bFreecamToggled = false;
	}

	m_bFreecamFlying = bFlying;
}