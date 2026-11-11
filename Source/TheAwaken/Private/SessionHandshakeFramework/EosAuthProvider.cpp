#include "SessionHandshakeFramework/EosAuthProvider.h"

// SKELETON (guide step 11). Every function fails closed. Replace each TODO with the SDK calls listed in the step 11 table.
// Rules for every body: call back on the game thread, call back exactly once (or never), and return an id the provider verified itself.
// The Epic ownership proof is still open in review B05-02: decide it before shipping on the Epic store.

E_StoreKind UEosAuthProvider::getStoreKind() const{
    return E_StoreKind::Epic;
}

FString UEosAuthProvider::getLocalAccountId() const{
    // TODO: the EOS Connect id of the signed-in local user.
    return FString();
}

void UEosAuthProvider::requestLocalAuthToken(TFunction<void(const FString& authToken)> onDone){
    // TODO: the local user's Connect ID token (EOS_Connect_CopyIdToken), as text.
    onDone(FString());
}

void UEosAuthProvider::verifyPeerOwnership(const FString& peerId, const FS_PeerAuthToken& claim, TFunction<void(const FS_OwnershipResult& ownership)> onDone){
    // TODO: EOS_Connect_VerifyIdToken on the host, plus an entitlement proof from the guest (for example EOS_Ecom_QueryOwnershipToken).
    // accountId is the verified Connect id.
    onDone(FS_OwnershipResult());   // verified is false: fails closed
}

void UEosAuthProvider::queryDisplayName(const FString& peerId, TFunction<void(const FString& displayName)> onDone){
    // TODO: map the Connect id to an Epic account id, then EOS_UserInfo_QueryUserInfo and EOS_UserInfo_CopyUserInfo.
    onDone(FString());
}

void UEosAuthProvider::releasePeer(const FString& peerId){
    // Nothing to end for a verified token; clear any cached state here.
}
