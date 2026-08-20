#pragma once

#include <map>
#include <imgui.h>
#include "Dumper-7/SDK.hpp"
#include "Config.hpp"
#include "Utils/Logging.hpp"
#include "Features/ESP/ESP.hpp"

namespace Menu{
    struct Hotkey_t{
        enum class EType : uint8_t{
            AlwaysOff,
            AlwaysOn,
            Hold,
            HoldOff,
            Toggle
        };
        
        ImGuiKey m_eKeyCode = ImGuiKey_None;
        EType m_eType = EType::Hold;
        bool m_bFixedType = false;
        bool m_bActive = false;
        bool m_bPressedThisFrame = false;

        const char* ToString() const{
            switch(m_eKeyCode){
            case ImGuiKey_None: return "None";
            case ImGuiKey_Tab: return "Tab";   
            case ImGuiKey_LeftArrow: return "Left";
            case ImGuiKey_RightArrow: return "Right";
            case ImGuiKey_UpArrow: return "Up";
            case ImGuiKey_DownArrow: return "Down";
            case ImGuiKey_PageUp: return "Page Up";
            case ImGuiKey_PageDown: return "Page Down";
            case ImGuiKey_Home: return "Home";
            case ImGuiKey_End: return "End";
            case ImGuiKey_Insert: return "Insert";
            case ImGuiKey_Delete: return "Delete";
            case ImGuiKey_Backspace: return "Backspace";
            case ImGuiKey_Space: return "Space";
            case ImGuiKey_Enter: return "Enter";
            case ImGuiKey_Escape: return "None";
            case ImGuiKey_LeftCtrl: return "Left Ctrl";
            case ImGuiKey_LeftShift: return "Left Shift";
            case ImGuiKey_LeftAlt: return "Left Alt";
            case ImGuiKey_LeftSuper: return "Left Super";
            case ImGuiKey_RightCtrl: return "Right Ctrl";
            case ImGuiKey_RightShift: return "Right Shift";
            case ImGuiKey_RightAlt: return "Right Alt";
            case ImGuiKey_RightSuper: return "Right Super";
            case ImGuiKey_Menu: return "Menu";
            case ImGuiKey_0: return "0";
            case ImGuiKey_1: return "1";
            case ImGuiKey_2: return "2";
            case ImGuiKey_3: return "3";
            case ImGuiKey_4: return "4";
            case ImGuiKey_5: return "5";
            case ImGuiKey_6: return "6";
            case ImGuiKey_7: return "7";
            case ImGuiKey_8: return "8";
            case ImGuiKey_9: return "9";
            case ImGuiKey_A: return "A";
            case ImGuiKey_B: return "B";
            case ImGuiKey_C: return "C";
            case ImGuiKey_D: return "D";
            case ImGuiKey_E: return "E";
            case ImGuiKey_F: return "F";
            case ImGuiKey_G: return "G";
            case ImGuiKey_H: return "H";
            case ImGuiKey_I: return "I";
            case ImGuiKey_J: return "J";
            case ImGuiKey_K: return "K";
            case ImGuiKey_L: return "L";
            case ImGuiKey_M: return "M";
            case ImGuiKey_N: return "N";
            case ImGuiKey_O: return "O";
            case ImGuiKey_P: return "P";
            case ImGuiKey_Q: return "Q";
            case ImGuiKey_R: return "R";
            case ImGuiKey_S: return "S";
            case ImGuiKey_T: return "T";
            case ImGuiKey_U: return "U";
            case ImGuiKey_V: return "V";
            case ImGuiKey_W: return "W";
            case ImGuiKey_X: return "X";
            case ImGuiKey_Y: return "Y";
            case ImGuiKey_Z: return "Z";
            case ImGuiKey_F1: return "F1";
            case ImGuiKey_F2: return "F2";
            case ImGuiKey_F3: return "F3";
            case ImGuiKey_F4: return "F4";
            case ImGuiKey_F5: return "F5";
            case ImGuiKey_F6: return "F6";
            case ImGuiKey_F7: return "F7";
            case ImGuiKey_F8: return "F8";
            case ImGuiKey_F9: return "F9";
            case ImGuiKey_F10: return "F10";
            case ImGuiKey_F11: return "F11";
            case ImGuiKey_F12: return "F12";
            case ImGuiKey_F13: return "F13";
            case ImGuiKey_F14: return "F14";
            case ImGuiKey_F15: return "F15";
            case ImGuiKey_F16: return "F16";
            case ImGuiKey_F17: return "F17";
            case ImGuiKey_F18: return "F18";
            case ImGuiKey_F19: return "F19";
            case ImGuiKey_F20: return "F20";
            case ImGuiKey_F21: return "F21";
            case ImGuiKey_F22: return "F22";
            case ImGuiKey_F23: return "F23";
            case ImGuiKey_F24: return "F24";
            case ImGuiKey_Apostrophe: return "'";
            case ImGuiKey_Comma: return ",";
            case ImGuiKey_Minus: return "-";
            case ImGuiKey_Period: return ".";
            case ImGuiKey_Slash: return "/";
            case ImGuiKey_Semicolon: return ";";
            case ImGuiKey_Equal: return "=";
            case ImGuiKey_LeftBracket: return "["; 
            case ImGuiKey_Backslash: return "\\";
            case ImGuiKey_RightBracket: return "]";
            case ImGuiKey_GraveAccent: return "`";
            case ImGuiKey_CapsLock: return "Caps Lock";
            case ImGuiKey_ScrollLock: return "Scroll Lock";
            case ImGuiKey_NumLock: return "Num Lock";
            case ImGuiKey_PrintScreen: return "Print Screen";
            case ImGuiKey_Pause: return "Pause";
            case ImGuiKey_Keypad0: return "Kp0";
            case ImGuiKey_Keypad1: return "Kp1";
            case ImGuiKey_Keypad2: return "Kp2";
            case ImGuiKey_Keypad3: return "Kp3";
            case ImGuiKey_Keypad4: return "Kp4";
            case ImGuiKey_Keypad5: return "Kp5";
            case ImGuiKey_Keypad6: return "Kp6";
            case ImGuiKey_Keypad7: return "Kp7";
            case ImGuiKey_Keypad8: return "Kp8";
            case ImGuiKey_Keypad9: return "Kp9";
            case ImGuiKey_KeypadDecimal: return "Kp Dec";
            case ImGuiKey_KeypadDivide: return "Kp Div";
            case ImGuiKey_KeypadMultiply: return "Kp Mul";
            case ImGuiKey_KeypadSubtract: return "Kp Sub";
            case ImGuiKey_KeypadAdd: return "Kp Add";
            case ImGuiKey_KeypadEnter: return "Kp Enter";
            case ImGuiKey_KeypadEqual: return "Kp Equ";
            case ImGuiKey_AppBack: return "App Back";
            case ImGuiKey_AppForward: return "App Forward";
            case ImGuiKey_Oem102: return "\\";
            case ImGuiKey_GamepadStart: return "Con Start";
            case ImGuiKey_GamepadBack: return "Con Back";
            case ImGuiKey_GamepadFaceLeft: return "Con X";
            case ImGuiKey_GamepadFaceRight: return "Con B";
            case ImGuiKey_GamepadFaceUp: return "Con Y";
            case ImGuiKey_GamepadFaceDown: return "Con A";
            case ImGuiKey_GamepadDpadLeft: return "Con D-Left" ;
            case ImGuiKey_GamepadDpadRight: return "Con D-Right";
            case ImGuiKey_GamepadDpadUp: return "Con D-Up";
            case ImGuiKey_GamepadDpadDown: return "Con D-Down";
            case ImGuiKey_GamepadL1: return "Con L1";     
            case ImGuiKey_GamepadR1: return "Con R1";
            case ImGuiKey_GamepadL2: return "Con L2";
            case ImGuiKey_GamepadR2: return "Con R2";
            case ImGuiKey_GamepadL3: return "Con L3";
            case ImGuiKey_GamepadR3: return "Con R3";
            case ImGuiKey_GamepadLStickLeft: return "Con L-Left";
            case ImGuiKey_GamepadLStickRight: return "Con L-Right";
            case ImGuiKey_GamepadLStickUp: return "Con L-Up";
            case ImGuiKey_GamepadLStickDown: return "Con L-Down";
            case ImGuiKey_GamepadRStickLeft: return "Con R-Left";
            case ImGuiKey_GamepadRStickRight: return "Con R-Right";
            case ImGuiKey_GamepadRStickUp: return "Con R-Up";
            case ImGuiKey_GamepadRStickDown: return "Con R-Down";
            case ImGuiKey_MouseLeft: return "M1";
            case ImGuiKey_MouseRight: return "M2";
            case ImGuiKey_MouseMiddle: return "M3";
            case ImGuiKey_MouseX1: return "M4";
            case ImGuiKey_MouseX2: return "M5";
            case ImGuiKey_MouseWheelX: return "Mouse Wheel X";
            case ImGuiKey_MouseWheelY: return "Mouse Wheel Y";

            default:
                return "Unk";
            }
        };

