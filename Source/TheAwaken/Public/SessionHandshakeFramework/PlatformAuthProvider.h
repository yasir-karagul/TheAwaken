#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SessionHandshakeFramework/00Enums/E_SessionHandshakeFramework.h"
#include "SessionHandshakeFramework/01Structs/S_SessionHandshakeFramework.h"
#include "PlatformAuthProvider.generated.h"

UCLASS(Abstract)
class THEAWAKEN_API UPlatformAuthProvider : public UObject{
    GENERATED_BODY()

public:
    virtual E_StoreKind getStoreKind() const;

    /** The Epic Online Services id of the signed-in local user, or empty when signed out. Query, answers at once from what the SDK already knows. */
    virtual FString getLocalAccountId() const;

    /** Guest side. Produce the proof the host will check: a Steam auth ticket or an Epic id token, as text. Empty text means it could not. */
    virtual void requestLocalAuthToken(TFunction<void(const FString& authToken)> onDone);

    /** Host side. Check one peer (Doc B05 3.2). The default fails at once.
     *  The claim holds the token, the store and the store id the guest says it has. The id returned must be the Epic Online Services id and must equal peerId. */
    virtual void verifyPeerOwnership(const FString& peerId, const FS_PeerAuthToken& claim, TFunction<void(const FS_OwnershipResult& ownership)> onDone);

    /** Host side. The platform persona name of a peer or of the local user (Doc B05 3.4). Empty text means it could not. */
    virtual void queryDisplayName(const FString& peerId, TFunction<void(const FString& displayName)> onDone);

    /** Host side. The peer left or was refused: end the SDK session for it (Steam needs EndAuthSession). Safe to call twice. */
    virtual void releasePeer(const FString& peerId);
};
