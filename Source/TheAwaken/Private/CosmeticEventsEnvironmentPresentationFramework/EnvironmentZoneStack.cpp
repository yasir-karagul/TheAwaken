#include "CosmeticEventsEnvironmentPresentationFramework/EnvironmentZoneStack.h"

namespace{
    constexpr float visibleWeightLimit = 0.001f;

    float moveToward(float current, float target, float maxDelta){
        if(current < target){
            return FMath::Min(current + maxDelta, target);
        }
        return FMath::Max(current - maxDelta, target);
    }
}

void FEnvironmentZoneStack::push(const FGuid& zoneId, const FS_EnvironmentZoneSettings& settings, float defaultBlendSeconds, bool jumpToFull){
    const float resolvedBlend = settings.blendSeconds > 0.0f ? settings.blendSeconds : defaultBlendSeconds;
    FS_ZoneEntry* existing = entries.FindByPredicate([&zoneId](const FS_ZoneEntry& entry){
        return entry.zoneId == zoneId;
    });
    if(existing != nullptr){
        // Same id: update the settings, keep the position, and bring it back if it was fading out.
        existing->settings = settings;
        existing->blendSeconds = resolvedBlend;
        existing->targetWeight = 1.0f;
        if(jumpToFull){
            existing->weight = 1.0f;
        }
        return;
    }
    FS_ZoneEntry entry;
    entry.zoneId = zoneId;
    entry.settings = settings;
    entry.blendSeconds = resolvedBlend;
    entry.targetWeight = 1.0f;
    entry.weight = jumpToFull ? 1.0f : 0.0f;
    entries.Add(entry);
}

void FEnvironmentZoneStack::pop(const FGuid& zoneId){
    const int32 index = entries.IndexOfByPredicate([&zoneId](const FS_ZoneEntry& entry){
        return entry.zoneId == zoneId;
    });
    if(index == INDEX_NONE || entries[index].targetWeight <= 0.0f){
        return;
    }
    entries[index].targetWeight = 0.0f;
    if(entries[index].weight <= 0.0f){
        entries.RemoveAt(index);
    }
}

void FEnvironmentZoneStack::advance(float deltaSeconds){
    for(int32 index = entries.Num() - 1; index >= 0; index--){
        FS_ZoneEntry& entry = entries[index];
        const float step = entry.blendSeconds > 0.0f ? deltaSeconds / entry.blendSeconds : 1.0f;
        entry.weight = moveToward(entry.weight, entry.targetWeight, step);
        if(entry.targetWeight <= 0.0f && entry.weight <= 0.0f){
            entries.RemoveAt(index);
        }
    }
}

void FEnvironmentZoneStack::snapAll(){
    for(int32 index = entries.Num() - 1; index >= 0; index--){
        entries[index].weight = entries[index].targetWeight;
        if(entries[index].targetWeight <= 0.0f){
            entries.RemoveAt(index);
        }
    }
}

void FEnvironmentZoneStack::clear(){
    entries.Reset();
}

bool FEnvironmentZoneStack::isMoving() const{
    for(const FS_ZoneEntry& entry : entries){
        if(!FMath::IsNearlyEqual(entry.weight, entry.targetWeight)){
            return true;
        }
    }
    return false;
}

TArray<FGuid> FEnvironmentZoneStack::getActiveIds() const{
    TArray<FGuid> ids;
    for(const FS_ZoneEntry& entry : entries){
        if(entry.targetWeight > 0.0f){
            ids.Add(entry.zoneId);
        }
    }
    return ids;
}

void FEnvironmentZoneStack::getPostProcessLayers(const FS_ZoneEntry*& lower, const FS_ZoneEntry*& upper) const{
    lower = nullptr;
    upper = nullptr;
    for(int32 index = entries.Num() - 1; index >= 0; index--){
        if(entries[index].weight <= visibleWeightLimit){
            continue;
        }
        if(upper == nullptr){
            upper = &entries[index];
        }else{
            lower = &entries[index];
            return;
        }
    }
}

float FEnvironmentZoneStack::layerSkyVisibility() const{
    float visibility = 1.0f;
    for(const FS_ZoneEntry& entry : entries){
        visibility = FMath::Lerp(visibility, entry.settings.hideSky ? 0.0f : 1.0f, entry.weight);
    }
    return visibility;
}

void FEnvironmentZoneStack::layerFog(FS_ZoneFog& fog) const{
    for(const FS_ZoneEntry& entry : entries){
        if(!entry.settings.overrideFog || entry.weight <= 0.0f){
            continue;
        }
        const float alpha = entry.weight;
        fog.density = FMath::Lerp(fog.density, entry.settings.fogDensity, alpha);
        fog.heightFalloff = FMath::Lerp(fog.heightFalloff, entry.settings.fogHeightFalloff, alpha);
        fog.inscatteringColor = FLinearColor(
            FMath::Lerp(fog.inscatteringColor.R, entry.settings.fogInscatteringColor.R, alpha),
            FMath::Lerp(fog.inscatteringColor.G, entry.settings.fogInscatteringColor.G, alpha),
            FMath::Lerp(fog.inscatteringColor.B, entry.settings.fogInscatteringColor.B, alpha),
            FMath::Lerp(fog.inscatteringColor.A, entry.settings.fogInscatteringColor.A, alpha));
        fog.startDistance = FMath::Lerp(fog.startDistance, entry.settings.fogStartDistance, alpha);
        fog.maxOpacity = FMath::Lerp(fog.maxOpacity, entry.settings.fogMaxOpacity, alpha);
    }
}
