#include "pch.h"
#include "AimbotHooks.hpp"
#include "../Features/Aimbot/Aimbot.hpp"

#define ULOCALPLAYER_GETVIEWPOINT_INDEX 0x50
#define APLAYERCONTROLLER_GETPLAYERVIEWPOINT_INDEX 0xED

static SDK::FVector g_vecOriginalLocation{};
static SDK::FRotator g_rotOriginalRotation{};
bool g_bAttemptedToShoot = false;

static Memory::Hook<void(*)(SDK::ULocalPlayer*, SDK::FMinimalViewInfo*)> oLocalPlayerGetViewPoint;

static void hkLocalPlayerGetViewPoint(SDK::ULocalPlayer* pLocalPlayer, SDK::FMinimalViewInfo* pViewInfo)
{
	oLocalPlayerGetViewPoint(pLocalPlayer, pViewInfo);

	if (!Framework::bShouldRun || Framework::bProcessExiting)
		return;

	if (pAimbot->CurrentAimbotType() == 0)
		return;

	if (!pAimbot->ShouldOverrideView)
		return;

	pViewInfo->Location = g_vecOriginalLocation;
	pViewInfo->Rotation = g_rotOriginalRotation;
}

static Memory::Hook<void(*)(SDK::APlayerController*, SDK::FVector*, SDK::FRotator*)> oGetPlayerViewPoint;

static void hkGetPlayerViewPoint(SDK::APlayerController* pPlayerController, SDK::FVector* pLocation, SDK::FRotator* pRotation)
{
	static std::vector<std::pair<uintptr_t, uint64_t>> vecHackyShitVector{};
	static uintptr_t pGoalRet1 = 0;
	static uintptr_t pGoalRet2 = 0;
	oGetPlayerViewPoint(pPlayerController, pLocation, pRotation);

	if (!Framework::bShouldRun || Framework::bProcessExiting)
		return;

	if(!pAimbot->ShouldOverrideView){
		vecHackyShitVector.clear();
		return;
	}
	g_vecOriginalLocation = *pLocation;
	g_rotOriginalRotation = *pRotation;

	auto pReturnAddress = reinterpret_cast<uintptr_t>(_ReturnAddress());
	if(!pGoalRet1 || !pGoalRet2){
		bool bFound = false;
		for(size_t i = 0; i < vecHackyShitVector.size(); ++i){
			if(vecHackyShitVector[i].first == pReturnAddress){
				vecHackyShitVector[i].second++;
				bFound = true;
				break;
			}
		}

		if(!bFound)
			vecHackyShitVector.emplace_back(std::pair<uintptr_t, uint64_t>{ pReturnAddress, 1 });

		if(g_bAttemptedToShoot){
			if(vecHackyShitVector.size() > 5){
				std::pair<uintptr_t, uint64_t> pairs[3]{};

				for(size_t i = 0; i < vecHackyShitVector.size(); ++i){
					if(vecHackyShitVector[i].second < pairs[0].second || !pairs[0].first){
						pairs[2] = pairs[1];
						pairs[1] = pairs[0];
						pairs[0] = vecHackyShitVector[i];
						continue;
					}

					if(vecHackyShitVector[i].second < pairs[1].second || !pairs[1].first){
						pairs[2] = pairs[1];
						pairs[1] = vecHackyShitVector[i];
						continue;
					}

					if(vecHackyShitVector[i].second < pairs[2].second || !pairs[2].first){
						pairs[2] = vecHackyShitVector[i];
						continue;
					}
				}

				if(pairs[2].second > 6000 && pairs[1].second - pairs[0].second < pairs[2].second - pairs[1].second){
					pGoalRet1 = pairs[0].first;
					pGoalRet2 = pairs[1].first;
					vecHackyShitVector.clear();

					std::cout << "pAimbotFixup\n";
				}
			}
		}

		g_bAttemptedToShoot = false;
	}
	else if(pGoalRet1 != pReturnAddress && pGoalRet2 != pReturnAddress)
		return;

	if (!pAimbot->CurrentAimbotType() == 0)
		return;

	if (!pAimbot->ShouldOverrideView)
		return;

	*pLocation = pAimbot->OverrideLocation;
	*pRotation = pAimbot->OverrideRotation;
}

bool AimbotHooks::Setup()
{
	SDK::UWorld* pWorld = SDK::UWorld::GetWorld();
	if (!pWorld)
		return false;

	SDK::APlayerController* pPlayerController = SDK::UGameplayStatics::GetPlayerController(pWorld, 0);

	if (!pPlayerController)
		return false;

	void* pPlayerViewPointFunction = SDK::InSDKUtils::GetVirtualFunction<void*>(pPlayerController, APLAYERCONTROLLER_GETPLAYERVIEWPOINT_INDEX);

	if (!pPlayerViewPointFunction)
	{
		Utils::LogError("Failed to find APlayerController::GetPlayerViewPoint");

		return false;
	}

	MH_STATUS status = oGetPlayerViewPoint.Start(hkGetPlayerViewPoint, pPlayerController, APLAYERCONTROLLER_GETPLAYERVIEWPOINT_INDEX);

	if (status != MH_OK)
	{
		Utils::LogHook("APlayerController::GetPlayerViewPoint", status);

		return false;
	}

	SDK::ULocalPlayer* pLocalPlayer = reinterpret_cast<SDK::ULocalPlayer*>(pPlayerController->Player);

	if (!pLocalPlayer)
	{
		Utils::LogError("Failed to find ULocalPlayer");

		oGetPlayerViewPoint.Remove();
		return false;
	}

	void* pLocalPlayerViewPointFunction = SDK::InSDKUtils::GetVirtualFunction<void*>(pLocalPlayer, ULOCALPLAYER_GETVIEWPOINT_INDEX);

	if (!pLocalPlayerViewPointFunction)
	{
		Utils::LogError("Failed to find ULocalPlayer::GetViewPoint");

		oGetPlayerViewPoint.Remove();
		return false;
	}

	status = oLocalPlayerGetViewPoint.Start(hkLocalPlayerGetViewPoint, pLocalPlayer, ULOCALPLAYER_GETVIEWPOINT_INDEX);

	if (status != MH_OK)
	{
		Utils::LogHook("ULocalPlayer::GetViewPoint", status);

		oGetPlayerViewPoint.Remove();
		return false;
	}

	Utils::LogDebug("AimbotHooks initialized successfully");

	return true;
}

void AimbotHooks::Destroy()
{
	oLocalPlayerGetViewPoint.Remove();
	oGetPlayerViewPoint.Remove();

	Utils::LogDebug("AimbotHooks removed.");
}