#include <map>
#include <span>
#include <array>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <cstdlib>
#include <cstdio>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#undef min
#undef max

#include <imgui.h>
#include <imgui_internal.h>
#include "Dumper-7/SDK.hpp"
#include "Config.hpp"
#include "Utils/Logging.hpp"
#include "Features/ESP/ESP.hpp"
#include "Features/Features.hpp"
#include "Features/Misc/FriendlyFire.hpp"
#include "Features/Misc/GrabAll.hpp"
#include "Features/Misc/GrabAccess.hpp"
#include "Features/Misc/InstaDrill.hpp"
#include "Features/Misc/SilentKill.hpp"
#include "Features/Misc/PresetTeleport.hpp"
#include "Features/Misc/GodAmmo.hpp"
#include "Features/Misc/CarryBags.hpp"
#include "Features/Misc/CarryBodies.hpp"
#include "Features/Misc/NoCivPenalty.hpp"
#include "Features/Misc/SpawnerTools.hpp"
#include "Features/Misc/VaultCodes.hpp"
#include "Features/Misc/GhostMode.hpp"
#include "Features/Misc/ThirdPerson.hpp"
#include "Menu.hpp"

namespace
{
    constexpr int g_iConfigVersion = 1;

    std::string Trim(std::string_view svValue)
    {
        size_t iBegin = 0;
        size_t iEnd = svValue.size();

        while (iBegin < iEnd && std::isspace(static_cast<unsigned char>(svValue[iBegin])))
            ++iBegin;

        while (iEnd > iBegin && std::isspace(static_cast<unsigned char>(svValue[iEnd - 1])))
            --iEnd;

        return std::string(svValue.substr(iBegin, iEnd - iBegin));
    }

    bool TryParseBool(const std::string& sValue, bool& bOut)
    {
        std::string sNormalized = sValue;
        std::transform(sNormalized.begin(), sNormalized.end(), sNormalized.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        if (sNormalized == "1" || sNormalized == "true")
        {
            bOut = true;
            return true;
        }

        if (sNormalized == "0" || sNormalized == "false")
        {
            bOut = false;
            return true;
        }

        return false;
    }

    bool TryParseInt(const std::string& sValue, int& iOut)
    {
        try
        {
            size_t iPos = 0;
            const int iParsed = std::stoi(sValue, &iPos);
            if (iPos != sValue.size())
                return false;

            iOut = iParsed;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool TryParseFloat(const std::string& sValue, float& flOut)
    {
        try
        {
            size_t iPos = 0;
            const float flParsed = std::stof(sValue, &iPos);
            if (iPos != sValue.size())
                return false;

            flOut = flParsed;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    std::filesystem::path GetConfigPath()
    {
        char szModulePath[MAX_PATH]{};
        if (GetModuleFileNameA(nullptr, szModulePath, MAX_PATH) == 0)
            return "payday3_internal.cfg";

        std::filesystem::path pathConfigDir = std::filesystem::path(szModulePath).parent_path() / "Configs";
        std::error_code ec{};
        std::filesystem::create_directories(pathConfigDir, ec);

        return pathConfigDir / "payday3_internal.cfg";
    }
}

void MultiSelectInternal(const char* szLabel, std::string& sPreviewText, std::span<std::tuple<const char*, const char*, bool&>> aEntries){
    if(!sPreviewText.size()){
        sPreviewText = "";

        for(const auto& tupleEntry : aEntries){
            if(!std::get<2>(tupleEntry))
                continue;

            if(sPreviewText.size())
                sPreviewText += ", ";
            
            sPreviewText += std::get<1>(tupleEntry);
        }
        
        sPreviewText = std::format("{}###{}", (!sPreviewText.size()) ? "None" : sPreviewText, szLabel);
    }

    if(!ImGui::BeginCombo(szLabel, sPreviewText.c_str()))
        return;

    ImGui::PushItemFlag(ImGuiItemFlags_AutoClosePopups, false);
    
    bool bChanged = false;
    for(const auto& tupleEntry : aEntries)
        bChanged |= ImGui::Selectable(std::get<0>(tupleEntry), &std::get<2>(tupleEntry));

    ImGui::PopItemFlag();
    ImGui::EndCombo();

    if(!bChanged)
        return;

    sPreviewText = "";

    for(const auto& tupleEntry : aEntries){
        if(!std::get<2>(tupleEntry))
            continue;

        if(sPreviewText.size())
            sPreviewText += ", ";
        
        sPreviewText += std::get<1>(tupleEntry);
    }
    
    sPreviewText = std::format("{}###{}", (!sPreviewText.size()) ? "None" : sPreviewText, szLabel);
};

#define MultiSelect(szLabel, aData) { \
    static std::string sMultiSelectPreview{};\
    static auto aMultiSelectData = std::to_array<std::tuple<const char*, const char*, bool&>>aData;\
    MultiSelectInternal(szLabel, sMultiSelectPreview, aMultiSelectData);\
}

void Hotkey(const char* szLabel, Menu::Hotkey_t& bind){
    const auto id = ImGui::GetID(szLabel);
    ImGui::PushID(id);

    ImGui::TextUnformatted(szLabel);

    if(!bind.m_bFixedType){
        ImGui::SameLine();
        static const char* aTypes[] = { "Off", "On", "Hold", "Hold Off", "Toggle" };
        int iItem = static_cast<int>(bind.m_eType);
        ImGui::Combo("", &iItem, aTypes, IM_ARRAYSIZE(aTypes));
        bind.m_eType = static_cast<Menu::Hotkey_t::EType>(iItem);
    }
    
    if(bind.m_eType != Menu::Hotkey_t::EType::AlwaysOff && bind.m_eType != Menu::Hotkey_t::EType::AlwaysOn){
        ImGui::SameLine();
        if(ImGui::GetActiveID() == id){
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonActive));
            ImGui::Button("...");
            ImGui::PopStyleColor();
            ImGui::SetKeyOwner(ImGuiKey_Escape, id);

            ImGui::GetCurrentContext()->ActiveIdAllowOverlap = true;
            ImGui::SetActiveID(id, ImGui::GetCurrentWindow());
            if((!ImGui::IsItemHovered() && ImGui::GetIO().MouseClicked[0]) || bind.SetToPressedKey())
                ImGui::ClearActiveID();
        } else if(ImGui::Button(bind.ToString()))
            ImGui::SetActiveID(id, ImGui::GetCurrentWindow());
    }

    ImGui::PopID();
}

// Compact click-to-bind key next to a checkbox (same ActiveID pattern as Hotkey/"Teleport").
// IMPORTANT: id must NOT match the checkbox label — Checkbox already owns GetID(szLabel).
static void BindKeyButton(const char* szBindId, Menu::Hotkey_t& bind)
{
    const ImGuiID id = ImGui::GetID(szBindId);
    ImGui::PushID(id);
    ImGui::SameLine();
    if (bind.m_eType == Menu::Hotkey_t::EType::AlwaysOff || bind.m_eType == Menu::Hotkey_t::EType::AlwaysOn)
    {
        ImGui::PopID();
        return;
    }

    if (ImGui::GetActiveID() == id)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonActive));
        ImGui::Button("...");
        ImGui::PopStyleColor();
        ImGui::SetKeyOwner(ImGuiKey_Escape, id);
        ImGui::GetCurrentContext()->ActiveIdAllowOverlap = true;
        ImGui::SetActiveID(id, ImGui::GetCurrentWindow());
        if ((!ImGui::IsItemHovered() && ImGui::GetIO().MouseClicked[0]) || bind.SetToPressedKey())
            ImGui::ClearActiveID();
    }
    else if (ImGui::Button(bind.ToString()))
    {
        ImGui::SetActiveID(id, ImGui::GetCurrentWindow());
    }
    ImGui::PopID();
}

static void CheckboxWithHotkey(const char* szLabel, bool* pEnabled, Menu::Hotkey_t& bind)
{
    ImGui::Checkbox(szLabel, pEnabled);
    // Unique ImGui id — "##" hides extra label text, avoids colliding with the checkbox.
    char szBindId[192]{};
    std::snprintf(szBindId, sizeof(szBindId), "##hk_%s", szLabel);
    BindKeyButton(szBindId, bind);
}

static bool ButtonWithHotkey(const char* szLabel, Menu::Hotkey_t& bind)
{
    const bool bClicked = ImGui::Button(szLabel);
    char szBindId[192]{};
    std::snprintf(szBindId, sizeof(szBindId), "##hk_%s", szLabel);
    BindKeyButton(szBindId, bind);
    return bClicked;
}


