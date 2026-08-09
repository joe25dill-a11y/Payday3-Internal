#include "PresetTeleport.hpp"
#include "../../Menu.hpp"

#include <Windows.h>
#include <commdlg.h>
#include <ShlObj.h>

#include <cctype>
#include <chrono>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace fs = std::filesystem;

namespace Cheat::PresetTeleport
{
    std::string g_sStatus = "No JSON loaded — click Load JSON";

    namespace
    {
        struct Spot_t
        {
            std::string name;
            float x = 0.f;
            float y = 0.f;
            float z = 0.f;
            float pitch = 0.f;
            float yaw = 0.f;
        };

        static std::vector<Spot_t> s_vecSpots{};
        static std::string s_sFileName{};
        static std::wstring s_wsLastDir{};
        static int s_iIndex = 0;
        static bool s_bTour = false;
        static float s_flDelaySec = 1.5f;
        static std::chrono::steady_clock::time_point s_timeNextTp{};
        static bool s_bPendingDialog = false;
        static bool s_bPendingTpHere = false;      // you only
        static bool s_bPendingTpEveryone = false;  // you + AI to selected spot
        static int s_iPendingTpIndex = -1;
        static int s_iLastCrewTp = 0;

        static SDK::UWorld* s_pWorld = nullptr;
        static SDK::ASBZPlayerController* s_pCtrl = nullptr;
        static SDK::ASBZPlayerCharacter* s_pPawn = nullptr;

