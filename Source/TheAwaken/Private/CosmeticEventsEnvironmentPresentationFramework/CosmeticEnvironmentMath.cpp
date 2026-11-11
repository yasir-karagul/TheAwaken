#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEnvironmentMath.h"

namespace{
    FLinearColor lerpColor(const FLinearColor& from, const FLinearColor& to, float alpha){
        return FLinearColor(
            FMath::Lerp(from.R, to.R, alpha),
            FMath::Lerp(from.G, to.G, alpha),
            FMath::Lerp(from.B, to.B, alpha),
            FMath::Lerp(from.A, to.A, alpha));
    }
}

FGuid UCosmeticEnvironmentMathLibrary::makePlayerInstigatorId(const FString& playerIdentity){
    if(playerIdentity.IsEmpty()){
        return FGuid();
    }
    // The salt keeps this GUID apart from any other deterministic GUID built from the same text. Identity ids are compared without case.
    const FString key = FString(TEXT("Awaken.PlayerIdentity:")) + playerIdentity.ToLower();
    return FGuid::NewDeterministicGuid(key);
}

FGuid UCosmeticEnvironmentMathLibrary::makeNpcInstigatorId(const FGuid& npcId){
    return npcId;
}

bool UCosmeticEnvironmentMathLibrary::isRateLimited(TArray<FS_RecentSend>& recent, double now, const FVector& location, float radius, int32 maxPerSecond, double windowSeconds){
    recent.RemoveAll([now, windowSeconds](const FS_RecentSend& entry){
        return now - entry.time > windowSeconds;
    });
    const double radiusSquared = static_cast<double>(radius) * static_cast<double>(radius);
    int32 sameArea = 0;
    for(const FS_RecentSend& entry : recent){
        if(FVector::DistSquared(entry.location, location) <= radiusSquared){
            sameArea++;
        }
    }
    if(sameArea >= FMath::Max(1, maxPerSecond)){
        return true;
    }
    FS_RecentSend added;
    added.time = now;
    added.location = location;
    recent.Add(added);
    return false;
}

float UCosmeticEnvironmentMathLibrary::normalizeTimeOfDay(float hours){
    float wrapped = FMath::Fmod(hours, 24.0f);
    if(wrapped < 0.0f){
        wrapped += 24.0f;
    }
    return wrapped / 24.0f;
}

FVector UCosmeticEnvironmentMathLibrary::computeToSunDirection(float dayFraction, float yawDegrees, float tiltDegrees){
    const float angle = FMath::DegreesToRadians((dayFraction - 0.25f) * 360.0f);
    FVector toSun(FMath::Cos(angle), 0.0f, FMath::Sin(angle));
    toSun = FQuat(FVector::XAxisVector, FMath::DegreesToRadians(tiltDegrees)).RotateVector(toSun);
    toSun = FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(yawDegrees)).RotateVector(toSun);
    return toSun.GetSafeNormal();
}

float UCosmeticEnvironmentMathLibrary::computeBlendAlpha(float elapsedSeconds, float blendSeconds){
    if(blendSeconds <= 0.0f){
        return 1.0f;
    }
    return FMath::Clamp(elapsedSeconds / blendSeconds, 0.0f, 1.0f);
}

FS_WeatherLook UCosmeticEnvironmentMathLibrary::lerpLook(const FS_WeatherLook& from, const FS_WeatherLook& to, float alpha){
    FS_WeatherLook result;
    result.cloudCoverage = FMath::Lerp(from.cloudCoverage, to.cloudCoverage, alpha);
    result.cloudDensity = FMath::Lerp(from.cloudDensity, to.cloudDensity, alpha);
    result.fogDensity = FMath::Lerp(from.fogDensity, to.fogDensity, alpha);
    result.lightScale = FMath::Lerp(from.lightScale, to.lightScale, alpha);
    result.skyTint = lerpColor(from.skyTint, to.skyTint, alpha);
    result.windStrength = FMath::Lerp(from.windStrength, to.windStrength, alpha);
    result.windDirectionDegrees = from.windDirectionDegrees + FMath::FindDeltaAngleDegrees(from.windDirectionDegrees, to.windDirectionDegrees) * alpha;
    result.wetnessTarget = FMath::Lerp(from.wetnessTarget, to.wetnessTarget, alpha);
    return result;
}

FS_WeatherLook UCosmeticEnvironmentMathLibrary::scaleLookByStrength(const FS_WeatherLook& atZero, const FS_WeatherLook& atFull, float strength){
    return lerpLook(atZero, atFull, FMath::Clamp(strength / 100.0f, 0.0f, 1.0f));
}

float UCosmeticEnvironmentMathLibrary::stepWetness(float wetness, float target, float rainStrength, float deltaSeconds, float riseSecondsAtFullRain, float drySeconds){
    const float rain = FMath::Clamp(rainStrength / 100.0f, 0.0f, 1.0f);
    if(wetness < target && rain > 0.0f){
        const float ratePerSecond = rain / FMath::Max(riseSecondsAtFullRain, 0.01f);
        return FMath::Min(target, wetness + ratePerSecond * deltaSeconds);
    }
    if(wetness > target){
        const float ratePerSecond = 1.0f / FMath::Max(drySeconds, 0.01f);
        return FMath::Max(target, wetness - ratePerSecond * deltaSeconds);
    }
    return wetness;
}

float UCosmeticEnvironmentMathLibrary::clampUpdateDelta(double now, double lastTime, float maxDeltaSeconds){
    return FMath::Clamp(static_cast<float>(now - lastTime), 0.0f, FMath::Max(maxDeltaSeconds, 0.0f));
}

float UCosmeticEnvironmentMathLibrary::computeLightningPulse(float elapsedSeconds, float pulseSeconds){
    if(pulseSeconds <= 0.0f || elapsedSeconds < 0.0f || elapsedSeconds >= pulseSeconds){
        return 0.0f;
    }
    const float remaining = 1.0f - (elapsedSeconds / pulseSeconds);
    return remaining * remaining;
}

FVector UCosmeticEnvironmentMathLibrary::computeFlow(const FVector& flowDirection, float baseFlowSpeed, float riverStrength, float flowSpeedPerStrength){
    return flowDirection.GetSafeNormal() * (baseFlowSpeed * riverStrength * flowSpeedPerStrength);
}
