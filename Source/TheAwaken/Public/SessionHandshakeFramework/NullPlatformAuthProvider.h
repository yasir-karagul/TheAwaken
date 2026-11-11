#pragma once
#include "CoreMinimal.h"
#include "SessionHandshakeFramework/PlatformAuthProvider.h"
#include "NullPlatformAuthProvider.generated.h"

/** Editor and test stand-in. Counts its calls so a test can prove that offline play made none (Doc B05 3.1). */
UCLASS()
class THEAWAKEN_API UNullPlatformAuthProvider : public UPlatformAuthProvider{
    GENERATED_BODY()

public:
    UPROPERTY(Transient)
    bool verifiesOwnership = true;

    /** True: never answer, so a test can reach Timeout. */
    UPROPERTY(Transient)
    bool neverAnswers = false;

    /** When not empty, ownership returns this id instead of the peer id: a test of the rule that the two must be equal. */
    UPROPERTY(Transient)
    FString accountIdOverride;

    /** Empty means signed out. */
    UPROPERTY(Transient)
    FString localAccountId;

    UPROPERTY(Transient)
    int32 platformCallCount = 0;

    virtual FString getLocalAccountId() const override;
    virtual void requestLocalAuthToken(TFunction<void(const FString& authToken)> onDone) override;
    virtual void verifyPeerOwnership(const FString& peerId, const FS_PeerAuthToken& claim, TFunction<void(const FS_OwnershipResult& ownership)> onDone) override;
    virtual void queryDisplayName(const FString& peerId, TFunction<void(const FString& displayName)> onDone) override;
};
