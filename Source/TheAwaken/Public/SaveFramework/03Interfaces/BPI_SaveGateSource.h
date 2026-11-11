#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "GameFramework/PlayerState.h"
#include "BPI_SaveGateSource.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UBPI_SaveGateSource : public UInterface{
	GENERATED_BODY()
};

class THEAWAKEN_API IBPI_SaveGateSource{
	GENERATED_BODY()
public:
	/** Server only. The requester asked to save. Return a refusal tag (Interaction.FailReason.*), or an invalid tag to allow.
	 *  B12: isPlayerDetected -> EnemyAware.  B02: Downed / Restrained / Bagless.  B06 or B16: host restore not finished. */
	UFUNCTION(BlueprintNativeEvent, Category = "Save") FGameplayTag getSaveRefusalReason(APlayerState* requester) const;

	/** Server only. True = this connected player keeps the previous Character file (Bagless, Downed, Restrained: Doc R13 3.3). */
	UFUNCTION(BlueprintNativeEvent, Category = "Save") bool shouldKeepPreviousCharacterFile(APlayerState* player) const;
};