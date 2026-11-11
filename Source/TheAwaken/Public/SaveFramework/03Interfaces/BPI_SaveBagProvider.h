#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SaveFramework/01Structs/S_SaveFramework.h"
#include "BPI_SaveBagProvider.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UBPI_SaveBagProvider : public UInterface{
	GENERATED_BODY()
};

class THEAWAKEN_API IBPI_SaveBagProvider{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, Category = "Save") void getBagRecordsForSave(TArray<FS_BagRecord>& bags) const;   // Query
	UFUNCTION(BlueprintNativeEvent, Category = "Save") void restoreBagRecords(const TArray<FS_BagRecord>& bags);   // Command, B06 World step 11
	UFUNCTION(BlueprintNativeEvent, Category = "Save") void removeBagRecord(FGuid bagID);   // Command, saved state wins
	UFUNCTION(BlueprintNativeEvent, Category = "Save") int32 removeBagsOwnedBy(FGuid ownerCharacterID);   // Command, fresh character
};