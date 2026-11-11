#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BPI_SavePlayerSubject.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UBPI_SavePlayerSubject : public UInterface{
	GENERATED_BODY()
};

class THEAWAKEN_API IBPI_SavePlayerSubject{
	GENERATED_BODY()
public:
	/** Doc B05 3.5: platform account id, else the cached platform id, else the local id. */
	UFUNCTION(BlueprintNativeEvent, Category = "Save") FString getPlayerIdentity() const;
	/** The cached platform id and the local id, so a file is found by either (Doc R13 3.10). */
	UFUNCTION(BlueprintNativeEvent, Category = "Save") void getLinkedPlayerIds(TArray<FString>& linkedIds) const;
	/** New unique id per character; owner of every bag the character drops (Doc R13 3.10). */
	UFUNCTION(BlueprintNativeEvent, Category = "Save") FGuid getCharacterId() const;
	/** Doc R01: the bag this player holds, or an invalid Guid when Bagless. */
	UFUNCTION(BlueprintNativeEvent, Category = "Save") FGuid getHeldBagId() const;
	/** COMMAND, server only. Save calls it from beginCharacterRestore (Step 14): a new character's id, or the id the file stored. */
	UFUNCTION(BlueprintNativeEvent, Category = "Save") void setCharacterId(FGuid newCharacterId);
};