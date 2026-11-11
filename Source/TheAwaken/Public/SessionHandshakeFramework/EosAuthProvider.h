#pragma once
#include "CoreMinimal.h"
#include "SessionHandshakeFramework/PlatformAuthProvider.h"
#include "EosAuthProvider.generated.h"

/** SKELETON. Every body in the .cpp fails closed until you fill in the EOS SDK calls (guide step 11). */
UCLASS()
class THEAWAKEN_API UEosAuthProvider : public UPlatformAuthProvider{
    GENERATED_BODY()

public:
    virtual E_StoreKind getStoreKind() const override;
    virtual FString getLocalAccountId() const override;
    virtual void requestLocalAuthToken(TFunction<void(const FString& authToken)> onDone) override;
    virtual void verifyPeerOwnership(const FString& peerId, const FS_PeerAuthToken& claim, TFunction<void(const FS_OwnershipResult& ownership)> onDone) override;
    virtual void queryDisplayName(const FString& peerId, TFunction<void(const FString& displayName)> onDone) override;
    virtual void releasePeer(const FString& peerId) override;
};
