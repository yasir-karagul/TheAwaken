#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UInventorySettings.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

FPrimaryAssetId UDA_ItemDefinition::GetPrimaryAssetId() const{
    // Item:<itemId>. Before the id is typed in, the asset name stands in so the Asset Manager can still list it.
    return FPrimaryAssetId(getPrimaryAssetType(), itemId.IsNone() ? GetFName() : itemId);
}

FString UDA_ItemDefinition::getDisplayNameKey() const{
    return FString::Printf(TEXT("Item.%s.Name"), *itemId.ToString());
}

FString UDA_ItemDefinition::getDescriptionKey() const{
    return FString::Printf(TEXT("Item.%s.Description"), *itemId.ToString());
}

#if WITH_EDITOR
EDataValidationResult UDA_ItemDefinition::IsDataValid(FDataValidationContext& Context) const{
    EDataValidationResult result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
    auto fail = [this, &Context, &result](const TCHAR* text){
        Context.AddError(FText::FromString(FString::Printf(TEXT("%s: %s"), *GetName(), text)));
        result = EDataValidationResult::Invalid;
    };
    // The Unarmed entry is never stored anywhere and a heavy item never enters the bag: neither needs a limit or a slot.
    const bool isNeverStored = itemId == UInventorySettings::get()->unarmedItemId;
    const bool isBagItem = !isNeverStored && !isHeavy();

    // ---- identity ----
    if(itemId.IsNone()){
        fail(TEXT("itemId is empty (Doc02 19.8: a missing id fails the check)."));
    }
    if(!categoryTag.IsValid() || !categoryTag.MatchesTag(T_Item_Category)){
        fail(TEXT("categoryTag must be a leaf of Item.Category."));
    }
    if(isBagItem && perTypeLimit < 1){
        fail(TEXT("perTypeLimit must be at least 1."));
    }

    // ---- the two kinds (R01 3.2) ----
    if(isStackable() && durabilityMax != 0.0f){
        fail(TEXT("A stackable item has no state of its own: durabilityMax must be 0. Make it Instanced if it wears out."));
    }
    if(isInstanced() && canSpoil){
        fail(TEXT("Only a stackable item can spoil: the spoil time belongs to the stack."));
    }
    if((categoryTag == T_Item_Category_Container) != isPortableContainer()){
        fail(TEXT("A Portable Container item must be Instanced: each copy keeps its own contents."));
    }

    // ---- weapon and durability ----
    if(isWeapon() != (weaponType != E_WeaponType::None)){
        fail(TEXT("A weapon needs a weaponType, and only a weapon may have one."));
    }
    if(isWeapon() && attackSpeed <= 0.0f){
        fail(TEXT("A weapon needs attackSpeed above zero."));
    }
    if((weaponType == E_WeaponType::Crossbow || weaponType == E_WeaponType::Firearm) && ammoItemId.IsNone()){
        fail(TEXT("A crossbow or a firearm needs ammoItemId."));
    }
    if(weaponType == E_WeaponType::Firearm && magazineSize < 1){
        fail(TEXT("A firearm needs magazineSize of at least 1."));
    }
    if(durabilityMax < 0.0f && !isUnbreakable()){
        fail(TEXT("durabilityMax below zero is only allowed as the unbreakable sentinel, -1."));
    }
    if(isUnbreakable() && !isPermanent()){
        fail(TEXT("The unbreakable sentinel is for the starter Axe only, which also carries Item.Flag.Permanent (R01 3.1, 3.10)."));
    }

    // ---- consumables ----
    if(canSpoil && (spoilTimeHours <= 0.0f || spoiledItemId.IsNone() || spoiledItemId == itemId)){
        fail(TEXT("An item that can spoil needs spoilTimeHours above zero and a spoiledItemId that is another item."));
    }
    if(effectBaseItemId == itemId && !itemId.IsNone()){
        fail(TEXT("effectBaseItemId cannot point at the item itself."));
    }
    for(const FS_ConsumeEffect& effect : consumeEffects){
        if(effect.kind != E_ConsumeEffectKind::HealthLoss && !effect.targetTag.IsValid()){
            fail(TEXT("A ModifyMeter or IllnessChance entry needs its targetTag."));
        }
    }

    // ---- bag screen (R01 3.1a: every item has a slot, a mesh and a stack layout) ----
    if(isBagItem){
        if(!inventorySlot.isSet()){
            fail(TEXT("inventorySlot is not set. Pick a free slot of the page layout of this item's category."));
        }
        if(inventoryMesh.IsNull()){
            fail(TEXT("inventoryMesh is not set."));
        }
        if(stackLayout.IsNull()){
            fail(TEXT("stackLayout is not set."));
        }
    }

    if(isArmor()){
        if(itemKind != E_ItemKind::Instanced){
            fail(TEXT("Armor must be Instanced: each piece has its own durability (R01 3.2)."));
        }
        if(durabilityMax <= 0.0f){
            fail(TEXT("Armor needs durabilityMax above zero: it is how much damage the piece can absorb (R05 3.2). The unbreakable sentinel is for the starter Axe only."));
        }
        if(armorValue <= 0.0f && warmth <= 0.0f){
            Context.AddWarning(FText::FromString(TEXT("Armor has no armorValue and no warmth, so wearing it does nothing.")));
        }
    }
    if(isClothing()){
        if(itemKind != E_ItemKind::Stackable){
            fail(TEXT("Clothes must be Stackable: they carry no state (R05 3.6, Index A87)."));
        }
        if(durabilityMax != 0.0f){
            fail(TEXT("Clothes never wear out (R05 3.6): durabilityMax must be 0."));
        }
    }
    return result;
}
#endif