bool CheatConfig::Save() const{

    const auto pathConfig = GetConfigPath();
    std::ofstream fileConfig(pathConfig, std::ios::trunc);
    if (!fileConfig.is_open() || fileConfig.fail())
    {
        Utils::LogError(std::format("Failed to open config for writing: {}", pathConfig.string()));
        return false;
    }

    auto Write = [&](const char* szKey, const auto& value){
        using T = std::decay_t<decltype(value)>;

        if constexpr(std::is_enum_v<T>){
            fileConfig << szKey << '=' << static_cast<int>(value) << '\n';
        }
        else if constexpr(std::is_integral_v<T> || std::is_floating_point_v<T>){
            fileConfig << szKey << '=' << value << '\n';
        }
        else if constexpr(std::is_same_v<T, bool>){
            fileConfig << szKey << '=' << (value ? 1 : 0) << '\n';
        }
        else if constexpr(std::is_same_v<T, Menu::Hotkey_t>){
            fileConfig << szKey << ".key=" << static_cast<int>(value.m_eKeyCode) << '\n';
            if(value.m_bFixedType)
                fileConfig << szKey << ".type=" << static_cast<int>(value.m_eType) << '\n';
        }
    };

    fileConfig << "# Payday3-Internal config\n";
    Write("config.version", g_iConfigVersion);

    const auto& espConfig = ESP::GetConfig();
    Write("aimbot.enabled", m_aimbot.m_bEnabled);
    Write("aimbot.fov", m_aimbot.m_flAimFOV);
    Write("aimbot.fovCircle", espConfig.bDrawFovCircle);
    Write("aimbot.sorting", static_cast<int>(m_aimbot.m_eSorting));
    Write("aimbot.type", static_cast<int>(m_aimbot.m_eAimType));
    Write("aimbot.smoothing", m_aimbot.m_iSmoothing);
    Write("aimbot.targets.guards", m_aimbot.m_bGuards);
    Write("aimbot.targets.specials", m_aimbot.m_bSpecials);
    Write("aimbot.targets.fbiVan", m_aimbot.m_bFBIVan);
    Write("aimbot.targets.civilians", m_aimbot.m_bCivilians);
    Write("aimbot.throughWalls", m_aimbot.m_bThroughWalls);
    Write("aimbot.disableInStealth", m_aimbot.m_bDisableInStealth);

    Write("misc.keyClientMove", m_misc.m_keyClientMove);
    Write("misc.keyClientMoveTeleport", m_misc.m_keyClientMoveTeleport);
    Write("misc.keyClientMoveFaster", m_misc.m_keyClientMoveFaster);

    Write("misc.clientMove.autoTeleport", m_misc.m_bClientMoveAutoTeleport);
    Write("misc.clientMove.baseSpeed", m_misc.m_flClientMoveBaseSpeed);

    Write("misc.noSpread", m_misc.m_bNoSpread);
    Write("misc.noRecoil", m_misc.m_bNoRecoil);
    Write("misc.noFallDamage", m_misc.m_bNoFallDamage);
    Write("misc.instantInteraction", m_misc.m_bInstantInteraction);
    Write("misc.instantMinigame", m_misc.m_bInstantMinigame);
    Write("misc.instantReload", m_misc.m_bInstantReload);
    Write("misc.instantMelee", m_misc.m_bInstantMelee);
    Write("misc.autoPistol", m_misc.m_bAutoPistol);

    Write("misc.speedBuff", m_misc.m_bSpeedBuff);
    Write("misc.damageBuff", m_misc.m_bDamageBuff);
    Write("misc.armorBuff", m_misc.m_bArmorBuff);

    Write("misc.noCameraShake", m_misc.m_bNoCameraShake);
    Write("misc.noCameraTilt", m_misc.m_bNoCameraTilt);
    Write("misc.cameraFov", m_misc.m_flCameraFOV);

    Write("misc.rapidFire", m_misc.m_iRapidFire);

    Write("misc.moreBullets.enabled", m_misc.m_bMoreBullets);
    Write("misc.moreBullets.count", m_misc.m_iMoreBullets);

    Write("misc.superToss.enabled", m_misc.m_bSuperToss);
    Write("misc.superToss.velocity", m_misc.m_flSuperToss);

    Write("misc.friendlyFire", m_misc.m_bFriendlyFire);
    Write("misc.grabAll", m_misc.m_bGrabAll);
    Write("misc.grabAccess", m_misc.m_bGrabAccess);
    Write("misc.instaDrill", m_misc.m_bInstaDrill);
    Write("misc.silentKillCops", m_misc.m_bSilentKillCops);
    Write("misc.godMode", m_misc.m_bGodMode);
    Write("misc.infiniteAmmo", m_misc.m_bInfiniteAmmo);
    Write("misc.instaKill", m_misc.m_bInstaKill);
    Write("misc.carryMoreBags", m_misc.m_bCarryMoreBags);
    Write("misc.carryMoreBodies", m_misc.m_bCarryMoreBodies);
    Write("misc.noCivPenalty", m_misc.m_bNoCivPenalty);
    Write("misc.ghostMode", m_misc.m_bGhostMode);
    Write("misc.thirdPerson", m_misc.m_bThirdPerson);
    Write("misc.thirdPersonSide", m_misc.m_iThirdPersonSide);

    Write("misc.keyGodMode", m_misc.m_keyGodMode);
    Write("misc.keyInfiniteAmmo", m_misc.m_keyInfiniteAmmo);
    Write("misc.keyInstaKill", m_misc.m_keyInstaKill);
    Write("misc.keyCarryMoreBags", m_misc.m_keyCarryMoreBags);
    Write("misc.keyCarryMoreBodies", m_misc.m_keyCarryMoreBodies);
    Write("misc.keyNoCivPenalty", m_misc.m_keyNoCivPenalty);
    Write("misc.keyFriendlyFire", m_misc.m_keyFriendlyFire);
    Write("misc.keyGrabAll", m_misc.m_keyGrabAll);
    Write("misc.keyGrabAccess", m_misc.m_keyGrabAccess);
    Write("misc.keyInstaDrill", m_misc.m_keyInstaDrill);
    Write("misc.keySilentKillCops", m_misc.m_keySilentKillCops);
    Write("misc.keySpawnMeth", m_misc.m_keySpawnMeth);
    Write("misc.keySpawnVan", m_misc.m_keySpawnVan);
    Write("misc.keySpawnGreenExit", m_misc.m_keySpawnGreenExit);
    Write("misc.keySpawnMoney", m_misc.m_keySpawnMoney);
    Write("misc.keyVaultCodes", m_misc.m_keyVaultCodes);
    Write("misc.keyGhostMode", m_misc.m_keyGhostMode);
    Write("misc.keyThirdPerson", m_misc.m_keyThirdPerson);
    Write("misc.keyThirdPersonLeft", m_misc.m_keyThirdPersonLeft);
    Write("misc.keyThirdPersonRight", m_misc.m_keyThirdPersonRight);

    Write("esp.enabled", espConfig.bESP);
    Write("esp.colors.box", static_cast<uint32_t>(espConfig.m_colors.m_colBox));
    Write("esp.colors.health", static_cast<uint32_t>(espConfig.m_colors.m_colHealth));
    Write("esp.colors.armor", static_cast<uint32_t>(espConfig.m_colors.m_colArmor));
    Write("esp.colors.skeleton", static_cast<uint32_t>(espConfig.m_colors.m_colSkeleton));
    Write("esp.colors.highlight", static_cast<uint32_t>(espConfig.m_colors.m_colHighlight));
    Write("esp.colors.keyItems", static_cast<uint32_t>(espConfig.m_colors.m_colKeyItems));
    Write("esp.colors.money", static_cast<uint32_t>(espConfig.m_colors.m_colMoney));
    Write("esp.colors.chem", static_cast<uint32_t>(espConfig.m_colors.m_colChem));
    Write("esp.colors.detection", static_cast<uint32_t>(espConfig.m_colors.m_colDetection));
    Write("esp.colors.bagZone", static_cast<uint32_t>(espConfig.m_colors.m_colBagZone));
    Write("esp.colors.suspicious", static_cast<uint32_t>(espConfig.m_colors.m_colSuspicious));
    Write("esp.pagerHud", espConfig.bPagerHud);
    Write("esp.bagZones", espConfig.bBagZones);
    Write("esp.normal.box", espConfig.m_stNormalEnemies.m_bBox);
    Write("esp.normal.health", espConfig.m_stNormalEnemies.m_bHealth);
    Write("esp.normal.armor", espConfig.m_stNormalEnemies.m_bArmor);
    Write("esp.normal.name", espConfig.m_stNormalEnemies.m_bName);
    Write("esp.normal.flags", espConfig.m_stNormalEnemies.m_bFlags);
    Write("esp.normal.skeleton", espConfig.m_stNormalEnemies.m_bSkeleton);
    Write("esp.normal.outline", espConfig.m_stNormalEnemies.m_bOutline);

    Write("esp.special.box", espConfig.m_stSpecialEnemies.m_bBox);
    Write("esp.special.health", espConfig.m_stSpecialEnemies.m_bHealth);
    Write("esp.special.armor", espConfig.m_stSpecialEnemies.m_bArmor);
    Write("esp.special.name", espConfig.m_stSpecialEnemies.m_bName);
    Write("esp.special.flags", espConfig.m_stSpecialEnemies.m_bFlags);
    Write("esp.special.skeleton", espConfig.m_stSpecialEnemies.m_bSkeleton);
    Write("esp.special.outline", espConfig.m_stSpecialEnemies.m_bOutline);

    Write("esp.civilians.box", espConfig.m_stCivilians.m_bBox);
    Write("esp.civilians.flags", espConfig.m_stCivilians.m_bFlags);
    Write("esp.civilians.skeleton", espConfig.m_stCivilians.m_bSkeleton);
    Write("esp.civilians.outline", espConfig.m_stCivilians.m_bOutline);
    Write("esp.civilians.onlyWhenSpecial", espConfig.m_stCivilians.m_bOnlyWhenSpecial);

    Write("esp.debug.skeleton", espConfig.bDebugSkeleton);
    Write("esp.debug.drawBoneIndices", espConfig.bDebugDrawBoneIndices);
    Write("esp.debug.drawBoneNames", espConfig.bDebugDrawBoneNames);
    Write("esp.debug.skeletonDrawBoneIndices", espConfig.bDebugSkeletonDrawBoneIndices);
    Write("esp.debug.skeletonDrawBoneNames", espConfig.bDebugSkeletonDrawBoneNames);
    Write("esp.debug.esp", espConfig.bDebugESP);

    LootESP::Config& lootespConfig = LootESP::GetConfig();
    Write("lootesp.enabled", lootespConfig.bLootESP);
    Write("lootesp.outline", lootespConfig.bOutline);
    Write("lootesp.strongglow", lootespConfig.bStrongGlow);
    Write("lootesp.labels", lootespConfig.bLabels);

    if (fileConfig.fail())
    {
        Utils::LogError(std::format("Failed to write config: {}", pathConfig.string()));
        return false;
    }

    Utils::LogDebug(std::format("Config saved: {}", pathConfig.string()));
    return true;
}

