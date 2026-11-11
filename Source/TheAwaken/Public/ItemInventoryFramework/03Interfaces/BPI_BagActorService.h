#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "BPI_BagActorService.generated.h"

UINTERFACE(BlueprintType)
class UBPI_BagActorService : public UInterface{
    GENERATED_BODY()
};

class IBPI_BagActorService{
    GENERATED_BODY()

public:
    // Step 1 of DetachBag: an empty record and its actor at the location (Doc B02 3.6). False on failure.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool createBag(FGuid bagId, FGuid ownerCharacterId, FVector location);

    // The last step of DetachBag: what the bag holds. Cannot fail.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void setBagContents(FGuid bagId, const FS_BagContents& contents);

    // isValid is false when there is no such bag.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    FS_BagInfo findBag(FGuid bagId);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    FS_BagInfo findBagOwnedBy(FGuid ownerCharacterId);

    // Removes the record and the actor. The inventory is NOT touched: R01 owns it.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void deleteBag(FGuid bagId);

    // Fresh character (R01 3.8): removes every bag of that character and returns the inventory id of each,
    // so that R01 can destroy those inventories (a request of the R01 spec to Doc B02).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    TArray<FGuid> removeBagsOwnedBy(FGuid ownerCharacterId);
};
