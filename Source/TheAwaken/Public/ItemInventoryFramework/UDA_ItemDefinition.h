#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ItemInventoryFramework/00Enums/E_ItemInventoryFramework.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "UDA_ItemDefinition.generated.h"

class UStaticMesh;
class USkeletalMesh;
class UTexture2D;
class UDA_StackLayout;

// R01 3.1: every item is one of these. A new item is a new data asset, never new code.
UCLASS(BlueprintType)
class THEAWAKEN_API UDA_ItemDefinition : public UPrimaryDataAsset{
    GENERATED_BODY()

public:
    // The unbreakable sentinel (R01 3.1, I4). Used by the starter Axe only.
    inline static constexpr float unbreakableDurability = -1.0f;

    static FPrimaryAssetType getPrimaryAssetType(){
        return FPrimaryAssetType(TEXT("Item"));
    }

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

    // ---------- S2.4 Identity ----------
    // The stable id (I1). Saves, replication, recipes and hotbar keys all carry this name. Never rename it after release.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity") FName itemId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity") E_ItemKind itemKind = E_ItemKind::Stackable;
    // What kind of item it is. The page of the bag screen is derived from this (R01 3.1a).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (Categories = "Item.Category")) FGameplayTag categoryTag;
    // Tags used for restrictions and flags: Item.Flag.Permanent, clean or unclean water, and so on.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity") FGameplayTagContainer itemTags;
    // The most of this item the bag can hold (designer-confirmed: the bag limits each item type). The value is data.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (ClampMin = "0")) int32 perTypeLimit = 1;
    // Soft references (Doc02 19.8). The icon stays for places that cannot show 3D (hover cards, popups).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity") TSoftObjectPtr<UTexture2D> icon;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity") TSoftObjectPtr<UStaticMesh> worldMesh;

    // ---------- S2.5 Carry (Doc B07) and processing (Doc B10). This document only holds the fields ----------
    // 0 = an ordinary item. Above 0 = a heavy item: it fills that many carry slots and NEVER enters the bag (R01 3.7 step 0).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry", meta = (ClampMin = "0")) int32 carryCapacityCost = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry", meta = (Categories = "Carry.WeightClass")) FGameplayTag weightClassTag;
    // 'Use directly' versus 'must be cooked, dried or boiled first'.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing") bool requiresProcessing = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing") FName linkedProcessingRecipe;

