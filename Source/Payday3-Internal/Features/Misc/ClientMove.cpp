#include "ClientMove.hpp"
#include "../../Menu.hpp"

#undef min
#undef max

namespace {
    using Clock = std::chrono::steady_clock;

    enum class EState{
        Disabled,
        Active,
        TeleportStart,
        TeleportEnd
    };

    SDK::ASBZPlayerCharacter* pLocalPlayer{};
    SDK::USBZPlayerMovementComponent* pMovementComponent{};
    EState eState = EState::Disabled;
    EState eLastState = EState::Disabled;
    uint32_t iTicksFromState = 0;
    Clock::time_point timeEnteredState = Clock::now();

    bool AreRoomsSynced(){
        return pLocalPlayer->GetCurrentRoom_Implementation() == pLocalPlayer->GetLastKnownRoom();
    };

    void ChangeState(EState eNewState){
        eState = eNewState;
        timeEnteredState = Clock::now();
    };

    void TeleportPlayer(bool bChangeState = true){
        // Solo: client already owns position — just stay put (no server desync to fix).
        // Online: fake traversal RPC tells the server "you're here" after noclip.
        if (!Cheat::g_bIsSoloGame && pMovementComponent)
        {
            SDK::FVector vecPoint = pLocalPlayer->K2_GetActorLocation();
            SDK::FSBZMinimalAgilityTraversalTrajectory trajectory{
                vecPoint, vecPoint, vecPoint, vecPoint, std::numeric_limits<int16_t>::max(),
                SDK::ESBZAgilityTraversalType::VaultLowFast, false, false
            };
            pMovementComponent->Server_StartTraversal(trajectory);
        }

        if (bChangeState)
            ChangeState(EState::TeleportStart);
    };

    void EnableNoclipMovement(){
        if (pLocalPlayer->GetActorEnableCollision())
            pLocalPlayer->SetActorEnableCollision(false);

        pMovementComponent->MovementMode = SDK::EMovementMode::MOVE_Flying;
        pMovementComponent->BrakingDecelerationFlying = 10000.f;
        pMovementComponent->MaxFlySpeed = 10000.f;
        float flFlySpeed = CheatConfig::Get().m_misc.m_flClientMoveBaseSpeed;
        if(CheatConfig::Get().m_misc.m_keyClientMoveFaster.GetState())
            flFlySpeed *= 2.f;
        pMovementComponent->Velocity = pMovementComponent->GetLastInputVector() * flFlySpeed;
    };

    void DisableNoclipMovement(){
        pLocalPlayer->SetActorEnableCollision(true);
        pMovementComponent->MovementMode = SDK::EMovementMode::MOVE_Walking;
        pMovementComponent->Velocity = SDK::FVector{};
    };
};

bool Cheat::ClientMove::ShouldSendMovePacket(){
    if(eState != eLastState){
        iTicksFromState = 0;
        eLastState = eState;
    }

    ++iTicksFromState;
    return eState != EState::Active && eState != EState::TeleportStart;
};

void Cheat::ClientMove::OnPlayerControllerTick(SDK::ASBZPlayerCharacter* _pLocalPlayer, SDK::USBZPlayerMovementComponent* _pMovementComponent){
    pLocalPlayer = _pLocalPlayer;
    pMovementComponent = _pMovementComponent;

    auto timeNow = Clock::now();
    bool bKeyActive = CheatConfig::Get().m_misc.m_keyClientMove.GetState();
    Cheat::g_bIsDesynced = eState != EState::Disabled;
    if(eState == EState::TeleportStart && timeNow - timeEnteredState > g_durationPing && iTicksFromState){
        ChangeState(EState::TeleportEnd);
        TeleportPlayer(false);
    }
        
    if(eState == EState::TeleportEnd && timeNow - timeEnteredState > g_durationPing && iTicksFromState)
        ChangeState(bKeyActive ? EState::Active : EState::Disabled);
    
    if(bKeyActive){
        EnableNoclipMovement();
        if(eState == EState::Disabled)
            ChangeState(EState::Active);

        // One-shot on key press (Hold GetState() would re-fire every tick after sync).
        if (eState == EState::Active && CheatConfig::Get().m_misc.m_keyClientMoveTeleport.Pressed())
            TeleportPlayer();

        return;
    }
    
    if(!pLocalPlayer->GetActorEnableCollision())
        DisableNoclipMovement();

    if(eState != EState::Active)
        return;
        
    ChangeState(EState::Disabled);

    if(CheatConfig::Get().m_misc.m_bClientMoveAutoTeleport)
        TeleportPlayer();
}