bool CheatConfig::Load()
{
    const auto pathConfig = GetConfigPath();
    std::ifstream fileConfig(pathConfig);
    if (!fileConfig.is_open() || fileConfig.fail())
    {
        Utils::LogDebug(std::format("Config file not found, using defaults: {}", pathConfig.string()));
        return false;
    }

    std::unordered_map<std::string, std::string> mapConfigValues{};
    std::string sLine{};

    while (std::getline(fileConfig, sLine))
    {
        const std::string sTrimmed = Trim(sLine);
        if (sTrimmed.empty() || sTrimmed[0] == '#' || sTrimmed[0] == ';')
            continue;

        const size_t iEquals = sTrimmed.find('=');
        if (iEquals == std::string::npos)
            continue;

        const std::string sKey = Trim(std::string_view(sTrimmed).substr(0, iEquals));
        const std::string sValue = Trim(std::string_view(sTrimmed).substr(iEquals + 1));
        if (!sKey.empty())
            mapConfigValues[sKey] = sValue;
    }

    auto Read = [&](const char* szKey, auto& value) -> bool {
        using T = std::decay_t<decltype(value)>;
        if constexpr(std::is_same_v<T, Menu::Hotkey_t>){
            const auto itr1 = mapConfigValues.find((std::string{szKey} + ".key").c_str());
            const auto itr2 = mapConfigValues.find((std::string{szKey} + ".type").c_str());

            bool bFailed = true;
            if(itr1 != mapConfigValues.end()){
                int iParsed{};
                if(TryParseInt(itr1->second, iParsed)){
                    bFailed = false;
                    value.m_eKeyCode = static_cast<ImGuiKey>(iParsed);
                    if(iParsed < static_cast<int>(ImGuiKey_NamedKey_BEGIN) || iParsed > static_cast<int>(ImGuiKey_NamedKey_END))
                        value.m_eKeyCode = ImGuiKey_None;

                }
            }

            if(value.m_bFixedType)
                return bFailed;

            if(itr2 == mapConfigValues.end())
                return false;

            int iParsed{};
            if(!TryParseInt(itr2->second, iParsed))
                return false;

            value.m_eType = static_cast<Menu::Hotkey_t::EType>(iParsed);
            if(iParsed < static_cast<int>(Menu::Hotkey_t::EType::AlwaysOff) || iParsed > static_cast<int>(Menu::Hotkey_t::EType::Toggle))
                value.m_eType = Menu::Hotkey_t::EType::Hold;
            
            return bFailed;
        }

        const auto itr = mapConfigValues.find(szKey);
        if(itr == mapConfigValues.end())
            return false;
        
        if constexpr(std::is_same_v<T, bool>){
            bool bParsed{};
            if(!TryParseBool(itr->second, bParsed))
                return false;

            value = bParsed;
            return true;
        }
        else if constexpr(std::is_integral_v<T>){
            int iParsed{};
            if(!TryParseInt(itr->second, iParsed))
                return false;

            value = iParsed;
            return true;
        }
        else if constexpr(std::is_floating_point_v<T>){
            float flParsed{};
            if(!TryParseFloat(itr->second, flParsed))
                return false;

            value = flParsed;
            return true;
        }
    };

    int iVersion{};
    auto& espConfig = ESP::GetConfig();

    if (Read("config.version", iVersion) && iVersion != g_iConfigVersion)
        Utils::LogDebug(std::format("Loading config version {} with parser version {}", iVersion, g_iConfigVersion));

    Read("aimbot.enabled", m_aimbot.m_bEnabled);
    if (Read("aimbot.fov", m_aimbot.m_flAimFOV))
        m_aimbot.m_flAimFOV = std::clamp(m_aimbot.m_flAimFOV, 0.0f, 180.0f);

    int iSorting{};

    Read("aimbot.fovCircle", espConfig.bDrawFovCircle);
    if (Read("aimbot.sorting", iSorting)
        && iSorting >= static_cast<int>(Aimbot_t::ESorting::Smart)
        && iSorting <= static_cast<int>(Aimbot_t::ESorting::Threat))
    {
        m_aimbot.m_eSorting = static_cast<Aimbot_t::ESorting>(iSorting);
    }
    int iAimType{};
    if (Read("aimbot.type", iAimType)
        && iAimType >= static_cast<int>(Aimbot_t::EAimType::Silent)
        && iAimType <= static_cast<int>(Aimbot_t::EAimType::Snapping))
    {
        m_aimbot.m_eAimType = static_cast<Aimbot_t::EAimType>(iAimType);
    }
    if (Read("aimbot.smoothing", m_aimbot.m_iSmoothing))
        m_aimbot.m_iSmoothing = std::clamp(m_aimbot.m_iSmoothing, 0, 100);

    Read("aimbot.targets.guards", m_aimbot.m_bGuards);
    Read("aimbot.targets.specials", m_aimbot.m_bSpecials);
    Read("aimbot.targets.fbiVan", m_aimbot.m_bFBIVan);
    Read("aimbot.targets.civilians", m_aimbot.m_bCivilians);
    Read("aimbot.throughWalls", m_aimbot.m_bThroughWalls);
    Read("aimbot.disableInStealth", m_aimbot.m_bDisableInStealth);

    Read("misc.keyClientMove", m_misc.m_keyClientMove);
    Read("misc.keyClientMoveTeleport", m_misc.m_keyClientMoveTeleport);
    Read("misc.keyClientMoveFaster", m_misc.m_keyClientMoveFaster);

    Read("misc.clientMove.autoTeleport", m_misc.m_bClientMoveAutoTeleport);
    if (Read("misc.clientMove.baseSpeed", m_misc.m_flClientMoveBaseSpeed))
        m_misc.m_flClientMoveBaseSpeed = std::max(0.0f, m_misc.m_flClientMoveBaseSpeed);

    Read("misc.noSpread", m_misc.m_bNoSpread);
    Read("misc.noRecoil", m_misc.m_bNoRecoil);
    Read("misc.noFallDamage", m_misc.m_bNoFallDamage);
    Read("misc.instantInteraction", m_misc.m_bInstantInteraction);
    Read("misc.instantMinigame", m_misc.m_bInstantMinigame);
    Read("misc.instantReload", m_misc.m_bInstantReload);
    Read("misc.instantMelee", m_misc.m_bInstantMelee);
    Read("misc.autoPistol", m_misc.m_bAutoPistol);

    Read("misc.speedBuff", m_misc.m_bSpeedBuff);
    Read("misc.damageBuff", m_misc.m_bDamageBuff);
    Read("misc.armorBuff", m_misc.m_bArmorBuff);

    Read("misc.noCameraShake", m_misc.m_bNoCameraShake);
    Read("misc.noCameraTilt", m_misc.m_bNoCameraTilt);
    Read("misc.cameraFov", m_misc.m_flCameraFOV);

    int iRapidFire{};
    if (Read("misc.rapidFire", iRapidFire))
        m_misc.m_iRapidFire = std::clamp(iRapidFire, 0, 2);

    Read("misc.moreBullets.enabled", m_misc.m_bMoreBullets);
    int iMoreBullets{};
    if (Read("misc.moreBullets.count", iMoreBullets))
        m_misc.m_iMoreBullets = std::max(1, iMoreBullets);

    Read("misc.superToss.enabled", m_misc.m_bSuperToss);
    if (Read("misc.superToss.velocity", m_misc.m_flSuperToss))
        m_misc.m_flSuperToss = std::max(0.0f, m_misc.m_flSuperToss);

    // Always start OFF so a bad session can't auto-enable crashy path.
    m_misc.m_bFriendlyFire = false;
    m_misc.m_bGrabAll = false;
    m_misc.m_bGrabAccess = false;
    m_misc.m_bInstaDrill = false;
    m_misc.m_bSilentKillCops = false;
    m_misc.m_bGodMode = false;
    m_misc.m_bInfiniteAmmo = false;
    m_misc.m_bInstaKill = false;
    m_misc.m_bCarryMoreBags = false;
    m_misc.m_bCarryMoreBodies = false;
    m_misc.m_bNoCivPenalty = false;
    m_misc.m_bGhostMode = false;
    m_misc.m_bThirdPerson = false;
    if (Read("misc.thirdPersonSide", m_misc.m_iThirdPersonSide))
        m_misc.m_iThirdPersonSide = std::clamp(m_misc.m_iThirdPersonSide, -1, 1);
    else
        m_misc.m_iThirdPersonSide = 0;

    Read("esp.enabled", espConfig.bESP);
    {
        auto ReadColor = [&](const char* szKey, ImU32& col)
        {
            const auto itr = mapConfigValues.find(szKey);
            if (itr == mapConfigValues.end())
                return;
            try
            {
                col = static_cast<ImU32>(std::stoul(itr->second));
            }
            catch (...)
            {
            }
        };
        ReadColor("esp.colors.box", espConfig.m_colors.m_colBox);
        ReadColor("esp.colors.health", espConfig.m_colors.m_colHealth);
        ReadColor("esp.colors.armor", espConfig.m_colors.m_colArmor);
        ReadColor("esp.colors.skeleton", espConfig.m_colors.m_colSkeleton);
        ReadColor("esp.colors.highlight", espConfig.m_colors.m_colHighlight);
        ReadColor("esp.colors.keyItems", espConfig.m_colors.m_colKeyItems);
        ReadColor("esp.colors.money", espConfig.m_colors.m_colMoney);
        ReadColor("esp.colors.chem", espConfig.m_colors.m_colChem);
        ReadColor("esp.colors.detection", espConfig.m_colors.m_colDetection);
        ReadColor("esp.colors.bagZone", espConfig.m_colors.m_colBagZone);
        ReadColor("esp.colors.suspicious", espConfig.m_colors.m_colSuspicious);
    }

    Read("esp.pagerHud", espConfig.bPagerHud);
    Read("esp.bagZones", espConfig.bBagZones);

    Read("esp.normal.box", espConfig.m_stNormalEnemies.m_bBox);
    Read("esp.normal.health", espConfig.m_stNormalEnemies.m_bHealth);
    Read("esp.normal.armor", espConfig.m_stNormalEnemies.m_bArmor);
    Read("esp.normal.name", espConfig.m_stNormalEnemies.m_bName);
    Read("esp.normal.flags", espConfig.m_stNormalEnemies.m_bFlags);
    Read("esp.normal.skeleton", espConfig.m_stNormalEnemies.m_bSkeleton);
    Read("esp.normal.outline", espConfig.m_stNormalEnemies.m_bOutline);

    Read("esp.special.box", espConfig.m_stSpecialEnemies.m_bBox);
    Read("esp.special.health", espConfig.m_stSpecialEnemies.m_bHealth);
    Read("esp.special.armor", espConfig.m_stSpecialEnemies.m_bArmor);
    Read("esp.special.name", espConfig.m_stSpecialEnemies.m_bName);
    Read("esp.special.flags", espConfig.m_stSpecialEnemies.m_bFlags);
    Read("esp.special.skeleton", espConfig.m_stSpecialEnemies.m_bSkeleton);
    Read("esp.special.outline", espConfig.m_stSpecialEnemies.m_bOutline);

    Read("esp.civilians.box", espConfig.m_stCivilians.m_bBox);
    Read("esp.civilians.flags", espConfig.m_stCivilians.m_bFlags);
    Read("esp.civilians.skeleton", espConfig.m_stCivilians.m_bSkeleton);
    Read("esp.civilians.outline", espConfig.m_stCivilians.m_bOutline);
    Read("esp.civilians.onlyWhenSpecial", espConfig.m_stCivilians.m_bOnlyWhenSpecial);

    Read("esp.debug.skeleton", espConfig.bDebugSkeleton);
    Read("esp.debug.drawBoneIndices", espConfig.bDebugDrawBoneIndices);
    Read("esp.debug.drawBoneNames", espConfig.bDebugDrawBoneNames);
    Read("esp.debug.skeletonDrawBoneIndices", espConfig.bDebugSkeletonDrawBoneIndices);
    Read("esp.debug.skeletonDrawBoneNames", espConfig.bDebugSkeletonDrawBoneNames);
    Read("esp.debug.esp", espConfig.bDebugESP);

    auto ResetKey = [](Menu::Hotkey_t& k)
    {
        k.m_bActive = false;
        k.m_bPressedThisFrame = false;
    };
    ResetKey(m_misc.m_keyClientMove);
    ResetKey(m_misc.m_keyClientMoveTeleport);
    ResetKey(m_misc.m_keyClientMoveFaster);
    ResetKey(m_misc.m_keyGodMode);
    ResetKey(m_misc.m_keyInfiniteAmmo);
    ResetKey(m_misc.m_keyInstaKill);
    ResetKey(m_misc.m_keyCarryMoreBags);
    ResetKey(m_misc.m_keyCarryMoreBodies);
    ResetKey(m_misc.m_keyNoCivPenalty);
    ResetKey(m_misc.m_keyFriendlyFire);
    ResetKey(m_misc.m_keyGrabAll);
    ResetKey(m_misc.m_keyGrabAccess);
    ResetKey(m_misc.m_keyInstaDrill);
    ResetKey(m_misc.m_keySilentKillCops);
    ResetKey(m_misc.m_keySpawnMeth);
    ResetKey(m_misc.m_keySpawnVan);
    ResetKey(m_misc.m_keySpawnGreenExit);
    ResetKey(m_misc.m_keySpawnMoney);
    ResetKey(m_misc.m_keyVaultCodes);
    ResetKey(m_misc.m_keyGhostMode);
    ResetKey(m_misc.m_keyThirdPerson);
    ResetKey(m_misc.m_keyThirdPersonLeft);
    ResetKey(m_misc.m_keyThirdPersonRight);

    Read("misc.keyGodMode", m_misc.m_keyGodMode);
    Read("misc.keyInfiniteAmmo", m_misc.m_keyInfiniteAmmo);
    Read("misc.keyInstaKill", m_misc.m_keyInstaKill);
    Read("misc.keyCarryMoreBags", m_misc.m_keyCarryMoreBags);
    Read("misc.keyCarryMoreBodies", m_misc.m_keyCarryMoreBodies);
    Read("misc.keyNoCivPenalty", m_misc.m_keyNoCivPenalty);
    Read("misc.keyFriendlyFire", m_misc.m_keyFriendlyFire);
    Read("misc.keyGrabAll", m_misc.m_keyGrabAll);
    Read("misc.keyGrabAccess", m_misc.m_keyGrabAccess);
    Read("misc.keyInstaDrill", m_misc.m_keyInstaDrill);
    Read("misc.keySilentKillCops", m_misc.m_keySilentKillCops);
    Read("misc.keySpawnMeth", m_misc.m_keySpawnMeth);
    Read("misc.keySpawnVan", m_misc.m_keySpawnVan);
    Read("misc.keySpawnGreenExit", m_misc.m_keySpawnGreenExit);
    Read("misc.keySpawnMoney", m_misc.m_keySpawnMoney);
    Read("misc.keyVaultCodes", m_misc.m_keyVaultCodes);
    Read("misc.keyGhostMode", m_misc.m_keyGhostMode);
    Read("misc.keyThirdPerson", m_misc.m_keyThirdPerson);
    Read("misc.keyThirdPersonLeft", m_misc.m_keyThirdPersonLeft);
    Read("misc.keyThirdPersonRight", m_misc.m_keyThirdPersonRight);

    auto& lootespConfig = LootESP::GetConfig();
    Read("lootesp.enabled", lootespConfig.bLootESP);
    lootespConfig.bOutline = false;
    lootespConfig.bStrongGlow = false;
    lootespConfig.bLabels = false;

    Utils::LogDebug(std::format("Config loaded: {}", pathConfig.string()));
    return true;
}