    // ---------- S2.6 The weapon block (Doc R03 reads these numbers; it does not own them) and durability ----------
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") E_WeaponType weaponType = E_WeaponType::None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0")) float attack = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0")) float attackSpeed = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0")) float damage = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0", ClampMax = "1")) float armorPenetrationPercent = 0.0f;
    // Crossbow and Firearm: the stackable ammunition item that a reload spends (name PROPOSED; to confirm with Doc R03).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") FName ammoItemId;
    // Firearm only (names PROPOSED, all values data). 1 for a single-shot gun.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Firearm", meta = (ClampMin = "0")) int32 magazineSize = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Firearm", meta = (ClampMin = "0")) float bulletSpeed = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Firearm", meta = (ClampMin = "0")) float bulletDrop = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Firearm", meta = (ClampMin = "0")) float bulletLongestRange = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Firearm") float recoilPitch = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Firearm") float recoilYaw = 0.0f;
    // -1 = unbreakable (the starter Axe only), 0 = no durability, above 0 = how much the item has when new.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Durability") float durabilityMax = 0.0f;
    // Weapon wear is a flat amount per use (Index A112). Doc R03 applies it.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Durability", meta = (ClampMin = "0")) float durabilityLossPerUse = 0.0f;

    // ---------- S2.7 Consumables, spoilage, medicine (R01 3.2a, 3.9) ----------
    // Food, drink and medicine: meter changes, plus the illness chance and HP loss of spoiled food and unclean water.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consume") TArray<FS_ConsumeEffect> consumeEffects;
    // The spec's bCanSpoil. Off = never spoils.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consume|Spoilage") bool canSpoil = false;
    // In in-game hours. Only read when canSpoil is on.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consume|Spoilage", meta = (ClampMin = "0", EditCondition = "canSpoil")) float spoilTimeHours = 0.0f;
    // The item this one becomes when its spoil time is reached.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consume|Spoilage", meta = (EditCondition = "canSpoil")) FName spoiledItemId;
    // The share of this item's fresh meter values that its spoiled variant gives (designer: 0.5).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consume|Spoilage", meta = (ClampMin = "0", ClampMax = "1")) float spoiledEffectiveness = 0.5f;
    // Variant items only (unclean water): the item whose meter values this one derives from...
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consume|Variant") FName effectBaseItemId;
    // ...scaled by this share (designer: 0.5). A separate field from spoiledEffectiveness so they tune apart.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consume|Variant", meta = (ClampMin = "0", ClampMax = "1")) float uncleanWaterEffectiveness = 0.5f;
    // Medicine only (R01 3.9, decision D10).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consume|Medicine", meta = (ClampMin = "0")) float healthRestoreAmount = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consume|Medicine", meta = (ClampMin = "0")) float illnessReductionAmount = 0.0f;

    // ---------- S2.8 Portable Container, loose life, and the Equipment block ----------
    // Portable Container items only: the limit per item type and which tags it accepts (R01 3.1, 3.4).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Container") FS_ContainerRules capacityRules;
    // Life of this item as a loose object, in seconds. 0 = the project default (Doc02 19.7).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loose", meta = (ClampMin = "0")) float looseLifetimeSeconds = 0.0f;

    // ---------- Equipment block (Doc R05). Names PROPOSED, every value data ----------
    // Armor: what one worn piece adds to the aggregated armor value (R05 §1 "ArmorValue").
    // How much damage the piece can absorb before it disappears is durabilityMax (R05 3.2; E4).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (ClampMin = "0")) float armorValue = 0.0f;
    // Clothes: taken off an attack before the armor is asked (R05 3.6; E5). Never wears out.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (ClampMin = "0")) float clothesProtection = 0.0f;
    // Armor and clothes: lowers the cold-exposure Illness source while worn (R05 3.5; Doc B14 3.6). The size is data.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (ClampMin = "0")) float warmth = 0.0f;
    // Armor: the mesh shown at the piece's stack position. Clothes: the outfit mesh. Doc R07 3.12 owns how they are shown.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment") TSoftObjectPtr<UStaticMesh> armorMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment") TSoftObjectPtr<USkeletalMesh> outfitMesh;

    // ---------- S2.9 Bag screen (R01 3.1a; names PROPOSED) ----------
    // The one fixed slot of this item on its page. The page itself is not stored: it is derived from categoryTag.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bag Screen") FS_InventorySlotRef inventorySlot;
    // The low-poly model drawn in the slot, one per unit. Separate from worldMesh so its detail is its own data.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bag Screen") TSoftObjectPtr<UStaticMesh> inventoryMesh;
    // Optional. The other item id that shares this item's slot: spoiled and fresh food, clean and unclean water.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bag Screen") FName slotSharedWith;
    // Where unit 1, 2, 3 and so on of the pile sit.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bag Screen") TSoftObjectPtr<UDA_StackLayout> stackLayout;
    // Optional second slot for the smaller staging widget (Doc R02 3.3). Unset = the inventory slot, scaled.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bag Screen") FS_InventorySlotRef stagingSlot;

    // ---------- Queries ----------
    bool isStackable() const{
        return itemKind == E_ItemKind::Stackable;
    }

    bool isInstanced() const{
        return itemKind == E_ItemKind::Instanced;
    }

    bool isWeapon() const{
        return categoryTag == T_Item_Category_Weapon;
    }

    bool isArmor() const{
        return categoryTag == T_Item_Category_Armor;
    }

    bool isClothing() const{
        return categoryTag == T_Item_Category_Clothing;
    }

    // Permanent items cannot be thrown, dropped, moved into a container, staged or destroyed (R01 3.10).
    bool isPermanent() const{
        return itemTags.HasTagExact(T_Item_Flag_Permanent);
    }

    bool isUnbreakable() const{
        return durabilityMax == unbreakableDurability;
    }

    bool hasDurability() const{
        return durabilityMax > 0.0f;
    }

    // A heavy item goes to the hands only (R01 3.7 step 0, 3.11).
    bool isHeavy() const{
        return carryCapacityCost > 0;
    }

    bool isPortableContainer() const{
        return categoryTag == T_Item_Category_Container && isInstanced();
    }

    bool isConsumable() const{
        return consumeEffects.Num() > 0 || !effectBaseItemId.IsNone() || healthRestoreAmount > 0.0f || illnessReductionAmount > 0.0f;
    }

    // True when the item's category tag or one of its item tags matches.
    bool carriesTag(const FGameplayTag& tag) const{
        return tag.IsValid() && (categoryTag.MatchesTag(tag) || itemTags.HasTag(tag));
    }

    // Text keys, derived from the id (Doc R10 3.3, I22). The UI turns a key into text; nothing here holds a sentence.
    UFUNCTION(BlueprintPure, Category = "Awaken|Item") FString getDisplayNameKey() const;
    UFUNCTION(BlueprintPure, Category = "Awaken|Item") FString getDescriptionKey() const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
