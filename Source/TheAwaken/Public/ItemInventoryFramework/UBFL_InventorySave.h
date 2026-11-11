#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "UBFL_InventorySave.generated.h"

class UInventoryData;
class UInventoryRegistrySubsystem;

// Helpers shared by every save participant of this module (the R05 guide uses toBytes and fromBytes too).
UCLASS()
class THEAWAKEN_API UBFL_InventorySave : public UBlueprintFunctionLibrary{
    GENERATED_BODY()

public:
    // A struct as a byte blob. Every property is written with its name, so adding a field later does not break old saves.
    static TArray<uint8> toBytes(UScriptStruct* structType, const void* data);
    // False when the blob is empty or cannot be read.
    static bool fromBytes(UScriptStruct* structType, void* data, const TArray<uint8>& bytes);

    // Query. One inventory, and the contents of every Portable Container inside it, as save records.
    static void capture(const UInventoryRegistrySubsystem* registry, const UInventoryData* inventory, bool usesTimeLeft, int64 now, FS_SavedInventory& outInventory, TArray<FS_SavedInventory>& outContained);
    // Command. Puts a saved inventory back into the registry, with its Portable Containers. Entries whose item no longer
    // exists are skipped with one log line (Doc R13 3.11). Null when the record cannot be restored.
    static UInventoryData* restore(UInventoryRegistrySubsystem* registry, const FS_SavedInventory& saved, const TArray<FS_SavedInventory>& containedPool, int64 now);

private:
    static TArray<FS_InventoryEntry> toSavedEntries(const UInventoryData* inventory, bool usesTimeLeft, int64 now);
    static TArray<FS_InventoryEntry> toLiveEntries(const UObject* worldContext, const FS_SavedInventory& saved, int64 now);
};