static void ColorEditU32(const char* szLabel, ImU32* pCol)
{
    ImVec4 v = ImGui::ColorConvertU32ToFloat4(*pCol);
    if (ImGui::ColorEdit4(szLabel, &v.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
        *pCol = ImGui::ColorConvertFloat4ToU32(v);
}

void CheatConfig::Aimbot_t::Draw(){
    ImGui::Checkbox("Enabled", &m_bEnabled);
    if(!m_bEnabled)
        return;

    static const char* aAimTypeItems[]{ "Silent", "Snapping" };
    ImGui::Combo("Aimbot Type", reinterpret_cast<int*>(&m_eAimType), aAimTypeItems, IM_ARRAYSIZE(aAimTypeItems));
    ImGui::TextDisabled(m_eAimType == EAimType::Silent
        ? "Silent: bullets track; camera stays put"
        : "Snapping: camera aims at target (smoothed)");

    ImGui::SliderFloat("Aim FOV", &m_flAimFOV, 0.f, 180.f, "%0.0f");
    if (m_eAimType == EAimType::Snapping)
        ImGui::SliderInt("Smoothing", &m_iSmoothing, 0, 100);

    ImGui::Checkbox("Draw FOV Circle", &ESP::GetConfig().bDrawFovCircle);

    static const char* aSortingItems[]{ "Smart", "FOV", "Threat" };
    ImGui::Combo("Sorting Method", reinterpret_cast<int*>(&m_eSorting), aSortingItems, IM_ARRAYSIZE(aSortingItems));

    MultiSelect("Targets", ({
        {"Guards", "Guards", m_bGuards},
        {"Specials", "Specials", m_bSpecials},
        {"FBI Van", "Van", m_bFBIVan},
        {"Civilians", "Civs", m_bCivilians}
    }));

    ImGui::Checkbox("Aim Through Walls", &m_bThroughWalls);
    ImGui::Checkbox("Loud Only", &m_bDisableInStealth);
}

void CheatConfig::Visuals_t::Draw(){
    auto& espConfig = ESP::GetConfig();
    ImGui::Checkbox("Enable ESP", &espConfig.bESP);
    if (espConfig.bESP) {
        ImGui::Indent();

        MultiSelect("Normal Enemies", ({
            {"Box", "B", espConfig.m_stNormalEnemies.m_bBox},
            {"Health", "H", espConfig.m_stNormalEnemies.m_bHealth},
            {"Armor", "A", espConfig.m_stNormalEnemies.m_bArmor},
            {"Name", "N", espConfig.m_stNormalEnemies.m_bName},
            {"Flags", "F", espConfig.m_stNormalEnemies.m_bFlags},
            {"Skeleton", "S", espConfig.m_stNormalEnemies.m_bSkeleton},
            {"Outline", "O", espConfig.m_stNormalEnemies.m_bOutline}
        }));

        MultiSelect("Special Enemies", ({
            {"Box", "B", espConfig.m_stSpecialEnemies.m_bBox},
            {"Health", "H", espConfig.m_stSpecialEnemies.m_bHealth},
            {"Armor", "A", espConfig.m_stSpecialEnemies.m_bArmor},
            {"Name", "N", espConfig.m_stSpecialEnemies.m_bName},
            {"Flags", "F", espConfig.m_stSpecialEnemies.m_bFlags},
            {"Skeleton", "S", espConfig.m_stSpecialEnemies.m_bSkeleton},
            {"Outline", "O", espConfig.m_stSpecialEnemies.m_bOutline}
        }));

        MultiSelect("Civilians", ({
            {"Box", "B", espConfig.m_stCivilians.m_bBox},
            {"Flags", "F", espConfig.m_stCivilians.m_bFlags},
            {"Skeleton", "S", espConfig.m_stCivilians.m_bSkeleton},
            {"Outline", "O", espConfig.m_stCivilians.m_bOutline},
            {"Only When Special Enemy is Visible", "OnlyWhenSpecial", espConfig.m_stCivilians.m_bOnlyWhenSpecial}
        }));

        ImGui::Separator();
        ImGui::TextDisabled("ESP Colors");
        auto& cols = espConfig.m_colors;
        ColorEditU32("Box##espcol", &cols.m_colBox);
        ImGui::SameLine();
        ColorEditU32("Health##espcol", &cols.m_colHealth);
        ImGui::SameLine();
        ColorEditU32("Armor##espcol", &cols.m_colArmor);
        ColorEditU32("Skeleton##espcol", &cols.m_colSkeleton);
        ImGui::SameLine();
        ColorEditU32("Highlight##espcol", &cols.m_colHighlight);
        ImGui::TextDisabled("Highlight = corner brackets when Outline is on");

#ifdef _DEBUG
        ImGui::Checkbox("Debug Draw Bone Indices", &espConfig.bDebugDrawBoneIndices);
        ImGui::Checkbox("Debug Draw Bone Names Instead of Indices", &espConfig.bDebugDrawBoneNames);
#endif
        
#ifdef _DEBUG
        ImGui::Checkbox("Debug Skeleton", &espConfig.bDebugSkeleton);
        if (espConfig.bDebugSkeleton) {
            ImGui::Indent();
            ImGui::Checkbox("Debug Skeleton Draw Bone Indices", &espConfig.bDebugSkeletonDrawBoneIndices);
            ImGui::Checkbox("Debug Skeleton Draw Bone Names Instead of Indices", &espConfig.bDebugSkeletonDrawBoneNames);
            ImGui::Unindent();
        }
#endif
        ImGui::Unindent();
    }

    auto& lootespConfig = LootESP::GetConfig();
    ImGui::Checkbox("Loot ESP", &lootespConfig.bLootESP);
    if (lootespConfig.bLootESP) {
        ImGui::TextDisabled("Through-wall loot glow (V2 outlines). Nested ImGui extras optional.");
        ImGui::Indent();
        ImGui::Checkbox("Loot Outline", &lootespConfig.bOutline);
        ImGui::SameLine();
        ImGui::Checkbox("Strong Glow (F4+)", &lootespConfig.bStrongGlow);
        ImGui::SameLine();
        ImGui::Checkbox("Labels", &lootespConfig.bLabels);
        ImGui::Text("Keys / tools");
        ImGui::SameLine();
        ColorEditU32("##keyitemscol", &ESP::GetConfig().m_colors.m_colKeyItems);
        ImGui::Text("Money / bags");
        ImGui::SameLine();
        ColorEditU32("##moneycol", &ESP::GetConfig().m_colors.m_colMoney);
        ImGui::Text("Chem");
        ImGui::SameLine();
        ColorEditU32("##chemcol", &ESP::GetConfig().m_colors.m_colChem);
        ImGui::Unindent();
    }

#ifdef _DEBUG
    ImGui::Checkbox("Debug ESP (Show Class Names)", &espConfig.bDebugESP);
#endif
}

void CheatConfig::Stealth_t::Draw()
{
    auto& misc = CheatConfig::Get().m_misc;
    auto& espConfig = ESP::GetConfig();

    ImGui::TextDisabled("Actions");
    CheckboxWithHotkey("Ghost Mode (cams + guards ignore you)", &misc.m_bGhostMode, misc.m_keyGhostMode);
    if (misc.m_bGhostMode)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.85f, 1.f, 1.f));
        ImGui::TextWrapped("%s", Cheat::GhostMode::g_sStatus.c_str());
        ImGui::PopStyleColor();
        ImGui::TextDisabled("Invisible / inaudible + AI perception off + camera sight zeroed. Default F11.");
    }

    if (ButtonWithHotkey("Vault Codes", misc.m_keyVaultCodes))
        Cheat::VaultCodes::RequestScan();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.95f, 0.55f, 1.f));
    ImGui::TextWrapped("%s", Cheat::VaultCodes::g_sStatus.c_str());
    ImGui::PopStyleColor();
    ImGui::TextDisabled("Scans keypads + code notes. Flash on screen ~4s. Default F10.");

    ImGui::Separator();
    ImGui::TextDisabled("Stealth HUD / ESP");
    ImGui::Checkbox("Pager HUD", &espConfig.bPagerHud);
    ImGui::Checkbox("Bag Drop Zones", &espConfig.bBagZones);
    if (espConfig.bBagZones) {
        ImGui::SameLine();
        ColorEditU32("##bagzonecol", &espConfig.m_colors.m_colBagZone);
        ImGui::TextDisabled("Secure / van / escape volumes — count vs target");
    }
}




