#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SessionHandshakeFramework/00Enums/E_SessionHandshakeFramework.h"
#include "SessionHandshakeFramework/01Structs/S_SessionHandshakeFramework.h"
#include "PlayerIdentityComponent.generated.h"

UCLASS(ClassGroup = (Awaken), meta = (BlueprintSpawnableComponent))
class THEAWAKEN_API UPlayerIdentityComponent : public UActorComponent{
    GENERATED_BODY()

public:
    UPlayerIdentityComponent();

    // The parameter must be called OutLifetimeProps: the DOREPLIFETIME macros use that exact name (Doc02 20.5).
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** COMMAND, server only. Written once (Doc B05 3.4). Writing the SAME primary id again is a quiet success, because a reconnect can reuse
     *  a PlayerState (Doc B20 3.8). A different id, an empty id, or a call on a client returns false. */
    bool setIdentityServer(const FS_PlayerIdentity& newIdentity);

    /** QUERY. On a client linkedIds is empty. */
    UFUNCTION(BlueprintPure, Category = "Handshake")
    FS_PlayerIdentity getIdentity() const;

    UFUNCTION(BlueprintPure, Category = "Handshake")
    bool hasIdentity() const;

private:
    UFUNCTION()
    void OnRep_primaryId();

    /** Raises Session.PlayerIdentity.Replicated on this machine (Doc02 19.2: local on every machine, raised from replicated state). */
    void raiseIdentityMessage() const;

    UPROPERTY(ReplicatedUsing = OnRep_primaryId)
    FString primaryId;

    UPROPERTY(Replicated)
    E_PlayerIdKind kind = E_PlayerIdKind::LocalFallback;

    /** Server only. */
    UPROPERTY()
    TArray<FString> linkedIds;
};