        void UpdateState(){
            m_bPressedThisFrame = false;
            // Bad config / unbound / ImGui not ready — never call IsKey* with junk keys.
            if (m_eKeyCode == ImGuiKey_None
                || m_eKeyCode < ImGuiKey_NamedKey_BEGIN
                || m_eKeyCode >= ImGuiKey_NamedKey_END
                || !ImGui::GetCurrentContext())
            {
                if (m_eType == EType::AlwaysOn)
                    m_bActive = true;
                else if (m_eType == EType::AlwaysOff || m_eType == EType::Hold)
                    m_bActive = false;
                return;
            }

            m_bPressedThisFrame = ImGui::IsKeyPressed(m_eKeyCode, false);
            switch(m_eType){
            case EType::AlwaysOff:
                m_bActive = false;
                break;
            case EType::AlwaysOn:
                m_bActive = true;
                break;
            case EType::Hold:
                m_bActive = ImGui::IsKeyDown(m_eKeyCode);
                break;
            case EType::HoldOff:
                m_bActive = !ImGui::IsKeyDown(m_eKeyCode);
                break;
            case EType::Toggle:
                if(m_bPressedThisFrame)
                    m_bActive = !m_bActive;
                break;
            }
        };

        inline bool GetState() const{
            return m_bActive;
        };

