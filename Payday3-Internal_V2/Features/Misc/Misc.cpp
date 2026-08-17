#include "pch.h"
#include "Misc.hpp"
#include "GhostMode.hpp"
#include "SilentKill.hpp"
#include "GrabAll.hpp"
#include "GrabAccess.hpp"
#include "CarryBags.hpp"
#include "NoCivPenalty.hpp"
#include "InstaDrill.hpp"
#include "VaultCodes.hpp"
#include "SpawnerTools.hpp"
#include "PresetTeleport.hpp"
#include "HeistUtil.hpp"

void Misc::PollHotkeyToggle(Checkbox* pBox, Hotkey* pKey)
{
	if (!pBox || !pKey)
		return;
	if (pKey->IsCapturing() || pKey->ShouldSkipPoll())
	{
		pKey->Update();
		return;
	}

	pKey->Update();
	switch (pKey->GetMode())
	{
	case Hotkey::EHotkeyMode::AlwaysOn:
		pBox->SetValue(true);
		break;
	case Hotkey::EHotkeyMode::Hold:
	case Hotkey::EHotkeyMode::HoldOff:
		pBox->SetValue(pKey->GetValue());
		break;
	default:
		if (pKey->GetKey() != ImGuiKey_None && ImGui::IsKeyPressed(pKey->GetKey(), false))
			pBox->SetValue(!pBox->GetValue());
		break;
	}
}

void Misc::PollHotkeyPress(Hotkey* pKey, const std::function<void()>& fn)
{
	if (!pKey || !fn)
		return;
	if (pKey->IsCapturing() || pKey->ShouldSkipPoll())
	{
		pKey->Update();
		return;
	}
	pKey->Update();
	if (pKey->GetKey() == ImGuiKey_None)
		return;
	if (ImGui::IsKeyPressed(pKey->GetKey(), false))
		fn();
}

void Misc::DrawVaultFlash()
{
	std::string flash;
	if (!VaultCodes::TryGetFlashText(flash) || flash.empty())
		return;

	ImDrawList* pDrawList = ImGui::GetBackgroundDrawList();
	if (!pDrawList)
		return;

	const ImVec2 screen = ImGui::GetIO().DisplaySize;
	const ImVec2 size = ImGui::CalcTextSize(flash.c_str());
	const ImVec2 pos{ (screen.x - size.x) * 0.5f, screen.y * 0.12f };
	pDrawList->AddText(ImVec2{ pos.x + 2.f, pos.y + 2.f }, IM_COL32(0, 0, 0, 220), flash.c_str());
	pDrawList->AddText(pos, IM_COL32(255, 230, 80, 255), flash.c_str());
}

