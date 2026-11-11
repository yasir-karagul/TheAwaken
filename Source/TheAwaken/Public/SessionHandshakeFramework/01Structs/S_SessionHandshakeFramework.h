#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SessionHandshakeFramework/00Enums/E_SessionHandshakeFramework.h"
#include "S_SessionHandshakeFramework.generated.h"

class APlayerState;

/** Doc B05 6. Both fields are required. Start build numbers at 1: 0 counts as missing. */
USTRUCT(BlueprintType)
struct FS_PeerBuildInfo{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 buildNumber = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString contentHash;

    bool isComplete() const{
        return buildNumber > 0 && !contentHash.IsEmpty();
    }
};

/** The key of a Character save (Doc R13 3.10) and the ids that find the same person's file on the host machine. */
USTRUCT(BlueprintType)
struct FS_PlayerIdentity{
    GENERATED_BODY()

    /** Never empty for an admitted player. */
    UPROPERTY(BlueprintReadWrite)
    FString primaryId;

    UPROPERTY(BlueprintReadWrite)
    E_PlayerIdKind kind = E_PlayerIdKind::LocalFallback;

    /** The cached platform id and the local id, when they belong to the same person (Doc R13 3.10). Host side only, never replicated. */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> linkedIds;

    bool isValid() const{
        return !primaryId.IsEmpty();
    }

    /** The lookup matches either id (Doc B05 3.5). Ids are compared without case: file names on Windows ignore case. */
    bool matchesId(const FString& id) const{
        if(id.IsEmpty()){
            return false;
        }
        if(primaryId.Equals(id, ESearchCase::IgnoreCase)){
            return true;
        }
        for(const FString& linkedId : linkedIds){
            if(linkedId.Equals(id, ESearchCase::IgnoreCase)){
                return true;
            }
        }
        return false;
    }

    /** Primary first, then the linked ids, no duplicates: the list a file lookup walks. */
    void getLookupIds(TArray<FString>& ids) const{
        ids.Reset();
        if(!primaryId.IsEmpty()){
            ids.Add(primaryId);
        }
        for(const FString& linkedId : linkedIds){
            if(!linkedId.IsEmpty()){
                ids.AddUnique(linkedId);
            }
        }
    }
};

/** What a platform provider reports after checking one peer (Doc B05 3.2). */
USTRUCT()
struct FS_OwnershipResult{
    GENERATED_BODY()

    UPROPERTY()
    bool verified = false;

    /** The account id the PROVIDER verified: the Epic Online Services id for both stores (Doc B05 3.5). Never copied from the claim. */
    UPROPERTY()
    FString accountId;

    UPROPERTY()
    E_StoreKind store = E_StoreKind::None;
};

/** Doc B05 6: allowed, failReasonTag, platformDisplayName. PROPOSED additions: peerId, identity, returningLookup. */
USTRUCT(BlueprintType)
struct FS_JoinValidationResult{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString peerId;

    UPROPERTY(BlueprintReadOnly)
    bool allowed = false;

    UPROPERTY(BlueprintReadOnly)
    FGameplayTag failReasonTag;

    UPROPERTY(BlueprintReadOnly)
    FString platformDisplayName;

    UPROPERTY(BlueprintReadOnly)
    FS_PlayerIdentity identity;

    UPROPERTY(BlueprintReadOnly)
    E_ReturningLookup returningLookup = E_ReturningLookup::NotChecked;
};

/** A passed check waiting for the engine connection (step 10). Single use. Real time, never the game clock (Doc02 19.4). */
USTRUCT()
struct FS_AdmissionRecord{
    GENERATED_BODY()

    UPROPERTY()
    FS_JoinValidationResult result;

    UPROPERTY()
    double expiresAt = 0.0;
};

/** The token a guest sent with its claim. Kept only until the ownership check ends. */
USTRUCT()
struct FS_PeerAuthToken{
    GENERATED_BODY()

    UPROPERTY()
    FString token;

    /** The Steam id (or Epic account id) the guest says the token belongs to. The provider needs it to check the token and must verify it (step 11). */
    UPROPERTY()
    FString storeAccountId;

    UPROPERTY()
    E_StoreKind store = E_StoreKind::None;
};

/** The small local file of step 8 (Doc B05 3.5). */
USTRUCT()
struct FS_IdentityCache{
    GENERATED_BODY()

    /** Created once. Marked with the settings prefix so it cannot be confused with a platform id. */
    UPROPERTY()
    FString localId;

    /** The last platform account id seen on this machine. Offline play uses it. */
    UPROPERTY()
    FString cachedPlatformId;

    /** The platform id the local id was first linked to. Empty until the first co-op host or join with a platform account. */
    UPROPERTY()
    FString localIdLinkedTo;
};

/** Payload of Session.PeerJoinAccepted (Doc B05 8). */
USTRUCT(BlueprintType)
struct FS_PeerJoinAcceptedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString peerId;

    UPROPERTY(BlueprintReadOnly)
    FString platformDisplayName;
};

/** Payload of Session.PeerJoinRejected (Doc B05 8). */
USTRUCT(BlueprintType)
struct FS_PeerJoinRejectedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString peerId;

    UPROPERTY(BlueprintReadOnly)
    FGameplayTag failReasonTag;

    /** False when the host raises the message (nobody there should see a join-failed screen). True when the joining machine raises it locally
     *  (Doc B05 8: server, then local to the owning player). Doc B17's screen shows only where this is true. PROPOSED field. */
    UPROPERTY(BlueprintReadOnly)
    bool localToRejectedPeer = false;
};

/** PROPOSED local message Session.PlayerIdentity.Replicated: raised on every machine when a PlayerState's identity is known (Doc02 19.2). */
USTRUCT(BlueprintType)
struct FS_PlayerIdentityReplicatedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<APlayerState> playerState;

    UPROPERTY(BlueprintReadOnly)
    FString primaryId;

    UPROPERTY(BlueprintReadOnly)
    E_PlayerIdKind kind = E_PlayerIdKind::LocalFallback;
};
