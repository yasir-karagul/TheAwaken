#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CosmeticEventsEnvironmentPresentationFramework/01Structs/S_CosmeticEventsEnvironmentPresentationFramework.h"
#include "CosmeticEnvironmentMath.generated.h"

// Pure functions of Part A, B and C. No world, no state, so the tier-1 automation tests can call them directly.
UCLASS()
class THEAWAKEN_API UCosmeticEnvironmentMathLibrary : public UBlueprintFunctionLibrary{
    GENERATED_BODY()

public:
    // ---- Part A ----

    // Gap G11. The same GUID on every machine for the same PlayerIdentity string (compared without case, like FS_PlayerIdentity::matchesId).
    // An empty string gives an empty GUID, which means no instigator.
    static FGuid makePlayerInstigatorId(const FString& playerIdentity);

    // An NPC's instigator id is the NPCId itself (R16).
    static FGuid makeNpcInstigatorId(const FGuid& npcId);

    // Prunes entries older than windowSeconds, then counts the sends within radius of location. Returns true (and records nothing) when the
    // count has reached maxPerSecond. Otherwise records the send and returns false. maxPerSecond below 1 counts as 1.
    static bool isRateLimited(TArray<FS_RecentSend>& recent, double now, const FVector& location, float radius, int32 maxPerSecond, double windowSeconds);

    // ---- Part B ----

    // Hours (0 to 24, the unit of UTimeSubsystem::getCurrentTimeOfDay) to a day fraction in 0 to 1. Wraps, also for negative input.
    static float normalizeTimeOfDay(float hours);

    // Unit vector from the ground toward the sun. Dawn (0.25) is on the horizon, noon (0.5) is overhead, midnight (0.0 and 1.0) is below.
    // The moon is the opposite vector. yawDegrees turns the orbit around the vertical axis, tiltDegrees leans it.
    static FVector computeToSunDirection(float dayFraction, float yawDegrees, float tiltDegrees);

    static float computeBlendAlpha(float elapsedSeconds, float blendSeconds);

    // Lerps every value of the look together. The wind direction takes the short way round.
    static FS_WeatherLook lerpLook(const FS_WeatherLook& from, const FS_WeatherLook& to, float alpha);

    // strength is 0 to 100 (B14). The target look moves from atZero to atFull as the strength rises.
    static FS_WeatherLook scaleLookByStrength(const FS_WeatherLook& atZero, const FS_WeatherLook& atFull, float strength);

    // Wetness rises while it rains, faster with strength, and falls slowly otherwise (EF6). 0 to 1.
    static float stepWetness(float wetness, float target, float rainStrength, float deltaSeconds, float riseSecondsAtFullRain, float drySeconds);

    // The update delta in seconds, clamped so a hitch or an alt-tab cannot make wetness or a blend jump.
    static float clampUpdateDelta(double now, double lastTime, float maxDeltaSeconds);

    // 0 to 1 flash curve: full at the start, gone at pulseSeconds.
    static float computeLightningPulse(float elapsedSeconds, float pulseSeconds);

    // ---- Part C ----

    // flowDirection times baseFlowSpeed times the replicated river strength (0 to 100) times a scale in data (B14 3.3).
    // Reads replicated values only, so the host and the owning client get the same vector.
    static FVector computeFlow(const FVector& flowDirection, float baseFlowSpeed, float riverStrength, float flowSpeedPerStrength);
};
