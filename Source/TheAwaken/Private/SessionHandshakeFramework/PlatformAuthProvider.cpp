#include "SessionHandshakeFramework/PlatformAuthProvider.h"

E_StoreKind UPlatformAuthProvider::getStoreKind() const{
    return E_StoreKind::None;
}

FString UPlatformAuthProvider::getLocalAccountId() const{
    return FString();
}

void UPlatformAuthProvider::requestLocalAuthToken(TFunction<void(const FString& authToken)> onDone){
    onDone(FString());
}

void UPlatformAuthProvider::verifyPeerOwnership(const FString& peerId, const FS_PeerAuthToken& claim, TFunction<void(const FS_OwnershipResult& ownership)> onDone){
    onDone(FS_OwnershipResult());   // verified is false
}

void UPlatformAuthProvider::queryDisplayName(const FString& peerId, TFunction<void(const FString& displayName)> onDone){
    onDone(FString());
}

void UPlatformAuthProvider::releasePeer(const FString& peerId){
}