void CheatConfig::Misc_t::UpdateFeatureHotkeys()
{
    // Don't toggle features while a "..." key-bind button is listening.
    if (ImGui::GetActiveID() != 0 && ImGui::GetCurrentContext()->ActiveIdAllowOverlap)
        return;

    auto PollToggle = [](Menu::Hotkey_t& key, bool& bFlag)
    {
        key.UpdateState();
        if (key.Pressed() && key.m_eKeyCode != ImGuiKey_None)
            bFlag = !bFlag;
    };

    PollToggle(m_keyGodMode, m_bGodMode);
    PollToggle(m_keyInfiniteAmmo, m_bInfiniteAmmo);
    PollToggle(m_keyInstaKill, m_bInstaKill);
    PollToggle(m_keyCarryMoreBags, m_bCarryMoreBags);
    PollToggle(m_keyCarryMoreBodies, m_bCarryMoreBodies);
    PollToggle(m_keyNoCivPenalty, m_bNoCivPenalty);
    PollToggle(m_keyFriendlyFire, m_bFriendlyFire);
    PollToggle(m_keyGrabAll, m_bGrabAll);
    PollToggle(m_keyGrabAccess, m_bGrabAccess);
    PollToggle(m_keyInstaDrill, m_bInstaDrill);
    PollToggle(m_keySilentKillCops, m_bSilentKillCops);
    PollToggle(m_keyGhostMode, m_bGhostMode);
    PollToggle(m_keyThirdPerson, m_bThirdPerson);

    auto PollOneShot = [](Menu::Hotkey_t& key, auto&& fn)
    {
        key.UpdateState();
        if (key.Pressed() && key.m_eKeyCode != ImGuiKey_None)
            fn();
    };
    PollOneShot(m_keyVaultCodes, [] { Cheat::VaultCodes::RequestScan(); });
    PollOneShot(m_keySpawnMeth, [] { Cheat::SpawnerTools::RequestMeth(); });
    PollOneShot(m_keySpawnVan, [] { Cheat::SpawnerTools::RequestVan(); });
    PollOneShot(m_keySpawnGreenExit, [] { Cheat::SpawnerTools::RequestGreenExit(); });
    PollOneShot(m_keySpawnMoney, [] { Cheat::SpawnerTools::RequestMoneyScreen(); });
    PollOneShot(m_keyThirdPersonLeft, [this]
    {
        if (m_bThirdPerson)
            m_iThirdPersonSide = -1;
    });
    PollOneShot(m_keyThirdPersonRight, [this]
    {
        if (m_bThirdPerson)
            m_iThirdPersonSide = 1;
    });
}

