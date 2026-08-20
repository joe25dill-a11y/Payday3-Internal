#pragma once

#include "../Dumper-7/SDK.hpp"

namespace FNames{
    #define ForEachFName(fn) \
        fn(ServerMovePacked) \
        fn(GA_Fire_C) \
        fn(K2_CommitExecute) \
        fn(ClientPlayForceFeedback_Internal) \
        fn(ServerTryActivateAbility) \
        fn(OnActionPressed) \
        fn(OnActionReleased) \
        fn(MaskOn) \
        fn(ReceiveTick) \
        fn(Server_ThrowBag) \
        fn(CH_PlayerBase_C) \
        fn(KismetStringLibrary) \
        fn(SBZActionInputWidget) \
        fn(SBZKeypad) \
        fn(SBZCookingStation) \
        fn(BP_QRPhone_C) \
        fn(BP_BlueKeycard_C) \
        fn(BP_RedKeycard_C) \
        fn(BP_RFIDTagBlue_C) \
        fn(BP_DAT_C4Explosive_01_Pickup_C) \
        fn(ClientMoveResponsePacked) \
        fn(BP_Meth_CausticSoda_C) \
        fn(BP_Meth_MuriaticAcid_C) \
        fn(BP_Meth_HydrogenChloride_C) \
        fn(BP_FOR_USBDrive_C) \
        fn(BP_Plankspile_C) \
        fn(BP_CheckRequirement) \
        fn(Client_OnPickupCarryActorFailed) \
        fn(Multicast_OnPickupCarryActor) \
        fn(Multicast_OnThrowCarryActor) \
    
    #define DefineFName(name) inline SDK::FName name{};
    ForEachFName(DefineFName)
    #undef DefineFName


    inline bool g_bIsFullyInitialized = false;
    inline void Initialize(){
        if(g_bIsFullyInitialized)
            return;

        bool bHasInvalid = false;
        auto fnInit = [&bHasInvalid](SDK::FName& name, const wchar_t* wsz) -> void {
            if(!name.IsNone())
                return;

            name = SDK::UKismetStringLibrary::Conv_StringToName(wsz);
            if(name.IsNone())
                bHasInvalid = true;
        };

        #define CreateFName(name) fnInit(name, L###name);
        ForEachFName(CreateFName)
        #undef CreateFName

        g_bIsFullyInitialized = !bHasInvalid;
    };

    #undef ForEachFName
};