        inline bool Pressed() const{
            return m_bPressedThisFrame;
        };

        bool SetToPressedKey(){
            for(int i = ImGuiKey_NamedKey_BEGIN; i < ImGuiKey_NamedKey_END; ++i){
                if(!ImGui::IsKeyPressed(static_cast<ImGuiKey>(i)))
                    continue;

                m_eKeyCode = (static_cast<ImGuiKey>(i) == ImGuiKey_Escape) ? ImGuiKey_None : static_cast<ImGuiKey>(i);
                return true;
            }

            return false;
        }
    };
};

struct CheatConfig{
    struct MultiSelect_t{
        
    };

    struct Aimbot_t {
        bool m_bEnabled = false;
        float m_flAimFOV = 180.f;

        enum class ESorting{
            Smart,
            FOV,
            Threat
        };

        // Silent = bullets track without camera move (current). Snapping = camera follows target.
        enum class EAimType {
            Silent = 0,
            Snapping = 1
        };

        ESorting m_eSorting = ESorting::Smart;
        EAimType m_eAimType = EAimType::Silent;
        // 0 = instant snap; higher = slower lerp toward target (Snapping mode).
        int m_iSmoothing = 10;

        bool m_bGuards = true;
        bool m_bSpecials = true;
        bool m_bFBIVan = true;
        bool m_bCivilians = false;

        bool m_bThroughWalls = true;
        bool m_bDisableInStealth = true;

        void Draw();
    };

    Aimbot_t m_aimbot{};

    struct Visuals_t {
        void Draw();
    };

    Visuals_t m_visuals{};

    struct Stealth_t {
        void Draw();
    };

    Stealth_t m_stealth{};

    struct Misc_t {
        Menu::Hotkey_t m_keyClientMove{ ImGuiKey_MouseX2, Menu::Hotkey_t::EType::Hold };
        Menu::Hotkey_t m_keyClientMoveTeleport{ ImGuiKey_Z, Menu::Hotkey_t::EType::Hold, true };
        Menu::Hotkey_t m_keyClientMoveFaster{ ImGuiKey_LeftShift, Menu::Hotkey_t::EType::Hold, true };
        bool m_bClientMoveAutoTeleport = true;
        float m_flClientMoveBaseSpeed = 1000.f;

        bool m_bNoSpread = true;
        bool m_bNoRecoil = true;
        bool m_bNoFallDamage = true;
        bool m_bInstantInteraction = true;
        bool m_bInstantMinigame = true;
        bool m_bInstantReload = true;
        bool m_bInstantMelee = true;
        bool m_bAutoPistol = true;

        bool m_bSpeedBuff = true;
        bool m_bDamageBuff = true;
        bool m_bArmorBuff = true;

        bool m_bNoCameraShake = true;
        bool m_bNoCameraTilt = true;
        float m_flCameraFOV = 90.f;

        int32_t m_iRapidFire = 1;

        bool m_bMoreBullets = true;
        int32_t m_iMoreBullets = 20;

        bool m_bSuperToss = true;
        float m_flSuperToss = 2000.f;

        // Mutual PvP when YOU are host. Starts OFF every launch.
        bool m_bFriendlyFire = false;
        Menu::Hotkey_t m_keyFriendlyFire{ ImGuiKey_None, Menu::Hotkey_t::EType::Toggle, true };

        // Auto grab loot while checked (does NOT use Num7 — leave that for Lua).
        // Starts OFF every launch.
        bool m_bGrabAll = false;
        Menu::Hotkey_t m_keyGrabAll{ ImGuiKey_None, Menu::Hotkey_t::EType::Toggle, true };

        // Num/ equivalent — keycards / RFID / hackable keys. Starts OFF every launch.
        bool m_bGrabAccess = false;
        Menu::Hotkey_t m_keyGrabAccess{ ImGuiKey_None, Menu::Hotkey_t::EType::Toggle, true };

