#include "SessionHandshakeFramework/SteamAuthProvider.h"

// SKELETON (guide step 11). Every function fails closed. Replace each TODO with the SDK calls listed in the step 11 table.
// Rules for every body: call back on the game thread, call back exactly once (or never), and return an id the provider verified itself.
// Add the Steam / EOS plugin modules to Build.cs only when you start filling these in.

E_StoreKind USteamAuthProvider::getStoreKind() const{
    return E_StoreKind::Steam;
}

FString USteamAuthProvider::getLocalAccountId() const{
    // TODO: the EOS id of the local user, from signing the Steam user in to EOS Connect with a Steam ticket (Doc B05 3.5).
    return FString();
}

void USteamAuthProvider::requestLocalAuthToken(TFunction<void(const FString& authToken)> onDone){
    // TODO: ISteamUser::GetAuthSessionTicket, wait for its response callback, send the ticket as hex text.
    onDone(FString());
}

void USteamAuthProvider::verifyPeerOwnership(const FString& peerId, const FS_PeerAuthToken& claim, TFunction<void(const FS_OwnershipResult& ownership)> onDone){
    // TODO: ISteamUser::BeginAuthSession with the ticket, wait for ValidateAuthTicketResponse_t, then UserHasLicenseForApp.
    // Then map the Steam id to the EOS id (EOS_Connect_QueryExternalAccountMappings, Steam type). accountId is the mapped EOS id.
    onDone(FS_OwnershipResult());   // verified is false: fails closed
}

void USteamAuthProvider::queryDisplayName(const FString& peerId, TFunction<void(const FString& displayName)> onDone){
    // TODO: ISteamFriends::RequestUserInformation, then GetFriendPersonaName.
    onDone(FString());
}

void USteamAuthProvider::releasePeer(const FString& peerId){
    // TODO: ISteamUser::EndAuthSession for that Steam id. Skipping it leaks the auth session.
}