void CheatConfig::Misc_t::Draw(){
    Hotkey("Client Move", m_keyClientMove);
    if(m_keyClientMove.m_eType != Menu::Hotkey_t::EType::AlwaysOff){
        ImGui::Indent();
        Hotkey("Teleport", m_keyClientMoveTeleport);
        Hotkey("Move Faster", m_keyClientMoveFaster);
        ImGui::SliderFloat("Speed###ClientMoveSpeed", &m_flClientMoveBaseSpeed, 500.f, 5000.f);
        ImGui::Checkbox("Auto Teleport", &m_bClientMoveAutoTeleport);
        ImGui::Unindent();
    }
    

    MultiSelect("Removals", ({
        {"No Spread", "Spread", m_bNoSpread},
        {"No Recoil", "Recoil", m_bNoRecoil},
        {"Instant Minigame", "Minigame", m_bInstantMinigame},
        {"Instant Reload", "Reload", m_bInstantReload},
        {"Instant Melee", "Melee", m_bInstantMelee},
        {"Auto Pistol", "Auto", m_bAutoPistol}
    }));

    MultiSelect("Camera Modifiers", ({
        {"Disable Shake", "Shake", m_bNoCameraShake},
        {"Disable Tilt", "Tilt", m_bNoCameraTilt}
    }));

    ImGui::SliderFloat("Camera FOV", &m_flCameraFOV, 0.f, 150.f, "%0.0f");

    MultiSelect("Buffs", ({
        {"Speed", "Speed", m_bSpeedBuff},
        {"Damage", "Damage", m_bDamageBuff},
        {"Armor", "Armor", m_bArmorBuff}
    }));

    static const char* aRapidFireOptions[]{ "Disabled", "Steady", "Rapid" };
    ImGui::Combo("Rapid Fire", &m_iRapidFire, aRapidFireOptions, IM_ARRAYSIZE(aRapidFireOptions));

    ImGui::Checkbox("More Bullets", &m_bMoreBullets);
    if(m_bMoreBullets){
        ImGui::SameLine();
        ImGui::SliderInt("###More Bullets Count", &m_iMoreBullets, 1, 100, "%d", ImGuiSliderFlags_AlwaysClamp);
    }
    
    ImGui::Checkbox("Super Toss", &m_bSuperToss);
    if(m_bSuperToss){
        ImGui::SameLine();
        ImGui::SliderFloat("###Super Toss Speed", &m_flSuperToss, 1000.f, 5000.f);
    }

    if (ImGui::Button(m_bThirdPerson ? "3rd Person: ON" : "3rd Person: OFF"))
        m_bThirdPerson = !m_bThirdPerson;
    BindKeyButton("##hk_3rd Person", m_keyThirdPerson);
    ImGui::SameLine();
    if (ImGui::Button("3P Center"))
        m_iThirdPersonSide = 0;
    BindKeyButton("##hk_3rd Person Left", m_keyThirdPersonLeft);
    ImGui::SameLine();
    BindKeyButton("##hk_3rd Person Right", m_keyThirdPersonRight);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.9f, 1.f, 1.f));
    ImGui::TextWrapped("%s", Cheat::ThirdPerson::g_sStatus.c_str());
    ImGui::PopStyleColor();
    ImGui::TextDisabled("F9 toggles 3P. Q = left shoulder, E = right shoulder, Center button puts camera back in the middle.");

    CheckboxWithHotkey("God Mode", &m_bGodMode, m_keyGodMode);
    if (m_bGodMode)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.f, 0.55f, 1.f));
        ImGui::TextWrapped("%s", Cheat::GodAmmo::g_sStatusGod.c_str());
        ImGui::PopStyleColor();
        ImGui::TextDisabled("Incoming dmg×0 + HP/armor top-up (does not block tasers).");
    }

    CheckboxWithHotkey("Infinite Ammo", &m_bInfiniteAmmo, m_keyInfiniteAmmo);
    if (m_bInfiniteAmmo)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.85f, 1.f, 1.f));
        ImGui::TextWrapped("%s", Cheat::GodAmmo::g_sStatusAmmo.c_str());
        ImGui::PopStyleColor();
        ImGui::TextDisabled("True-inf game flag + mag refill backup. Guns only (not placeables).");
    }

    CheckboxWithHotkey("More Bullet Damage (×1000)", &m_bInstaKill, m_keyInstaKill);
    if (m_bInstaKill)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.45f, 0.45f, 1.f));
        ImGui::TextWrapped("%s", Cheat::GodAmmo::g_sStatusInstaKill.c_str());
        ImGui::PopStyleColor();
        ImGui::TextDisabled("Bullets hit harder — same FireData path as Nexus InstantKill.");
    }

    CheckboxWithHotkey("Carry More Bags (you + AI)", &m_bCarryMoreBags, m_keyCarryMoreBags);
    if (m_bCarryMoreBags)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.75f, 1.f, 1.f));
        ImGui::TextWrapped("%s", Cheat::CarryBags::g_sStatus.c_str());
        ImGui::PopStyleColor();
        ImGui::TextDisabled("MaxCarryBagCount → 50 for you and AI crew (SkysBags-style).");
    }

    CheckboxWithHotkey("Carry More Bodies (stack corpses)", &m_bCarryMoreBodies, m_keyCarryMoreBodies);
    if (m_bCarryMoreBodies)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.75f, 1.f, 1.f));
        ImGui::TextWrapped("%s", Cheat::CarryBodies::g_sStatus.c_str());
        ImGui::PopStyleColor();
        ImGui::TextDisabled("Pick up every body you kill — stacks on your back. Press G to drop the whole pile.");
    }

    CheckboxWithHotkey("No Civ / Custody Penalty", &m_bNoCivPenalty, m_keyNoCivPenalty);
    if (m_bNoCivPenalty)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.95f, 0.7f, 1.f));
        ImGui::TextWrapped("%s", Cheat::NoCivPenalty::g_sStatus.c_str());
        ImGui::PopStyleColor();
        ImGui::TextDisabled("Clears civ-kill + jail/custody cash docks on results. Solo/host best.");
    }

    CheckboxWithHotkey("Friendly Fire (HOST you->them SAFE)", &m_bFriendlyFire, m_keyFriendlyFire);
    if (m_bFriendlyFire)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.85f, 0.2f, 1.f));
        ImGui::TextWrapped("%s", Cheat::FriendlyFire::g_sDebugStatus.c_str());
        ImGui::PopStyleColor();
        ImGui::TextDisabled("Crash-safe: hold LMB to damage other players. Friend cannot kill you yet.");
    }

    CheckboxWithHotkey("Grab All (loot)", &m_bGrabAll, m_keyGrabAll);
    if (m_bGrabAll)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.9f, 0.55f, 1.f));
        ImGui::TextWrapped("%s", Cheat::GrabAll::g_sDebugStatus.c_str());
        ImGui::PopStyleColor();
        ImGui::TextDisabled("Real F/Claim + floor bags — Num7 still free for leftovers.");
    }

    CheckboxWithHotkey("Grab Access (keys / RFID / badge / N2)", &m_bGrabAccess, m_keyGrabAccess);
    if (m_bGrabAccess)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.85f, 1.f, 1.f));
        ImGui::TextWrapped("%s", Cheat::GrabAccess::g_sDebugStatus.c_str());
        ImGui::PopStyleColor();
        ImGui::TextDisabled("Num/ equivalent — keycards, RFID, press badge. Num/ still free for Lua.");
    }

    CheckboxWithHotkey("Insta Drill (drills / PCs / thermite / cleaner)", &m_bInstaDrill, m_keyInstaDrill);
    if (m_bInstaDrill)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.75f, 0.4f, 1.f));
        ImGui::TextWrapped("%s", Cheat::InstaDrill::g_sDebugStatus.c_str());
        ImGui::PopStyleColor();
        ImGui::TextDisabled("Drills / PCs / thermite / lance + cash/jewelry cleaner + minigame.");
    }

    CheckboxWithHotkey("Silent Despawn Cops", &m_bSilentKillCops, m_keySilentKillCops);
    if (m_bSilentKillCops)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.45f, 0.45f, 1.f));
        ImGui::TextWrapped("%s", Cheat::SilentKill::g_sDebugStatus.c_str());
        ImGui::PopStyleColor();
        ImGui::TextDisabled("CH_BaseCop only — never Houston / FWB inside man / civs.");
    }

    ImGui::Separator();
    ImGui::TextDisabled("Spawner");

    if (ButtonWithHotkey("Meth spawn", m_keySpawnMeth))
        Cheat::SpawnerTools::RequestMeth();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.75f, 1.f, 1.f));
    ImGui::TextWrapped("%s", Cheat::SpawnerTools::g_sStatusMeth.c_str());
    ImGui::PopStyleColor();

    if (ButtonWithHotkey("Van drive-in", m_keySpawnVan))
        Cheat::SpawnerTools::RequestVan();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.85f, 1.f, 1.f));
    ImGui::TextWrapped("%s", Cheat::SpawnerTools::g_sStatusVan.c_str());
    ImGui::PopStyleColor();

    if (ButtonWithHotkey("Green exit", m_keySpawnGreenExit))
        Cheat::SpawnerTools::RequestGreenExit();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.95f, 0.55f, 1.f));
    ImGui::TextWrapped("%s", Cheat::SpawnerTools::g_sStatusExit.c_str());
    ImGui::PopStyleColor();

    if (ButtonWithHotkey("Money / results", m_keySpawnMoney))
        Cheat::SpawnerTools::RequestMoneyScreen();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.9f, 0.45f, 1.f));
    ImGui::TextWrapped("%s", Cheat::SpawnerTools::g_sStatusMoney.c_str());
    ImGui::PopStyleColor();
}