        // Num8 equivalent — drills / thermite / lance. Starts OFF every launch.
        bool m_bInstaDrill = false;
        Menu::Hotkey_t m_keyInstaDrill{ ImGuiKey_None, Menu::Hotkey_t::EType::Toggle, true };

        // Num* equivalent — silent bury-kill cops only. Starts OFF every launch.
        bool m_bSilentKillCops = false;
        Menu::Hotkey_t m_keySilentKillCops{ ImGuiKey_None, Menu::Hotkey_t::EType::Toggle, true };

        // SkyCheats Num0 quality — start OFF every launch.
        bool m_bGodMode = false;
        Menu::Hotkey_t m_keyGodMode{ ImGuiKey_None, Menu::Hotkey_t::EType::Toggle, true };
        bool m_bInfiniteAmmo = false;
        Menu::Hotkey_t m_keyInfiniteAmmo{ ImGuiKey_None, Menu::Hotkey_t::EType::Toggle, true };

        // FireData bullet damage ×1000 — start OFF every launch.
        bool m_bInstaKill = false;
        Menu::Hotkey_t m_keyInstaKill{ ImGuiKey_None, Menu::Hotkey_t::EType::Toggle, true };

        // MaxCarryBagCount bump for you + AI crew — start OFF every launch.
        bool m_bCarryMoreBags = false;
        Menu::Hotkey_t m_keyCarryMoreBags{ ImGuiKey_None, Menu::Hotkey_t::EType::Toggle, true };

        // Body disposer pile cap (dumpsters etc.) — start OFF every launch.
        bool m_bCarryMoreBodies = false;
        Menu::Hotkey_t m_keyCarryMoreBodies{ ImGuiKey_None, Menu::Hotkey_t::EType::Toggle, true };

        // Civ + custody end penalties — start OFF every launch.
        bool m_bNoCivPenalty = false;
        Menu::Hotkey_t m_keyNoCivPenalty{ ImGuiKey_None, Menu::Hotkey_t::EType::Toggle, true };

        // Vault/keypad codes one-shot (default F10). Ghost toggle (default F11).
        Menu::Hotkey_t m_keyVaultCodes{ ImGuiKey_F10, Menu::Hotkey_t::EType::Toggle, true };
        bool m_bGhostMode = false;
        Menu::Hotkey_t m_keyGhostMode{ ImGuiKey_F11, Menu::Hotkey_t::EType::Toggle, true };

        // Playable 3rd person behind YOUR body (not PD3 spectator). Default F9 — F10 is Vault Codes.
        bool m_bThirdPerson = false;
        Menu::Hotkey_t m_keyThirdPerson{ ImGuiKey_F9, Menu::Hotkey_t::EType::Toggle, true };
        int m_iThirdPersonSide = 0; // -1 left, 0 center, +1 right
        Menu::Hotkey_t m_keyThirdPersonLeft{ ImGuiKey_Q, Menu::Hotkey_t::EType::Toggle, true };
        Menu::Hotkey_t m_keyThirdPersonRight{ ImGuiKey_E, Menu::Hotkey_t::EType::Toggle, true };

        // Spawner one-shots (bind next to buttons; start unbound).
        Menu::Hotkey_t m_keySpawnMeth{ ImGuiKey_None, Menu::Hotkey_t::EType::Toggle, true };
        Menu::Hotkey_t m_keySpawnVan{ ImGuiKey_None, Menu::Hotkey_t::EType::Toggle, true };
        Menu::Hotkey_t m_keySpawnGreenExit{ ImGuiKey_None, Menu::Hotkey_t::EType::Toggle, true };
        Menu::Hotkey_t m_keySpawnMoney{ ImGuiKey_None, Menu::Hotkey_t::EType::Toggle, true };

        
        void Draw();
        void UpdateFeatureHotkeys(); // poll binds → toggle bools / fire spawners

        ImVec2 vec2ScreenSize = ImGui::GetIO().DisplaySize;
    };

    Misc_t m_misc{};

    bool Save() const;
    bool Load();

    static CheatConfig& Get(){
        static CheatConfig config{};
        return config;
    };
};

namespace Menu
{
    enum class ECallTraceArea{
        Inactive,
        UObject,
        PlayerController
    };

    inline char g_szCallTraceFilter[1024]{};
    inline bool g_bCallTraceFilterSubclasses = false;
    inline ECallTraceArea g_eCallTraceArea = ECallTraceArea::Inactive;

    inline std::string g_sCallTraceFilter{};

    struct CallTraceEntry_t{
        std::string m_sClassName{};
        std::vector<std::string> m_vecSubClasses{};
        std::map<size_t, std::string> m_mapCalledFunctions{};

        void Draw();
    };

    inline std::map<size_t, CallTraceEntry_t> g_mapCallTraces{};

    void PreDraw();
	void Draw(bool& bShowMenu);
    void PostDraw();
}
