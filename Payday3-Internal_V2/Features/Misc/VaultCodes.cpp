#include "pch.h"
#include "VaultCodes.hpp"
#include "HeistUtil.hpp"

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <format>
#include <set>
#include <string>
#include <vector>

#undef min
#undef max

namespace VaultCodes
{
    std::string g_sStatus = "Press F10 / Vault Codes to scan";
    std::string g_sHelperStatus = "Keypad helper off";

    namespace
    {
        constexpr auto kFlashMs = std::chrono::milliseconds(4000);
        constexpr auto kHelperGap = std::chrono::milliseconds(100);

        static bool s_bPending = false;
        static bool s_bHelperWasOn = false;
        static std::string s_sFlash{};
        static std::chrono::steady_clock::time_point s_timeFlashEnd{};
        static std::chrono::steady_clock::time_point s_timeNextHelper{};

        // SEH helpers — no C++ objects with destructors in these functions.
        static bool ActorOkSeh(SDK::AActor* pActor)
        {
            __try
            {
                return pActor && pActor->Class && !pActor->IsActorBeingDestroyed();
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool GetKeypadActorsSeh(SDK::UWorld* pGWorld, SDK::TArray<SDK::AActor*>* pOut)
        {
            __try
            {
                SDK::UGameplayStatics::GetAllActorsOfClass(
                    pGWorld, SDK::ASBZKeypadBase::StaticClass(), pOut);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool GetNoteActorsSeh(SDK::UWorld* pGWorld, SDK::TArray<SDK::AActor*>* pOut)
        {
            __try
            {
                SDK::UGameplayStatics::GetAllActorsOfClass(
                    pGWorld, SDK::ASBZCodeNote::StaticClass(), pOut);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool ReadKeypadCodeSeh(SDK::ASBZKeypadBase* pPad, int32_t* pOut)
        {
            __try
            {
                if (!pPad)
                    return false;
                *pOut = pPad->Code;
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool ReadMultiCodeAtSeh(SDK::ASBZMultiCodeKeypad* pMulti, int index, int32_t* pOut)
        {
            __try
            {
                if (!pMulti || index < 0 || index >= pMulti->CodeArray.Num())
                    return false;
                *pOut = pMulti->CodeArray[index];
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static int MultiCodeCountSeh(SDK::ASBZMultiCodeKeypad* pMulti)
        {
            __try
            {
                return pMulti ? pMulti->CodeArray.Num() : 0;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return 0;
            }
        }

        static bool IsMultiKeypadSeh(SDK::ASBZKeypadBase* pPad)
        {
            __try
            {
                return pPad && pPad->IsA(SDK::ASBZMultiCodeKeypad::StaticClass());
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool ApplyKeypadHelperSeh(SDK::ASBZKeypadBase* pKeypad)
        {
            __try
            {
                if (!pKeypad || !pKeypad->Class || pKeypad->IsActorBeingDestroyed())
                    return false;

                uint8_t iActiveKey = 0;
                switch (pKeypad->Inputs)
                {
                case 0:
                    iActiveKey = static_cast<uint8_t>(pKeypad->Code / 1000);
                    break;
                case 1:
                    iActiveKey = static_cast<uint8_t>((pKeypad->Code / 100) % 10);
                    break;
                case 2:
                    iActiveKey = static_cast<uint8_t>((pKeypad->Code / 10) % 10);
                    break;
                case 3:
                    iActiveKey = static_cast<uint8_t>(pKeypad->Code % 10);
                    break;
                default:
                    iActiveKey = (pKeypad->GuessedCode != pKeypad->Code) ? 10 : 11;
                    break;
                }

                const int n = pKeypad->KeypadInteractableComponentArray.Num();
                for (int i = 0; i < n; ++i)
                {
                    SDK::USBZInteractableComponent* pKey = pKeypad->KeypadInteractableComponentArray[i];
                    if (pKey)
                        pKey->SetLocalEnabled(i == static_cast<int>(iActiveKey));
                }
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static void RunKeypadHelper(SDK::UWorld* pGWorld)
        {
            if (!pGWorld || !HeistUtil::IsInHeist())
            {
                g_sHelperStatus = "Keypad helper ON — wait for heist";
                return;
            }

            const auto now = std::chrono::steady_clock::now();
            if (now < s_timeNextHelper)
                return;
            s_timeNextHelper = now + kHelperGap;

            SDK::TArray<SDK::AActor*> actors{};
            if (!GetKeypadActorsSeh(pGWorld, &actors))
            {
                g_sHelperStatus = "Keypad helper ON — scan failed";
                return;
            }

            int n = 0;
            for (int i = 0; i < actors.Num(); ++i)
            {
                auto* pPad = reinterpret_cast<SDK::ASBZKeypadBase*>(actors[i]);
                if (!ActorOkSeh(pPad))
                    continue;
                if (ApplyKeypadHelperSeh(pPad))
                    ++n;
            }

            char buf[96]{};
            std::snprintf(buf, sizeof(buf), "Keypad helper ON — pads=%d", n);
            g_sHelperStatus = buf;
        }

        static int NoteCodeCountSeh(SDK::ASBZCodeNote* pNote)
        {
            __try
            {
                return pNote ? pNote->CodeData.GeneratedCodeArray.Num() : 0;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return 0;
            }
        }

        static bool ReadNoteCodeAtSeh(SDK::ASBZCodeNote* pNote, int index, int32_t* pOut)
        {
            __try
            {
                if (!pNote || index < 0 || index >= pNote->CodeData.GeneratedCodeArray.Num())
                    return false;
                *pOut = pNote->CodeData.GeneratedCodeArray[index];
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static std::string PadCode(int32_t n)
        {
            if (n < 0)
                return {};
            if (n <= 9999)
            {
                char buf[16]{};
                std::snprintf(buf, sizeof(buf), "%04d", n);
                return buf;
            }
            return std::to_string(n);
        }

        static void SetFlash(const std::string& text)
        {
            s_sFlash = text;
            s_timeFlashEnd = std::chrono::steady_clock::now() + kFlashMs;
        }

        static void ScanWorld(SDK::UWorld* pGWorld)
        {
            if (!pGWorld)
            {
                g_sStatus = "CODES: no world (enter heist)";
                SetFlash(g_sStatus);
                return;
            }

            std::vector<std::string> lines{};
            std::set<std::string> seen{};

            auto Add = [&](const char* kind, const std::string& code)
            {
                if (code.empty())
                    return;
                if (code == "0000" && std::string(kind) != "NOTE")
                    return;
                const std::string key = std::string(kind) + "|" + code;
                if (!seen.insert(key).second)
                    return;
                lines.push_back(std::string("[CODES] ") + kind + " " + code);
            };

            {
                SDK::TArray<SDK::AActor*> actors{};
                if (GetKeypadActorsSeh(pGWorld, &actors))
                {
                    for (int i = 0; i < actors.Num(); ++i)
                    {
                        auto* pPad = reinterpret_cast<SDK::ASBZKeypadBase*>(actors[i]);
                        if (!ActorOkSeh(pPad))
                            continue;

                        int32_t code = 0;
                        if (ReadKeypadCodeSeh(pPad, &code))
                        {
                            const std::string padded = PadCode(code);
                            if (!padded.empty() && padded != "0000")
                                Add("KEYPAD", padded);
                        }

                        if (IsMultiKeypadSeh(pPad))
                        {
                            auto* pMulti = reinterpret_cast<SDK::ASBZMultiCodeKeypad*>(pPad);
                            const int n = (std::min)(MultiCodeCountSeh(pMulti), 8);
                            for (int j = 0; j < n; ++j)
                            {
                                int32_t v = 0;
                                if (ReadMultiCodeAtSeh(pMulti, j, &v))
                                    Add("MULTI", PadCode(v));
                            }
                        }
                    }
                }
            }

            {
                SDK::TArray<SDK::AActor*> notes{};
                if (GetNoteActorsSeh(pGWorld, &notes))
                {
                    for (int i = 0; i < notes.Num(); ++i)
                    {
                        auto* pNote = reinterpret_cast<SDK::ASBZCodeNote*>(notes[i]);
                        if (!ActorOkSeh(pNote))
                            continue;

                        const int n = (std::min)(NoteCodeCountSeh(pNote), 8);
                        for (int j = 0; j < n; ++j)
                        {
                            int32_t v = 0;
                            if (ReadNoteCodeAtSeh(pNote, j, &v))
                            {
                                const std::string padded = PadCode(v);
                                if (!padded.empty())
                                    Add("NOTE", padded);
                            }
                        }
                    }
                }
            }

            if (lines.empty())
            {
                g_sStatus = "[CODES] none yet — near vault, F10 again";
                SetFlash(g_sStatus);
                Utils::LogDebug("VaultCodes: none found");
                return;
            }

            std::string joined;
            for (size_t i = 0; i < lines.size(); ++i)
            {
                if (i)
                    joined += "\n";
                joined += lines[i];
            }
            g_sStatus = joined;

            std::string flash = lines[0];
            if (lines.size() > 1)
                flash += " (+" + std::to_string(lines.size() - 1) + " more)";
            SetFlash(flash);
            Utils::LogDebug(std::format("VaultCodes: found {} — {}", lines.size(), flash));
        }
    }

    void RequestScan()
    {
        s_bPending = true;
        g_sStatus = "CODES: scanning…";
    }

    bool TryGetFlashText(std::string& outText)
    {
        if (s_sFlash.empty())
            return false;
        if (std::chrono::steady_clock::now() >= s_timeFlashEnd)
        {
            s_sFlash.clear();
            return false;
        }
        outText = s_sFlash;
        return true;
    }

    void Tick(
        SDK::UWorld* pGWorld,
        SDK::ASBZPlayerController* /*pLocalController*/,
        SDK::ASBZPlayerCharacter* /*pLocalPlayer*/,
        bool bKeypadHelper)
    {
        if (s_bPending)
        {
            s_bPending = false;
            ScanWorld(pGWorld);
        }

        if (!bKeypadHelper)
        {
            if (s_bHelperWasOn)
            {
                s_bHelperWasOn = false;
                g_sHelperStatus = "Keypad helper off";
            }
            return;
        }

        s_bHelperWasOn = true;
        RunKeypadHelper(pGWorld);
    }
}
