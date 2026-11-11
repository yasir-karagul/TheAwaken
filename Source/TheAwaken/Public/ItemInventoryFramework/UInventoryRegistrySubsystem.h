#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "SaveFramework/03Interfaces/BPI_SaveParticipant.h"
#include "UInventoryRegistrySubsystem.generated.h"

class UInventoryData;
class UContainerComponent;

// Every inventory of the world, by id. The server's copy is the truth; a client's registry stays empty.
UCLASS()
class THEAWAKEN_API UInventoryRegistrySubsystem : public UWorldSubsystem, public IBPI_SaveParticipant{
    GENERATED_BODY()

public:
    // ---- Section 11: IBPI_SaveParticipant of Doc R13 (its real interface). Key Inventory.World ----
    inline static constexpr int32 currentSnapshotVersion = 1;

    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;

    virtual FName getSaveKey_Implementation() const override;
    virtual E_SaveScope getSaveScope_Implementation() const override;
    virtual int32 getSnapshotVersion_Implementation() const override;
    virtual APlayerState* getSaveOwner_Implementation() const override;
    // Query: never mutates.
    virtual bool getSnapshotForSave_Implementation(FInstancedStruct& snapshot) const override;
    virtual bool restoreFromSnapshot_Implementation(const FInstancedStruct& snapshot, int32 savedVersion) override;
    virtual bool migrateSnapshotStep_Implementation(int32 fromVersion, const FInstancedStruct& oldSnapshot, FInstancedStruct& newSnapshot) const override;
    virtual bool contributeToSlotHeader_Implementation(const FS_SaveSlotHeader& display, FS_SaveSlotHeader& contribution) const override;

    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

    static UInventoryRegistrySubsystem* get(const UObject* worldContext);

    // ---- queries ----
    UInventoryData* findInventory(const FGuid& inventoryId) const;
    // The loaded actor component of a world container, or null while its area is not loaded.
    UContainerComponent* findHost(const FGuid& inventoryId) const;
    const TMap<FGuid, TObjectPtr<UInventoryData>>& getAllInventories() const;
    // The contents of a Portable Container item (R01 3.4). Null when the entry is not a container.
    UInventoryData* getContainedInventory(const FS_InventoryEntry& containerEntry, bool createIfMissing);
    // True for an item that is not a container, and for a container that holds nothing (R01 3.4a).
    bool isContainerItemEmpty(const FS_InventoryEntry& entry) const;

    // ---- commands (server) ----
    UInventoryData* createInventory(E_InventoryType type, const FGuid& inventoryId, const FS_ContainerRules& rules);
    UInventoryData* findOrCreateInventory(E_InventoryType type, const FGuid& inventoryId, const FS_ContainerRules& rules);
    // Removes the inventory and the contents of every Portable Container inside it.
    void destroyInventory(const FGuid& inventoryId);
    void registerHost(const FGuid& inventoryId, UContainerComponent* host);
    void unregisterHost(const FGuid& inventoryId);

private:
    // What belongs in the World file: a world Container, or a bag lying in the world.
    static bool isWorldRoot(const UInventoryData* inventory);

    UPROPERTY() TMap<FGuid, TObjectPtr<UInventoryData>> inventories;
    TMap<FGuid, TWeakObjectPtr<UContainerComponent>> hosts;
};