namespace Menu
{
    void CallTraceEntry_t::Draw()
    {
        if(!g_sCallTraceFilter.empty() && !m_sClassName.contains(g_sCallTraceFilter)){
            if(!g_bCallTraceFilterSubclasses)
                return;
            
            if(std::find_if(m_vecSubClasses.begin(), m_vecSubClasses.end(), [&](const std::string& str) {
                return str.contains(g_sCallTraceFilter);
            }) == m_vecSubClasses.end())
                return;
        }

        if(ImGui::CollapsingHeader(m_sClassName.c_str()))
        {
            ImGui::PushID(m_sClassName.c_str());

            if(m_vecSubClasses.size()){
                if(ImGui::TreeNode("Sub Classes"))
                {
                    for(const auto& str : m_vecSubClasses)
                        ImGui::Text("%s", str.c_str());
                    ImGui::TreePop();
                }
            }
            
            if(ImGui::TreeNode("Called Functions"))
            {
                for(const auto& pairEntry : m_mapCalledFunctions)
                    ImGui::Text("%s", pairEntry.second.c_str());
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    void PreDraw()
    {
        // Present-hook can run during lobby/heist transitions with empty LocalPlayers.
        // TArray::operator[] throws std::out_of_range -> unhandled 0xe06d7363 crash.
        try
        {
            CheatConfig::Get().m_misc.m_keyClientMove.UpdateState();
            CheatConfig::Get().m_misc.m_keyClientMoveTeleport.UpdateState();
            CheatConfig::Get().m_misc.m_keyClientMoveFaster.UpdateState();
            CheatConfig::Get().m_misc.UpdateFeatureHotkeys();
            Cheat::PresetTeleport::PollPendingUi();

            SDK::UWorld* pGWorld = SDK::UWorld::GetWorld();
            if (!pGWorld)
                return;

            SDK::UGameInstance* pGameInstance = pGWorld->OwningGameInstance;
            if (!pGameInstance)
                return;

            if (pGameInstance->LocalPlayers.Num() <= 0)
                return;

            SDK::ULocalPlayer* pLocalPlayer = pGameInstance->LocalPlayers[0];
            if (!pLocalPlayer)
                return;

            SDK::APlayerController* pPlayerController = pLocalPlayer->PlayerController;
            if (!pPlayerController)
                return;

            SDK::ULevel* pPersistentLevel = pGWorld->PersistentLevel;
            if (!pPersistentLevel)
                return;

            ESP::Render(pGWorld, pPlayerController);
            ESP::RenderDebugESP(pPersistentLevel, pPlayerController);
        }
        catch (...)
        {
            // Swallow SDK/layout hiccups so Present never hard-kills the game.
        }
    }

    

	// Draw the main menu content
	void Draw(bool& bShowMenu)
	{
		if (!bShowMenu)
			return;

        std::string windowTitle = std::format("OmegaWare PD3 Internal - {}", CURRENT_VERSION);
        ImGui::Begin(windowTitle.c_str(), &bShowMenu, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

        if(ImGui::BeginTabBar("CheatTabs"))
        {
            if(ImGui::BeginTabItem("Aimbot")){
                CheatConfig::Get().m_aimbot.Draw();
                ImGui::EndTabItem();
            }

            if(ImGui::BeginTabItem("Visuals")){
                CheatConfig::Get().m_visuals.Draw();
                ImGui::EndTabItem();
            }

            if(ImGui::BeginTabItem("Stealth")){
                CheatConfig::Get().m_stealth.Draw();
                ImGui::EndTabItem();
            }

            if(ImGui::BeginTabItem("Misc")){
                CheatConfig::Get().m_misc.Draw();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Teleport")){
                Cheat::PresetTeleport::DrawTab();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Debug")){
                // Host / session — AccelByte path can be null in menus → "Unknown" is normal there.
                auto sHostStatus = []() -> const char* {
                    SDK::UWorld* pGWorld = SDK::UWorld::GetWorld();
                    if (!pGWorld)
                        return "no world";

                    SDK::UGameInstance* pGameInstance = pGWorld->OwningGameInstance;
                    if (!pGameInstance || !pGameInstance->IsA(SDK::USBZGameInstance::StaticClass()))
                        return "no game instance";

                    SDK::USBZGameInstance* pSBZGameInstance = static_cast<SDK::USBZGameInstance*>(pGameInstance);
                    auto pAccelByteUser = pSBZGameInstance->AccelByteUser;
                    if (!pAccelByteUser)
                        return "no AccelByte (lobby?)";

                    return pAccelByteUser->UserActivity.bIsHost ? "Host" : "Client";
                };

                ImGui::Text("Session: %s", sHostStatus());
                ImGui::Text("In heist: %s | Stealth: %s | Solo: %s",
                    Cheat::g_bIsInGame ? "yes" : "no",
                    Cheat::g_bIsInStealth ? "yes" : "no",
                    Cheat::g_bIsSoloGame ? "yes" : "no");

                ImGui::Separator();
                ImGui::TextDisabled("Feature status (updates while Misc checkboxes are on)");
                ImGui::TextWrapped("Grab All: %s", Cheat::GrabAll::g_sDebugStatus.c_str());
                ImGui::TextWrapped("Grab Access: %s", Cheat::GrabAccess::g_sDebugStatus.c_str());
                ImGui::TextWrapped("Insta Drill: %s", Cheat::InstaDrill::g_sDebugStatus.c_str());
                ImGui::TextWrapped("Silent Despawn: %s", Cheat::SilentKill::g_sDebugStatus.c_str());
                ImGui::TextWrapped("God Mode: %s", Cheat::GodAmmo::g_sStatusGod.c_str());
                ImGui::TextWrapped("Infinite Ammo: %s", Cheat::GodAmmo::g_sStatusAmmo.c_str());
                ImGui::TextWrapped("Insta Kill: %s", Cheat::GodAmmo::g_sStatusInstaKill.c_str());
                ImGui::TextWrapped("Carry Bags: %s", Cheat::CarryBags::g_sStatus.c_str());
                ImGui::TextWrapped("No Civ Penalty: %s", Cheat::NoCivPenalty::g_sStatus.c_str());
                ImGui::TextWrapped("Friendly Fire: %s", Cheat::FriendlyFire::g_sDebugStatus.c_str());
                ImGui::TextWrapped("Ghost Mode: %s", Cheat::GhostMode::g_sStatus.c_str());
                ImGui::TextWrapped("Vault Codes: %s", Cheat::VaultCodes::g_sStatus.c_str());
                ImGui::TextWrapped("3rd Person: %s", Cheat::ThirdPerson::g_sStatus.c_str());

                ImGui::Separator();
                ImGui::Text("%.1f FPS (%.3f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
                ImGui::EndTabItem();
            }


            ImGui::EndTabBar();
        }

        ImGui::Separator();
        if (ImGui::Button("Save Config"))
            CheatConfig::Get().Save();

        ImGui::SameLine();
        if (ImGui::Button("Load Config"))
            CheatConfig::Get().Load();

        ImGui::SameLine();
		
		// Performance metrics
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

        ImGui::End();

#ifdef _DEBUG
        ImGui::Begin("Call Traces",  &bShowMenu);
        static const char* aCallTraceItems[] = { "Inactive", "UObject", "PlayerController" };
        if(ImGui::Combo("Call Trace Area", reinterpret_cast<int*>(&g_eCallTraceArea), aCallTraceItems, IM_ARRAYSIZE(aCallTraceItems)))
            g_mapCallTraces.clear();
        
        ImGui::InputText("##Filter", g_szCallTraceFilter, sizeof(g_szCallTraceFilter));
        ImGui::SameLine();
        ImGui::Checkbox("##Filter Use Subclasses", &g_bCallTraceFilterSubclasses);

        g_sCallTraceFilter = g_szCallTraceFilter;

        ImGui::Separator();
        for (auto& pairEntry : g_mapCallTraces)
            pairEntry.second.Draw();
        ImGui::End();
#endif
	}

    void PostDraw() {
        // Moved to config to be grabbed by other methods instead of regrabbing again
        //auto vec2ScreenSize = ImGui::GetIO().DisplaySize;
        auto vec2Pos = ImVec2{ CheatConfig::Get().m_misc.vec2ScreenSize.x / 2.f + 10.f, CheatConfig::Get().m_misc.vec2ScreenSize.y / 2.f + 10.f };
        auto pDrawList = ImGui::GetBackgroundDrawList();

        #ifdef _DEBUG
        if(Cheat::g_bIsDesynced){
            pDrawList->AddText(ImVec2{ vec2Pos.x + 1.f, vec2Pos.y + 1.f }, IM_COL32(0, 0, 0, 255), "DESYNC");
            pDrawList->AddText(vec2Pos, IM_COL32(220, 0, 0, 255), "DESYNC");
            vec2Pos.y += 16.f;
        }

        if(Cheat::g_stTargetInfo){
            pDrawList->AddText(ImVec2{ vec2Pos.x + 1.f, vec2Pos.y + 1.f }, IM_COL32(0, 0, 0, 255), "Target");
            pDrawList->AddText(vec2Pos, IM_COL32(11, 220, 0, 255), "Target");
            vec2Pos.y += 16.f;
        }
        #endif

        // Vault codes flash — top-center, always (not only DEBUG)
        {
            std::string flash;
            if (Cheat::VaultCodes::TryGetFlashText(flash) && !flash.empty())
            {
                const ImVec2 screen = CheatConfig::Get().m_misc.vec2ScreenSize;
                const ImVec2 size = ImGui::CalcTextSize(flash.c_str());
                const ImVec2 pos{ (screen.x - size.x) * 0.5f, screen.y * 0.12f };
                pDrawList->AddText(ImVec2{ pos.x + 2.f, pos.y + 2.f }, IM_COL32(0, 0, 0, 220), flash.c_str());
                pDrawList->AddText(pos, IM_COL32(255, 230, 80, 255), flash.c_str());
            }
        }

        // Ghost mode indicator (small corner tag while on)
        if (CheatConfig::Get().m_misc.m_bGhostMode)
        {
            const char* tag = "GHOST";
            const ImVec2 pos{ 24.f, 24.f };
            pDrawList->AddText(ImVec2{ pos.x + 1.f, pos.y + 1.f }, IM_COL32(0, 0, 0, 255), tag);
            pDrawList->AddText(pos, IM_COL32(140, 200, 255, 255), tag);
        }
        if (CheatConfig::Get().m_misc.m_bThirdPerson)
        {
            const char* tag = "3RD";
            const ImVec2 pos{ 24.f, 44.f };
            pDrawList->AddText(ImVec2{ pos.x + 1.f, pos.y + 1.f }, IM_COL32(0, 0, 0, 255), tag);
            pDrawList->AddText(pos, IM_COL32(220, 220, 255, 255), tag);
        }
    }
}
