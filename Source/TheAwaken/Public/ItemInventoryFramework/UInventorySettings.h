#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "UInventorySettings.generated.h"

// Project Settings -> Game -> Awaken Inventory. Every value is tuning data (Doc00 2.7).
// Sections 9 and 10 add the starting kit and the page mapping to this class.
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Awaken Inventory"))
class THEAWAKEN_API UInventorySettings : public UDeveloperSettings{
    GENERATED_BODY()

public:
    UInventorySettings();

    static const UInventorySettings* get(){
        return GetDefault<UInventorySettings>();
    }

    // ---- hotbar (R01 3.5, 3.8) ----
    // Slots 0 to 9.
    UPROPERTY(Config, EditAnywhere, Category = "Hotbar", meta = (ClampMin = "1")) int32 hotbarSlotCount = 10;
    // While Bagless only keys 1, 2 and 3 work.
    UPROPERTY(Config, EditAnywhere, Category = "Hotbar", meta = (ClampMin = "0")) int32 baglessSlotCount = 3;
    // The always-available Unarmed entry (R01 3.10).
    UPROPERTY(Config, EditAnywhere, Category = "Hotbar") FName unarmedItemId = TEXT("Unarmed");

    // ---- ranges, in centimetres, measured from the pawn's eye. Inclusive (Doc02 19.9) ----
    UPROPERTY(Config, EditAnywhere, Category = "Requests", meta = (ClampMin = "0")) float pickupRange = 250.0f;
    UPROPERTY(Config, EditAnywhere, Category = "Requests", meta = (ClampMin = "0")) float containerRange = 300.0f;
    // Rate limit of one player's requests (Doc02 19.6). A player may also spend up to one second's worth at once.
    UPROPERTY(Config, EditAnywhere, Category = "Requests", meta = (ClampMin = "1")) float requestsPerSecond = 10.0f;

    // ---- loose items (Doc02 19.7) ----
    // Life of a loose item whose definition says 0. There is no global cap, only lifetimes.
    UPROPERTY(Config, EditAnywhere, Category = "Loose Items", meta = (ClampMin = "0")) float defaultLooseLifetimeSeconds = 600.0f;
    // How far in front of the player a dropped item appears.
    UPROPERTY(Config, EditAnywhere, Category = "Loose Items", meta = (ClampMin = "0")) float dropForwardDistance = 80.0f;

    // ---- starting kit (R01 3.8): what a new character's bag holds. The bag itself is created with it ----
    UPROPERTY(Config, EditAnywhere, Category = "Starting Kit") TArray<FS_ItemStack> startingKit;

    // ---- bag screen (R01 3.1a). The one mapping from item category to page ----
    UPROPERTY(Config, EditAnywhere, Category = "Bag Screen") TArray<FS_InventoryPageMapping> pageMappings;
    // The page of every category the mapping does not name: Others.
    UPROPERTY(Config, EditAnywhere, Category = "Bag Screen", meta = (Categories = "Inventory.Page")) FGameplayTag fallbackPageTag;
};