        static std::string Narrow(const std::wstring& w)
        {
            if (w.empty())
                return {};
            const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
            std::string out(n, '\0');
            WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), out.data(), n, nullptr, nullptr);
            return out;
        }

        static bool DirExists(const std::wstring& path)
        {
            DWORD attr = GetFileAttributesW(path.c_str());
            return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
        }

        static std::wstring DesktopPath()
        {
            wchar_t buf[MAX_PATH]{};
            if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, buf)))
                return buf;
            return {};
        }

        static std::wstring GameDir()
        {
            wchar_t buf[MAX_PATH]{};
            if (!GetModuleFileNameW(nullptr, buf, MAX_PATH))
                return {};
            return fs::path(buf).parent_path().wstring();
        }

        static std::wstring DefaultOpenDir()
        {
            if (!s_wsLastDir.empty() && DirExists(s_wsLastDir))
                return s_wsLastDir;

            const std::wstring desk = DesktopPath();
            if (!desk.empty())
            {
                const std::wstring hf = desk + L"\\Heist Farmer v1.4\\Teleport Config";
                if (DirExists(hf))
                    return hf;
            }

            const std::wstring game = GameDir();
            if (!game.empty())
            {
                auto presets2 = fs::path(game) / L"Mods" / L"ScoutFreecamPresets" / L"Presets";
                if (DirExists(presets2.wstring()))
                    return presets2.wstring();
            }
            return desk;
        }

        static std::string MatchField(const std::string& block, const char* key)
        {
            const std::string pat1 = std::string("\"") + key + "\"";
            size_t p = block.find(pat1);
            if (p == std::string::npos)
                return {};
            p = block.find(':', p + pat1.size());
            if (p == std::string::npos)
                return {};
            ++p;
            while (p < block.size() && (block[p] == ' ' || block[p] == '\t'))
                ++p;
            if (p >= block.size())
                return {};
            if (block[p] == '"')
            {
                ++p;
                size_t end = block.find('"', p);
                if (end == std::string::npos)
                    return {};
                return block.substr(p, end - p);
            }
            size_t end = p;
            while (end < block.size()
                && (isdigit((unsigned char)block[end]) || block[end] == '-' || block[end] == '+'
                    || block[end] == '.' || block[end] == 'e' || block[end] == 'E'))
                ++end;
            return block.substr(p, end - p);
        }

        static std::vector<Spot_t> ParsePresetJson(const std::string& text)
        {
            std::vector<Spot_t> out;
            size_t i = 0;
            while (i < text.size())
            {
                size_t open = text.find('{', i);
                if (open == std::string::npos)
                    break;
                size_t close = text.find('}', open + 1);
                if (close == std::string::npos)
                    break;
                const std::string block = text.substr(open, close - open + 1);
                i = close + 1;

                const std::string sx = MatchField(block, "X");
                const std::string sy = MatchField(block, "Y");
                const std::string sz = MatchField(block, "Z");
                if (sx.empty() || sy.empty() || sz.empty())
                    continue;

                Spot_t s{};
                s.name = MatchField(block, "Name");
                if (s.name.empty())
                    s.name = "Unnamed";
                try
                {
                    s.x = std::stof(sx);
                    s.y = std::stof(sy);
                    s.z = std::stof(sz);
                }
                catch (...)
                {
                    continue;
                }
                try
                {
                    const std::string pitch = MatchField(block, "RotationX");
                    const std::string yaw = MatchField(block, "RotationY");
                    if (!pitch.empty())
                        s.pitch = std::stof(pitch);
                    if (!yaw.empty())
                        s.yaw = std::stof(yaw);
                }
                catch (...)
                {
                }
                out.push_back(std::move(s));
            }
            return out;
        }

        static void RefreshStatus()
        {
            if (s_vecSpots.empty())
            {
                g_sStatus = "No JSON loaded — click Load JSON";
                return;
            }
            if (s_iIndex < 0)
                s_iIndex = 0;
            if (s_iIndex >= (int)s_vecSpots.size())
                s_iIndex = (int)s_vecSpots.size() - 1;
            const auto& spot = s_vecSpots[s_iIndex];
            g_sStatus = s_sFileName + " | spot " + std::to_string(s_iIndex + 1) + "/"
                + std::to_string(s_vecSpots.size()) + " | " + spot.name
                + (s_bTour ? " | TOUR..." : "")
                + (s_iLastCrewTp > 0 ? (" | crew+" + std::to_string(s_iLastCrewTp)) : "");
        }

        static bool LoadFromPath(const std::wstring& path)
        {
            s_bTour = false;
            std::ifstream in(path, std::ios::binary);
            if (!in)
            {
                g_sStatus = "Failed to open file";
                return false;
            }
            std::ostringstream ss;
            ss << in.rdbuf();
            auto spots = ParsePresetJson(ss.str());
            if (spots.empty())
            {
                g_sStatus = "No spots found in JSON";
                return false;
            }
            s_vecSpots = std::move(spots);
            s_iIndex = 0;
            fs::path p(path);
            s_sFileName = Narrow(p.filename().wstring());
            s_wsLastDir = p.parent_path().wstring();
            RefreshStatus();
            return true;
        }

        static void DoOpenDialog()
        {
            wchar_t fileBuf[MAX_PATH]{};
            OPENFILENAMEW ofn{};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = nullptr;
            ofn.lpstrFile = fileBuf;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrFilter = L"JSON Presets (*.json)\0*.json\0All Files (*.*)\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
            std::wstring initDir = DefaultOpenDir();
            ofn.lpstrInitialDir = initDir.empty() ? nullptr : initDir.c_str();
            ofn.lpstrTitle = L"Load Teleport JSON (Heist Farmer / Scout Freecam)";

            if (GetOpenFileNameW(&ofn))
                LoadFromPath(fileBuf);
            else if (CommDlgExtendedError() == 0)
                RefreshStatus();
            else
                g_sStatus = "File dialog error";
        }

        static bool TpActorRaw(SDK::AActor* pActor, float x, float y, float z, float pitch, float yaw)
        {
            __try
            {
                if (!pActor || !pActor->Class || pActor->IsActorBeingDestroyed())
                    return false;
                const SDK::FVector loc{ x, y, z };
                const SDK::FRotator rot{ pitch, yaw, 0.f };
                SDK::FHitResult hit{};
                pActor->K2_SetActorLocationAndRotation(loc, rot, false, &hit, true);
                if (pActor->IsA(SDK::ACharacter::StaticClass()))
                {
                    auto* pChar = reinterpret_cast<SDK::ACharacter*>(pActor);
                    if (pChar->CharacterMovement)
                        pChar->CharacterMovement->Velocity = SDK::FVector{};
                }
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        // AI heisters + other players — tight cluster on the spot (train carts / hide spots).
        static int TeleportAlliesAround(float baseX, float baseY, float baseZ, float pitch, float yaw)
        {
            if (!s_pWorld)
                return 0;

            int n = 0;
            int slot = 0;

            auto offsetFor = [&](int i) -> SDK::FVector
            {
                // ~28uu ring so they stay inside small volumes with you
                const float ang = (float)(i * (2.0 * M_PI / 6.0));
                const float r = 28.f;
                return SDK::FVector{
                    baseX + std::cos(ang) * r,
                    baseY + std::sin(ang) * r,
                    baseZ + 8.f // tiny lift so they don't clip into floor/tracks
                };
            };

            auto tryTp = [&](SDK::AActor* pActor)
            {
                if (!pActor || pActor == s_pPawn)
                    return;
                if (!pActor->Class || pActor->IsActorBeingDestroyed())
                    return;
                const SDK::FVector o = offsetFor(slot++);
                if (TpActorRaw(pActor, o.X, o.Y, o.Z, pitch, yaw))
                    ++n;
            };

            {
                SDK::TArray<SDK::AActor*> crew{};
                SDK::UGameplayStatics::GetAllActorsOfClass(
                    s_pWorld, SDK::ASBZAICrewCharacter::StaticClass(), &crew);
                for (int i = 0; i < crew.Num(); ++i)
                    tryTp(crew[i]);
            }
            {
                SDK::TArray<SDK::AActor*> players{};
                SDK::UGameplayStatics::GetAllActorsOfClass(
                    s_pWorld, SDK::ASBZPlayerCharacter::StaticClass(), &players);
                for (int i = 0; i < players.Num(); ++i)
                {
                    auto* p = reinterpret_cast<SDK::ASBZPlayerCharacter*>(players[i]);
                    if (!p || p == s_pPawn)
                        continue;
                    if (p->IsLocallyControlled())
                        continue;
                    tryTp(p);
                }
            }

            return n;
        }

        // bBringAllies: false = you only; true = you + AI + other players at this spot
        static bool TeleportToIndex(int idx, bool bBringAllies)
        {
            if (s_vecSpots.empty() || !s_pPawn)
                return false;
            if (idx < 0 || idx >= (int)s_vecSpots.size())
                return false;

            const Spot_t& spot = s_vecSpots[idx];
            if (!TpActorRaw(s_pPawn, spot.x, spot.y, spot.z, spot.pitch, spot.yaw))
            {
                g_sStatus = "TP FAIL — enter heist / have pawn";
                return false;
            }

            __try
            {
                if (s_pCtrl)
                    s_pCtrl->SetControlRotation(SDK::FRotator{ spot.pitch, spot.yaw, 0.f });
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }

            s_iLastCrewTp = 0;
            if (bBringAllies)
                s_iLastCrewTp = TeleportAlliesAround(spot.x, spot.y, spot.z, spot.pitch, spot.yaw);

            s_iIndex = idx;
            RefreshStatus();
            return true;
        }
    }

    bool NeedsPlayerTick()
    {
        return s_bTour || s_bPendingTpHere || s_bPendingTpEveryone || s_bPendingDialog;
    }

    void RequestLoadDialog()
    {
        s_bPendingDialog = true;
    }

    void PrevSpot()
    {
        if (s_vecSpots.empty())
            return;
        s_bTour = false;
        s_iIndex = (s_iIndex - 1 + (int)s_vecSpots.size()) % (int)s_vecSpots.size();
        RefreshStatus();
    }

    void NextSpot()
    {
        if (s_vecSpots.empty())
            return;
        s_bTour = false;
        s_iIndex = (s_iIndex + 1) % (int)s_vecSpots.size();
        RefreshStatus();
    }

    void TeleportHere()
    {
        if (s_vecSpots.empty())
        {
            g_sStatus = "No JSON loaded — click Load JSON";
            return;
        }
        s_bTour = false;
        s_bPendingTpEveryone = false;
        s_bPendingTpHere = true;
        s_iPendingTpIndex = s_iIndex;
    }

    void TeleportEveryone()
    {
        if (s_vecSpots.empty())
        {
            g_sStatus = "No JSON loaded — click Load JSON";
            return;
        }
        s_bTour = false;
        s_bPendingTpHere = false;
        s_bPendingTpEveryone = true;
        s_iPendingTpIndex = s_iIndex;
    }

    void StartTourSpots()
    {
        if (s_vecSpots.empty())
        {
            g_sStatus = "No JSON loaded — click Load JSON";
            return;
        }
        s_bPendingTpHere = false;
        s_bPendingTpEveryone = false;
        s_bTour = true;
        s_iIndex = 0;
        s_timeNextTp = std::chrono::steady_clock::now();
        RefreshStatus();
    }

    void StopTour()
    {
        s_bTour = false;
        RefreshStatus();
    }

    void PollPendingUi()
    {
        if (!s_bPendingDialog)
            return;
        s_bPendingDialog = false;
        DoOpenDialog();
    }

    void OnPlayerControllerTick(
        SDK::UWorld* pGWorld,
        SDK::ASBZPlayerController* pLocalController,
        SDK::ASBZPlayerCharacter* pLocalPlayer)
    {
        s_pWorld = pGWorld;
        s_pCtrl = pLocalController;
        s_pPawn = pLocalPlayer;

        if (s_bPendingTpHere)
        {
            s_bPendingTpHere = false;
            TeleportToIndex(s_iPendingTpIndex >= 0 ? s_iPendingTpIndex : s_iIndex, false);
        }

        if (s_bPendingTpEveryone)
        {
            s_bPendingTpEveryone = false;
            TeleportToIndex(s_iPendingTpIndex >= 0 ? s_iPendingTpIndex : s_iIndex, true);
        }

        if (!s_bTour || s_vecSpots.empty())
            return;

        const auto now = std::chrono::steady_clock::now();
        if (now < s_timeNextTp)
            return;

        // Tour = you only, every spot
        if (!TeleportToIndex(s_iIndex, false))
        {
            s_bTour = false;
            return;
        }

        if (s_iIndex + 1 >= (int)s_vecSpots.size())
        {
            s_bTour = false;
            g_sStatus = s_sFileName + " | TOUR done (" + std::to_string(s_vecSpots.size()) + " spots)";
            return;
        }

        ++s_iIndex;
        const auto ms = (int)(s_flDelaySec * 1000.f);
        s_timeNextTp = now + std::chrono::milliseconds(ms > 100 ? ms : 100);
        RefreshStatus();
    }

    void DrawTab()
    {
        ImGui::TextWrapped("Load a Heist Farmer / Scout Freecam .json.");
        ImGui::TextDisabled("TP Here = you | TP All = everyone to selected | Tour = you through spots | Double-click = TP you");
        if (ImGui::Button("Load JSON"))
            RequestLoadDialog();
        ImGui::SameLine();
        if (ImGui::Button("Prev"))
            PrevSpot();
        ImGui::SameLine();
        if (ImGui::Button("Next"))
            NextSpot();

        if (ImGui::Button("TP Here"))
            TeleportHere();
        ImGui::SameLine();
        if (ImGui::Button("TP All"))
            TeleportEveryone();
        ImGui::SameLine();
        if (ImGui::Button("Tour Spots"))
            StartTourSpots();
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
            StopTour();

        ImGui::SliderFloat("Tour delay (sec)", &s_flDelaySec, 0.3f, 5.0f, "%.1f");

        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.9f, 1.f, 1.f));
        ImGui::TextWrapped("%s", g_sStatus.c_str());
        ImGui::PopStyleColor();

        if (!s_vecSpots.empty())
        {
            ImGui::Separator();
            ImGui::Text("Spots (%d)  — double-click to TP you", (int)s_vecSpots.size());
            if (ImGui::BeginListBox("##SpotList", ImVec2(-1.f, 180.f)))
            {
                for (int i = 0; i < (int)s_vecSpots.size(); ++i)
                {
                    const bool selected = (i == s_iIndex);
                    const std::string label = std::to_string(i + 1) + ". " + s_vecSpots[i].name;
                    if (ImGui::Selectable(label.c_str(), selected))
                    {
                        s_bTour = false;
                        s_iIndex = i;
                        s_iLastCrewTp = 0;
                        RefreshStatus();
                    }
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        s_bTour = false;
                        s_iIndex = i;
                        TeleportHere();
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndListBox();
            }
        }
    }
}