void Misc::HandleMenu()
{
	static std::once_flag onceflag;
	std::call_once(onceflag, [this]() {
		auto pHeaderGroup = static_cast<HeaderGroup*>(Framework::menu->GetChild("HEADER_GROUP"));
		if (pHeaderGroup)
			pHeaderGroup->AddHeaders(Misc::s_iMiscPageId, { "MISC_TAB1"Hashed, "MISC_TAB2"Hashed, "MISC_TAB3"Hashed });

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

		m_pTab1Left->AddElement(m_pGhost.get());
		m_pTab1Left->AddElement(m_pGhostKey.get());
		m_pTab1Left->AddElement(m_pGhostStatus.get());
		m_pTab1Left->AddElement(m_pSilentBury.get());
		m_pTab1Left->AddElement(m_pSilentBuryKey.get());
		m_pTab1Left->AddElement(m_pSilentBuryStatus.get());
		m_pTab1Left->AddElement(m_pCarryBags.get());
		m_pTab1Left->AddElement(m_pCarryBagsKey.get());
		m_pTab1Left->AddElement(m_pCarryBagsStatus.get());
		m_pTab1Left->AddElement(m_pNoCiv.get());
		m_pTab1Left->AddElement(m_pNoCivKey.get());
		m_pTab1Left->AddElement(m_pNoCivStatus.get());
		m_pTab1Left->AddElement(m_pInstaDrill.get());
		m_pTab1Left->AddElement(m_pInstaDrillKey.get());
		m_pTab1Left->AddElement(m_pInstaDrillStatus.get());

		m_pTab1Right->AddElement(m_pGrabLoot.get());
		m_pTab1Right->AddElement(m_pGrabLootKey.get());
		m_pTab1Right->AddElement(m_pGrabLootStatus.get());
		m_pTab1Right->AddElement(m_pGrabKeys.get());
		m_pTab1Right->AddElement(m_pGrabKeysKey.get());
		m_pTab1Right->AddElement(m_pGrabKeysStatus.get());

		if (m_pGhostKey->GetKey() == ImGuiKey_None)
			m_pGhostKey->SetKey(ImGuiKey_F11);
		m_pGhostKey->SetMode(Hotkey::EHotkeyMode::Toggle);

		if (m_pSilentBuryKey->GetKey() == ImGuiKey_None)
			m_pSilentBuryKey->SetKey(ImGuiKey_KeypadMultiply);
		m_pSilentBuryKey->SetMode(Hotkey::EHotkeyMode::Toggle);

		m_pCarryBagsKey->SetMode(Hotkey::EHotkeyMode::Toggle);
		m_pNoCivKey->SetMode(Hotkey::EHotkeyMode::Toggle);
		m_pInstaDrillKey->SetMode(Hotkey::EHotkeyMode::Toggle);
		m_pGrabLootKey->SetMode(Hotkey::EHotkeyMode::Toggle);
		m_pGrabKeysKey->SetMode(Hotkey::EHotkeyMode::Toggle);

		m_pTab1Group->AddElement(m_pTab1Left.get());
		m_pTab1Group->AddElement(m_pTab1Right.get());

		m_pLoadJson->SetCallback([]() { PresetTeleport::RequestLoadDialog(); });
		m_pPrev->SetCallback([]() { PresetTeleport::PrevSpot(); });
		m_pNext->SetCallback([]() { PresetTeleport::NextSpot(); });
		m_pTpHere->SetCallback([]() { PresetTeleport::TeleportHere(); });
		m_pTpAll->SetCallback([]() { PresetTeleport::TeleportEveryone(); });
		m_pTour->SetCallback([]() { PresetTeleport::StartTourSpots(); });
		m_pStop->SetCallback([]() { PresetTeleport::StopTour(); });

		m_pTourDelay->SetOnValueChangedCallback([](const float, const float flNew) {
			PresetTeleport::SetTourDelay(flNew);
		});

		m_pSpotCombo->SetCallback([]() {
			const int n = PresetTeleport::GetSpotCount();
			if (n <= 0)
			{
				ImAdd::Selectable("No JSON loaded", false);
				return;
			}
			for (int i = 0; i < n; ++i)
			{
				const std::string label = std::to_string(i + 1) + ". " + PresetTeleport::GetSpotName(i);
				const bool selected = i == PresetTeleport::GetSelectedIndex();
				if (ImAdd::Selectable(label.c_str(), selected))
					PresetTeleport::SetSelectedIndex(i);
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
		});

		m_pTpHereKey->SetMode(Hotkey::EHotkeyMode::Toggle);
		m_pTpAllKey->SetMode(Hotkey::EHotkeyMode::Toggle);
		m_pPrevKey->SetMode(Hotkey::EHotkeyMode::Toggle);
		m_pNextKey->SetMode(Hotkey::EHotkeyMode::Toggle);
		m_pTourKey->SetMode(Hotkey::EHotkeyMode::Toggle);

		m_pTab2Left->AddElement(m_pLoadJson.get());
		m_pTab2Left->AddElement(m_pPrev.get());
		m_pTab2Left->AddElement(m_pNext.get());
		m_pTab2Left->AddElement(m_pTpHere.get());
		m_pTab2Left->AddElement(m_pTpAll.get());
		m_pTab2Left->AddElement(m_pTour.get());
		m_pTab2Left->AddElement(m_pStop.get());
		m_pTab2Left->AddElement(m_pTpHereKey.get());
		m_pTab2Left->AddElement(m_pTpAllKey.get());
		m_pTab2Left->AddElement(m_pPrevKey.get());
		m_pTab2Left->AddElement(m_pNextKey.get());
		m_pTab2Left->AddElement(m_pTourKey.get());
		m_pTab2Left->AddElement(m_pTourDelay.get());
		m_pTab2Left->AddElement(m_pSpotCombo.get());
		m_pTab2Left->AddElement(m_pTpStatus.get());

		m_pTab2Group->AddElement(m_pTab2Left.get());

		m_pVaultScan->SetCallback([]() { VaultCodes::RequestScan(); });
		if (m_pVaultScanKey->GetKey() == ImGuiKey_None)
			m_pVaultScanKey->SetKey(ImGuiKey_F10);
		m_pVaultScanKey->SetMode(Hotkey::EHotkeyMode::Toggle);
		m_pKeypadHelper->SetValue(true);

		m_pSpawnMeth->SetCallback([]() { SpawnerTools::RequestMeth(); });
		m_pSpawnVan->SetCallback([]() { SpawnerTools::RequestVan(); });
		m_pSpawnExit->SetCallback([]() { SpawnerTools::RequestGreenExit(); });
		m_pSpawnMoney->SetCallback([]() { SpawnerTools::RequestMoneyScreen(); });

		m_pTab3Left->AddElement(m_pVaultScan.get());
		m_pTab3Left->AddElement(m_pVaultScanKey.get());
		m_pTab3Left->AddElement(m_pVaultStatus.get());
		m_pTab3Left->AddElement(m_pKeypadHelper.get());
		m_pTab3Left->AddElement(m_pKeypadHelperStatus.get());

		m_pTab3Right->AddElement(m_pSpawnMeth.get());
		m_pTab3Right->AddElement(m_pSpawnMethStatus.get());
		m_pTab3Right->AddElement(m_pSpawnVan.get());
		m_pTab3Right->AddElement(m_pSpawnVanStatus.get());
		m_pTab3Right->AddElement(m_pSpawnExit.get());
		m_pTab3Right->AddElement(m_pSpawnExitStatus.get());
		m_pTab3Right->AddElement(m_pSpawnMoney.get());
		m_pTab3Right->AddElement(m_pSpawnMoneyStatus.get());

		m_pTab3Group->AddElement(m_pTab3Left.get());
		m_pTab3Group->AddElement(m_pTab3Right.get());

		m_pTab1Page->AddElement(m_pTab1Group.get());
		m_pTab2Page->AddElement(m_pTab2Group.get());
		m_pTab3Page->AddElement(m_pTab3Group.get());

		Framework::menu->GetChild("HEADER_GROUP")->GetChild("BODY")->AddElement(m_pTab1Page.get());
		Framework::menu->GetChild("HEADER_GROUP")->GetChild("BODY")->AddElement(m_pTab2Page.get());
		Framework::menu->GetChild("HEADER_GROUP")->GetChild("BODY")->AddElement(m_pTab3Page.get());
	});

	m_pGhostStatus->SetName(GhostMode::g_sStatus);
	m_pSilentBuryStatus->SetName(SilentKill::g_sDebugStatus);
	m_pGrabLootStatus->SetName(GrabAll::g_sDebugStatus);
	m_pGrabKeysStatus->SetName(GrabAccess::g_sDebugStatus);
	m_pCarryBagsStatus->SetName(CarryBags::g_sStatus);
	m_pNoCivStatus->SetName(NoCivPenalty::g_sStatus);
	m_pInstaDrillStatus->SetName(InstaDrill::g_sDebugStatus);
	m_pTpStatus->SetName(PresetTeleport::g_sStatus);
	m_pVaultStatus->SetName(VaultCodes::g_sStatus);
	m_pKeypadHelperStatus->SetName(VaultCodes::g_sHelperStatus);
	m_pSpawnMethStatus->SetName(SpawnerTools::g_sStatusMeth);
	m_pSpawnVanStatus->SetName(SpawnerTools::g_sStatusVan);
	m_pSpawnExitStatus->SetName(SpawnerTools::g_sStatusExit);
	m_pSpawnMoneyStatus->SetName(SpawnerTools::g_sStatusMoney);

	if (PresetTeleport::GetSpotCount() > 0)
	{
		const int i = PresetTeleport::GetSelectedIndex();
		m_pSpotCombo->SetPreviewLabel(std::to_string(i + 1) + ". " + PresetTeleport::GetSpotName(i));
	}
	else
	{
		m_pSpotCombo->SetPreviewLabel("No JSON loaded");
	}
}

void Misc::Render()
{
	PollHotkeyToggle(m_pGhost.get(), m_pGhostKey.get());
	PollHotkeyToggle(m_pSilentBury.get(), m_pSilentBuryKey.get());
	PollHotkeyToggle(m_pGrabLoot.get(), m_pGrabLootKey.get());
	PollHotkeyToggle(m_pGrabKeys.get(), m_pGrabKeysKey.get());
	PollHotkeyToggle(m_pCarryBags.get(), m_pCarryBagsKey.get());
	PollHotkeyToggle(m_pNoCiv.get(), m_pNoCivKey.get());
	PollHotkeyToggle(m_pInstaDrill.get(), m_pInstaDrillKey.get());

	PollHotkeyPress(m_pVaultScanKey.get(), []() { VaultCodes::RequestScan(); });
	PollHotkeyPress(m_pTpHereKey.get(), []() { PresetTeleport::TeleportHere(); });
	PollHotkeyPress(m_pTpAllKey.get(), []() { PresetTeleport::TeleportEveryone(); });
	PollHotkeyPress(m_pPrevKey.get(), []() { PresetTeleport::PrevSpot(); });
	PollHotkeyPress(m_pNextKey.get(), []() { PresetTeleport::NextSpot(); });
	PollHotkeyPress(m_pTourKey.get(), []() {
		if (PresetTeleport::IsTouring())
			PresetTeleport::StopTour();
		else
			PresetTeleport::StartTourSpots();
	});

	DrawVaultFlash();
}

void Misc::Run()
{
	PresetTeleport::PollPendingUi();

	SDK::UWorld* pWorld = SDK::UWorld::GetWorld();
	if (!pWorld)
		return;

	SDK::ASBZPlayerController* pCtrl = HeistUtil::GetLocalSBZController();
	SDK::ASBZPlayerCharacter* pLocal = Unreal::GetLocalCharacter();

	GhostMode::Tick(pWorld, pCtrl, pLocal, m_pGhost->GetValue());
	SilentKill::Tick(pWorld, pCtrl, pLocal, m_pSilentBury->GetValue());
	GrabAll::Tick(pWorld, pCtrl, pLocal, m_pGrabLoot->GetValue());
	GrabAccess::Tick(pWorld, pCtrl, pLocal, m_pGrabKeys->GetValue());
	CarryBags::Tick(pWorld, pCtrl, pLocal, m_pCarryBags->GetValue());
	NoCivPenalty::Tick(pWorld, pCtrl, pLocal, m_pNoCiv->GetValue());
	InstaDrill::Tick(pWorld, pCtrl, pLocal, m_pInstaDrill->GetValue());
	VaultCodes::Tick(pWorld, pCtrl, pLocal, m_pKeypadHelper->GetValue());
	SpawnerTools::Tick(pWorld, pCtrl, pLocal);
	PresetTeleport::Tick(pWorld, pCtrl, pLocal);
}
