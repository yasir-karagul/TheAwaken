#include "SessionHandshakeFramework/NullPlatformAuthProvider.h"

FString UNullPlatformAuthProvider::getLocalAccountId() const{
    return localAccountId;
}

void UNullPlatformAuthProvider::requestLocalAuthToken(TFunction<void(const FString& authToken)> onDone){
    platformCallCount++;
    if(neverAnswers){
        return;
    }
    onDone(TEXT("null-token"));
}

void UNullPlatformAuthProvider::verifyPeerOwnership(const FString& peerId, const FS_PeerAuthToken& claim, TFunction<void(const FS_OwnershipResult& ownership)> onDone){
    platformCallCount++;
    if(neverAnswers){
        return;
    }
    FS_OwnershipResult ownership;
    ownership.verified = verifiesOwnership;
    ownership.accountId = !verifiesOwnership ? FString() : (accountIdOverride.IsEmpty() ? peerId : accountIdOverride);
    ownership.store = E_StoreKind::None;
    onDone(ownership);
}

void UNullPlatformAuthProvider::queryDisplayName(const FString& peerId, TFunction<void(const FString& displayName)> onDone){
    platformCallCount++;
    if(neverAnswers){
        return;
    }
    onDone(TEXT("TestPlayer-") + peerId.Right(4));
}
