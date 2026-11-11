#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "SessionHandshakeFramework/01Structs/S_SessionHandshakeFramework.h"
#include "BPI_JoinGateSource.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UBPI_JoinGateSource : public UInterface{
    GENERATED_BODY()
};

class THEAWAKEN_API IBPI_JoinGateSource{
    GENERATED_BODY()

public:
    /** Host only. Runs after ownership has verified the id and before the build check (Doc B20 3.4 step 5).
     *  Return a refusal tag (Session.JoinFailReason.Banned and so on), or an invalid tag to let the peer through.
     *  It must answer at once and never wait: the handshake does not time this step. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Handshake")
    FGameplayTag evaluateJoin(const FS_PlayerIdentity& identity) const;
};
