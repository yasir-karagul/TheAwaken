#include "CosmeticEventsEnvironmentPresentationFramework/DA_GroundTypeMap.h"
#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEnvironmentLog.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "GameplayTagsManager.h"
#include "Misc/DataValidation.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

FPrimaryAssetId UDA_GroundTypeMap::GetPrimaryAssetId() const{
    return FPrimaryAssetId(FPrimaryAssetType("GroundTypeMap"), GetFName());
}

UDA_GroundTypeMap* UDA_GroundTypeMap::loadDefault(){
    const FPrimaryAssetId assetId(FPrimaryAssetType("GroundTypeMap"), FName("DA_GroundTypeMap_Default"));
    return Cast<UDA_GroundTypeMap>(UAssetManager::Get().GetPrimaryAssetPath(assetId).TryLoad());
}

FGameplayTag UDA_GroundTypeMap::sampleAt(const FVector& location) const{
    const int32 column = FMath::FloorToInt((location.X - originCm.X) / cellSizeCm);
    const int32 row = FMath::FloorToInt((location.Y - originCm.Y) / cellSizeCm);
    if(column < 0 || row < 0 || column >= gridWidth || row >= gridHeight){
        return defaultGround;
    }
    const int32 index = row * gridWidth + column;
    if(!cells.IsValidIndex(index)){
        return defaultGround;
    }
    const uint8 paletteIndex = cells[index];
    if(paletteIndex == noneIndex || !palette.IsValidIndex(paletteIndex)){
        return defaultGround;
    }
    return palette[paletteIndex];
}

#if WITH_EDITOR
EDataValidationResult UDA_GroundTypeMap::IsDataValid(FDataValidationContext& context) const{
    EDataValidationResult result = Super::IsDataValid(context);
    bool isInvalid = false;

    // A tag that names nothing in the registry fails the build (Doc02 19.8).
    TArray<FGameplayTag> allTags = palette;
    allTags.Add(defaultGround);
    for(const FGameplayTag& tag : allTags){
        if(tag.GetTagName().IsNone()){
            continue;
        }
        if(!UGameplayTagsManager::Get().RequestGameplayTag(tag.GetTagName(), false).IsValid()){
            context.AddError(FText::FromString(FString::Printf(TEXT("Ground tag %s is not in the Tag Registry"), *tag.GetTagName().ToString())));
            isInvalid = true;
        }
    }
    if(cells.Num() != 0 && cells.Num() != gridWidth * gridHeight){
        context.AddError(FText::FromString(TEXT("cells does not hold gridWidth times gridHeight entries. Bake the map again")));
        isInvalid = true;
    }
    if(palette.Num() >= noneIndex){
        context.AddError(FText::FromString(TEXT("The palette holds too many ground types for one byte per cell")));
        isInvalid = true;
    }
    for(const uint8 paletteIndex : cells){
        if(paletteIndex != noneIndex && !palette.IsValidIndex(paletteIndex)){
            context.AddError(FText::FromString(TEXT("A cell points outside the palette. Bake the map again")));
            isInvalid = true;
            break;
        }
    }
    if(isInvalid){
        return EDataValidationResult::Invalid;
    }
    return result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : result;
}

void UDA_GroundTypeMap::bakeFromWorld(UWorld* world){
    if(world == nullptr){
        UE_LOG(LogAwakenEnvironment, Warning, TEXT("Ground bake needs a world"));
        return;
    }
    Modify();
    palette.Reset();
    cells.Init(noneIndex, gridWidth * gridHeight);

    FCollisionQueryParams params(SCENE_QUERY_STAT(GroundTypeBake), false);
    params.bReturnPhysicalMaterial = true;

    int32 hitCells = 0;
    for(int32 row = 0; row < gridHeight; row++){
        for(int32 column = 0; column < gridWidth; column++){
            const double centreX = originCm.X + (column + 0.5) * cellSizeCm;
            const double centreY = originCm.Y + (row + 0.5) * cellSizeCm;
            const FVector start(centreX, centreY, bakeTraceHeightCm);
            const FVector end(centreX, centreY, -bakeTraceHeightCm);
            FHitResult hit;
            if(!world->LineTraceSingleByChannel(hit, start, end, ECC_Visibility, params)){
                continue;
            }
            const UPhysicalMaterial* material = hit.PhysMaterial.Get();
            const FGameplayTag* tag = nullptr;
            for(const TPair<TObjectPtr<UPhysicalMaterial>, FGameplayTag>& pair : physicalMaterialTags){
                if(pair.Key == material){
                    tag = &pair.Value;
                    break;
                }
            }
            if(tag == nullptr || !tag->IsValid()){
                continue;
            }
            int32 paletteIndex = palette.IndexOfByKey(*tag);
            if(paletteIndex == INDEX_NONE){
                paletteIndex = palette.Add(*tag);
            }
            if(paletteIndex >= noneIndex){
                continue;
            }
            cells[row * gridWidth + column] = static_cast<uint8>(paletteIndex);
            hitCells++;
        }
    }
    MarkPackageDirty();
    UE_LOG(LogAwakenEnvironment, Log, TEXT("Ground bake: %d of %d cells mapped, %d ground types. Save the asset."), hitCells, gridWidth * gridHeight, palette.Num());
}
#endif
