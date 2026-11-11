#include "CameraFramework/DA_ScreenEffectMapping.h"

#include "CameraFramework/CameraFramework.h"
#include "Curves/CurveFloat.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/DataValidation.h"
#include "Modules/ModuleManager.h"
#endif

FPrimaryAssetType UDA_ScreenEffectMapping::getAssetType(){
    return FPrimaryAssetType(TEXT("ScreenEffectMapping"));
}

FPrimaryAssetId UDA_ScreenEffectMapping::GetPrimaryAssetId() const{
    if(HasAnyFlags(RF_ClassDefaultObject)){
        return FPrimaryAssetId();
    }
    return FPrimaryAssetId(getAssetType(), GetFName());
}

#if WITH_EDITOR
EDataValidationResult UDA_ScreenEffectMapping::IsDataValid(FDataValidationContext& context) const{
    EDataValidationResult result = EDataValidationResult::Valid;
    if(Super::IsDataValid(context) == EDataValidationResult::Invalid){
        result = EDataValidationResult::Invalid;
    }

    if(!sourceMessageTag.IsValid()){
        context.AddError(FText::FromString(TEXT("ScreenEffectMapping: sourceMessageTag is empty. Pick the message this row listens to.")));
        result = EDataValidationResult::Invalid;
    }
    if(!effectTag.IsValid()){
        context.AddError(FText::FromString(TEXT("ScreenEffectMapping: effectTag is empty. Pick the ScreenEffect.* channel this row drives.")));
        result = EDataValidationResult::Invalid;
    }
    if(valueCurve == nullptr){
        context.AddError(FText::FromString(TEXT("ScreenEffectMapping: valueCurve is empty. The curve turns the source value into the 0 to 1 target.")));
        result = EDataValidationResult::Invalid;
    }
    if(pulseSeconds < 0.0f){
        context.AddError(FText::FromString(TEXT("ScreenEffectMapping: pulseSeconds must not be negative.")));
        result = EDataValidationResult::Invalid;
    }
    if(maxPawnDistance < 0.0f){
        context.AddError(FText::FromString(TEXT("ScreenEffectMapping: maxPawnDistance must not be negative.")));
        result = EDataValidationResult::Invalid;
    }

    // A duplicate asset id fails the check (Doc02 19.8). The id is the asset name, so two assets with one name in different folders clash.
    // Two rows on one parameter are legal, because priority resolves them; equal priority and the same source is almost always a mistake, so it only warns.
    FAssetRegistryModule& registryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FAssetData> assets;
    registryModule.Get().GetAssetsByClass(UDA_ScreenEffectMapping::StaticClass()->GetClassPathName(), assets, true);
    const FSoftObjectPath selfPath(this);
    for(const FAssetData& data : assets){
        if(data.GetSoftObjectPath() == selfPath){
            continue;
        }
        if(data.AssetName == GetFName()){
            context.AddError(FText::FromString(FString::Printf(TEXT("ScreenEffectMapping: duplicate asset id %s, rename one of the two assets."), *GetName())));
            result = EDataValidationResult::Invalid;
            continue;
        }
        const UDA_ScreenEffectMapping* other = Cast<UDA_ScreenEffectMapping>(data.GetAsset());
        if(other != nullptr && other->effectTag == effectTag && other->priority == priority && other->sourceMessageTag == sourceMessageTag){
            context.AddWarning(FText::FromString(FString::Printf(TEXT("ScreenEffectMapping: %s targets the same effect with the same priority and the same source. Legal (the larger target wins), but check it is intended."), *other->GetName())));
        }
    }
    return result;
}
#endif
