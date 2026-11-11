// Tier-1 automation tests of B21 (Doc B22 test layers): pure logic, no world, no network. Run from Session Frontend under Awaken.Cosmetic and Awaken.Environment.
// The network tests of B22 (distance, rate, resend with a host and two clients) use the console commands in CosmeticEnvironmentCheatCommands.cpp.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEnvironmentMath.h"
#include "CosmeticEventsEnvironmentPresentationFramework/DA_EnvironmentCurves.h"
#include "CosmeticEventsEnvironmentPresentationFramework/DA_GroundTypeMap.h"
#include "CosmeticEventsEnvironmentPresentationFramework/EnvironmentZoneStack.h"
#include "CosmeticEventsEnvironmentPresentationFramework/02Tags/T_CosmeticEventsEnvironmentPresentationFramework.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"

namespace{
    FGuid zoneGuid(uint32 number){
        return FGuid(0xB21B21B2, 0, 0, number);
    }

    FS_EnvironmentZoneSettings makeZone(bool hideSky, float blendSeconds){
        FS_EnvironmentZoneSettings zone;
        zone.hideSky = hideSky;
        zone.blendSeconds = blendSeconds;
        return zone;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCosmeticInstigatorIdTest, "Awaken.Cosmetic.InstigatorId", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCosmeticInstigatorIdTest::RunTest(const FString& parameters){
    const FGuid first = UCosmeticEnvironmentMathLibrary::makePlayerInstigatorId(TEXT("steam:7656119800000001"));
    const FGuid second = UCosmeticEnvironmentMathLibrary::makePlayerInstigatorId(TEXT("steam:7656119800000001"));
    const FGuid upper = UCosmeticEnvironmentMathLibrary::makePlayerInstigatorId(TEXT("STEAM:7656119800000001"));
    const FGuid other = UCosmeticEnvironmentMathLibrary::makePlayerInstigatorId(TEXT("steam:7656119800000002"));
    TestTrue(TEXT("The same identity gives the same id"), first == second);
    TestTrue(TEXT("The id ignores case, like the identity comparison"), first == upper);
    TestTrue(TEXT("Different identities give different ids"), first != other);
    TestTrue(TEXT("An id is valid"), first.IsValid());
    TestFalse(TEXT("An empty identity gives no instigator"), UCosmeticEnvironmentMathLibrary::makePlayerInstigatorId(FString()).IsValid());

    const FGuid npc = FGuid::NewGuid();
    TestTrue(TEXT("An NPC's instigator id is the NPCId itself"), UCosmeticEnvironmentMathLibrary::makeNpcInstigatorId(npc) == npc);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCosmeticRateLimitTest, "Awaken.Cosmetic.RateLimit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCosmeticRateLimitTest::RunTest(const FString& parameters){
    const int32 maxPerSecond = 8;
    const float radius = 1000.0f;
    TArray<FS_RecentSend> recent;
    const FVector spot(0.0, 0.0, 0.0);

    int32 passed = 0;
    for(int32 index = 0; index < maxPerSecond + 5; index++){
        if(!UCosmeticEnvironmentMathLibrary::isRateLimited(recent, 10.0 + index * 0.01, spot, radius, maxPerSecond, 1.0)){
            passed++;
        }
    }
    TestEqual(TEXT("Exactly maxPerSecond events pass in one second at one spot"), passed, maxPerSecond);

    // A different area has its own count.
    TestFalse(TEXT("An event outside the radius is not limited by the crowd"), UCosmeticEnvironmentMathLibrary::isRateLimited(recent, 10.2, FVector(5000.0, 0.0, 0.0), radius, maxPerSecond, 1.0));

    // After the window the limiter lets events through again.
    TestFalse(TEXT("After the window the same spot is free again"), UCosmeticEnvironmentMathLibrary::isRateLimited(recent, 12.0, spot, radius, maxPerSecond, 1.0));

    // A limit below one counts as one, so bad data cannot let everything through.
    TArray<FS_RecentSend> strict;
    TestFalse(TEXT("The first event passes with a limit of 0"), UCosmeticEnvironmentMathLibrary::isRateLimited(strict, 1.0, spot, radius, 0, 1.0));
    TestTrue(TEXT("The second is limited with a limit of 0"), UCosmeticEnvironmentMathLibrary::isRateLimited(strict, 1.1, spot, radius, 0, 1.0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnvironmentBlendTest, "Awaken.Environment.Blend", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEnvironmentBlendTest::RunTest(const FString& parameters){
    FS_WeatherLook clear;
    clear.cloudCoverage = 0.1f;
    clear.fogDensity = 0.002f;
    clear.lightScale = 1.0f;
    clear.windDirectionDegrees = 350.0f;
    clear.wetnessTarget = 0.0f;

    FS_WeatherLook storm;
    storm.cloudCoverage = 1.0f;
    storm.fogDensity = 0.03f;
    storm.lightScale = 0.3f;
    storm.windDirectionDegrees = 10.0f;
    storm.wetnessTarget = 1.0f;

    const FS_WeatherLook atStart = UCosmeticEnvironmentMathLibrary::lerpLook(clear, storm, 0.0f);
    const FS_WeatherLook atEnd = UCosmeticEnvironmentMathLibrary::lerpLook(clear, storm, 1.0f);
    TestEqual(TEXT("Alpha 0 is the start look"), atStart.cloudCoverage, clear.cloudCoverage);
    TestEqual(TEXT("Alpha 1 is the end look"), atEnd.fogDensity, storm.fogDensity);

    const FS_WeatherLook half = UCosmeticEnvironmentMathLibrary::lerpLook(clear, storm, 0.5f);
    TestEqual(TEXT("Every value moves together"), half.lightScale, 0.65f);
    TestEqual(TEXT("The wind direction takes the short way round (350 to 10 passes 0, not 180)"), FMath::Abs(FMath::FindDeltaAngleDegrees(half.windDirectionDegrees, 0.0f)), 0.0f, 0.01f);

    // A change that arrives mid-blend starts from the mixed look: no jump.
    const FS_WeatherLook fogLook;
    const FS_WeatherLook restarted = UCosmeticEnvironmentMathLibrary::lerpLook(half, fogLook, 0.0f);
    TestEqual(TEXT("A restarted blend begins exactly at the mixed look"), restarted.cloudCoverage, half.cloudCoverage);
    TestEqual(TEXT("A restarted blend begins exactly at the mixed look (fog)"), restarted.fogDensity, half.fogDensity);

    // Strength scaling
    const FS_WeatherLook scaledZero = UCosmeticEnvironmentMathLibrary::scaleLookByStrength(clear, storm, 0.0f);
    const FS_WeatherLook scaledFull = UCosmeticEnvironmentMathLibrary::scaleLookByStrength(clear, storm, 100.0f);
    const FS_WeatherLook scaledOver = UCosmeticEnvironmentMathLibrary::scaleLookByStrength(clear, storm, 250.0f);
    TestEqual(TEXT("Strength 0 gives the weak look"), scaledZero.cloudCoverage, clear.cloudCoverage);
    TestEqual(TEXT("Strength 100 gives the strong look"), scaledFull.cloudCoverage, storm.cloudCoverage);
    TestEqual(TEXT("Strength is clamped"), scaledOver.cloudCoverage, storm.cloudCoverage);

    TestEqual(TEXT("A blend time of 0 is finished at once"), UCosmeticEnvironmentMathLibrary::computeBlendAlpha(0.0f, 0.0f), 1.0f);
    TestEqual(TEXT("Halfway through the blend"), UCosmeticEnvironmentMathLibrary::computeBlendAlpha(15.0f, 30.0f), 0.5f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnvironmentUpdateDeltaTest, "Awaken.Environment.UpdateDelta", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEnvironmentUpdateDeltaTest::RunTest(const FString& parameters){
    TestEqual(TEXT("A normal delta passes through"), UCosmeticEnvironmentMathLibrary::clampUpdateDelta(10.25, 10.0, 0.5f), 0.25f);
    TestEqual(TEXT("A 20 second pause advances by at most the maximum"), UCosmeticEnvironmentMathLibrary::clampUpdateDelta(30.0, 10.0, 0.5f), 0.5f);
    TestEqual(TEXT("A clock that went backwards gives 0"), UCosmeticEnvironmentMathLibrary::clampUpdateDelta(9.0, 10.0, 0.5f), 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnvironmentWetnessTest, "Awaken.Environment.Wetness", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEnvironmentWetnessTest::RunTest(const FString& parameters){
    // Rises faster with stronger rain.
    const float weak = UCosmeticEnvironmentMathLibrary::stepWetness(0.0f, 1.0f, 20.0f, 10.0f, 60.0f, 300.0f);
    const float strong = UCosmeticEnvironmentMathLibrary::stepWetness(0.0f, 1.0f, 100.0f, 10.0f, 60.0f, 300.0f);
    TestTrue(TEXT("Strong rain wets faster than weak rain"), strong > weak && weak > 0.0f);
    TestEqual(TEXT("Full rain takes riseSeconds to reach 1"), UCosmeticEnvironmentMathLibrary::stepWetness(0.0f, 1.0f, 100.0f, 60.0f, 60.0f, 300.0f), 1.0f);

    // Never passes the target.
    TestEqual(TEXT("Wetness stops at the target"), UCosmeticEnvironmentMathLibrary::stepWetness(0.79f, 0.8f, 100.0f, 60.0f, 60.0f, 300.0f), 0.8f);

    // Falls slowly once the rain has stopped.
    const float dried = UCosmeticEnvironmentMathLibrary::stepWetness(1.0f, 0.0f, 0.0f, 30.0f, 60.0f, 300.0f);
    TestTrue(TEXT("Wetness falls after the rain stops"), dried < 1.0f);
    TestTrue(TEXT("Wetness falls slower than it rose"), (1.0f - dried) < strong);
    TestEqual(TEXT("It never goes below the target"), UCosmeticEnvironmentMathLibrary::stepWetness(0.01f, 0.0f, 0.0f, 600.0f, 60.0f, 300.0f), 0.0f);

    // No rain, wetness below target: it does not rise on its own.
    TestEqual(TEXT("Without rain wetness does not rise"), UCosmeticEnvironmentMathLibrary::stepWetness(0.2f, 0.8f, 0.0f, 10.0f, 60.0f, 300.0f), 0.2f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnvironmentZoneStackTest, "Awaken.Environment.ZoneStack", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEnvironmentZoneStackTest::RunTest(const FString& parameters){
    const FGuid a = zoneGuid(1);
    const FGuid b = zoneGuid(2);
    const FGuid c = zoneGuid(3);

    FEnvironmentZoneStack stack;
    stack.push(a, makeZone(true, 3.0f), 3.0f, true);
    stack.push(b, makeZone(true, 3.0f), 3.0f, true);
    stack.push(c, makeZone(true, 3.0f), 3.0f, true);
    TestEqual(TEXT("Three zones are active"), stack.getActiveIds().Num(), 3);

    // Same id again: no second entry.
    stack.push(b, makeZone(false, 3.0f), 3.0f, false);
    TestEqual(TEXT("Pushing an existing id adds no second entry"), stack.num(), 3);

    // Pop from the middle: the others keep their order.
    stack.pop(b);
    TArray<FGuid> active = stack.getActiveIds();
    TestEqual(TEXT("Popping from the middle leaves two active"), active.Num(), 2);
    TestTrue(TEXT("The order of the others is kept"), active.Num() == 2 && active[0] == a && active[1] == c);
    TestEqual(TEXT("The popped zone is still fading"), stack.num(), 3);
    TestTrue(TEXT("The stack is moving while a zone fades"), stack.isMoving());

    // An unknown id is ignored.
    stack.pop(zoneGuid(99));
    TestEqual(TEXT("An unknown id changes nothing"), stack.getActiveIds().Num(), 2);

    // The fade finishes and the entry is removed.
    stack.advance(10.0f);
    TestEqual(TEXT("A finished fade is removed"), stack.num(), 2);
    TestFalse(TEXT("The stack stops moving"), stack.isMoving());

    // Nesting: the top zone fully covers the one below, so popping the lower one changes nothing.
    FEnvironmentZoneStack nested;
    nested.push(a, makeZone(true, 2.0f), 2.0f, true);     // hides the sky
    nested.push(b, makeZone(false, 2.0f), 2.0f, true);    // shows the sky, on top
    TestEqual(TEXT("The top zone wins"), nested.layerSkyVisibility(), 1.0f);
    nested.pop(a);
    nested.advance(5.0f);
    TestEqual(TEXT("Popping a zone below a full top zone changes nothing"), nested.layerSkyVisibility(), 1.0f);

    // Popping the top zone fades into the zone below.
    FEnvironmentZoneStack fading;
    fading.push(a, makeZone(false, 2.0f), 2.0f, true);
    fading.push(b, makeZone(true, 2.0f), 2.0f, true);
    TestEqual(TEXT("The top zone hides the sky"), fading.layerSkyVisibility(), 0.0f);
    fading.pop(b);
    fading.advance(1.0f);
    const float midway = fading.layerSkyVisibility();
    TestTrue(TEXT("Midway through the pop the sky is partly back"), midway > 0.4f && midway < 0.6f);
    fading.advance(2.0f);
    TestEqual(TEXT("After the pop the zone below shows"), fading.layerSkyVisibility(), 1.0f);

    // snapAll finishes everything in one go.
    FEnvironmentZoneStack snapped;
    snapped.push(a, makeZone(true, 30.0f), 30.0f, false);
    snapped.snapAll();
    TestEqual(TEXT("snapAll puts the weight at the target"), snapped.layerSkyVisibility(), 0.0f);
    TestFalse(TEXT("Nothing moves after snapAll"), snapped.isMoving());

    // The layers for the post-process components: topmost visible first.
    const FS_ZoneEntry* lower = nullptr;
    const FS_ZoneEntry* upper = nullptr;
    FEnvironmentZoneStack layered;
    layered.push(a, makeZone(true, 1.0f), 1.0f, true);
    layered.push(b, makeZone(true, 1.0f), 1.0f, true);
    layered.getPostProcessLayers(lower, upper);
    TestTrue(TEXT("Upper is the top zone and lower the one below"), upper != nullptr && lower != nullptr && upper->zoneId == b && lower->zoneId == a);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWaterFlowTest, "Awaken.Environment.WaterFlow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWaterFlowTest::RunTest(const FString& parameters){
    const FVector direction(0.0, 2.0, 0.0);   // not unit length: the length must be ignored
    const FVector calm = UCosmeticEnvironmentMathLibrary::computeFlow(direction, 1.0f, 30.0f, 2.0f);
    const FVector strong = UCosmeticEnvironmentMathLibrary::computeFlow(direction, 1.0f, 100.0f, 2.0f);
    const FVector again = UCosmeticEnvironmentMathLibrary::computeFlow(direction, 1.0f, 30.0f, 2.0f);
    TestTrue(TEXT("The same replicated inputs give the same vector on every machine"), calm == again);
    TestEqual(TEXT("The current points along the flow direction"), static_cast<float>(calm.GetSafeNormal().Y), 1.0f);
    TestTrue(TEXT("A stronger river has a faster current"), strong.Size() > calm.Size());
    TestEqual(TEXT("Speed is base times strength times scale"), static_cast<float>(calm.Size()), 60.0f, 0.01f);
    TestTrue(TEXT("A strength of 0 is no current"), UCosmeticEnvironmentMathLibrary::computeFlow(direction, 1.0f, 0.0f, 2.0f).IsNearlyZero());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnvironmentSunTest, "Awaken.Environment.SunDirection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEnvironmentSunTest::RunTest(const FString& parameters){
    TestEqual(TEXT("00:00 is a day fraction of 0"), UCosmeticEnvironmentMathLibrary::normalizeTimeOfDay(0.0f), 0.0f);
    TestEqual(TEXT("12:00 is half a day"), UCosmeticEnvironmentMathLibrary::normalizeTimeOfDay(12.0f), 0.5f);
    TestEqual(TEXT("24:00 wraps to 0"), UCosmeticEnvironmentMathLibrary::normalizeTimeOfDay(24.0f), 0.0f);
    TestEqual(TEXT("A negative time wraps"), UCosmeticEnvironmentMathLibrary::normalizeTimeOfDay(-6.0f), 0.75f);

    const FVector noon = UCosmeticEnvironmentMathLibrary::computeToSunDirection(0.5f, 0.0f, 0.0f);
    const FVector midnight = UCosmeticEnvironmentMathLibrary::computeToSunDirection(0.0f, 0.0f, 0.0f);
    const FVector dawn = UCosmeticEnvironmentMathLibrary::computeToSunDirection(0.25f, 0.0f, 0.0f);
    TestTrue(TEXT("The sun is overhead at noon"), noon.Z > 0.999);
    TestTrue(TEXT("The sun is below the ground at midnight"), midnight.Z < -0.999);
    TestTrue(TEXT("The sun is on the horizon at dawn"), FMath::Abs(dawn.Z) < 0.001);
    TestTrue(TEXT("The direction is a unit vector"), FMath::IsNearlyEqual(static_cast<float>(UCosmeticEnvironmentMathLibrary::computeToSunDirection(0.37f, 40.0f, 20.0f).Size()), 1.0f, 0.001f));

    TestEqual(TEXT("A flash is at full strength at the start"), UCosmeticEnvironmentMathLibrary::computeLightningPulse(0.0f, 0.4f), 1.0f);
    TestEqual(TEXT("A flash is gone after its length"), UCosmeticEnvironmentMathLibrary::computeLightningPulse(0.4f, 0.4f), 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnvironmentNightMinimumTest, "Awaken.Environment.NightMinimum", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEnvironmentNightMinimumTest::RunTest(const FString& parameters){
    auto makeFloatCurve = [](float value){
        UCurveFloat* curve = NewObject<UCurveFloat>(GetTransientPackage());
        curve->FloatCurve.AddKey(0.0f, value);
        curve->FloatCurve.AddKey(1.0f, value);
        return curve;
    };
    auto makeColorCurve = [](float value){
        UCurveLinearColor* curve = NewObject<UCurveLinearColor>(GetTransientPackage());
        for(int32 channel = 0; channel < 3; channel++){
            curve->FloatCurves[channel].AddKey(0.0f, value);
            curve->FloatCurves[channel].AddKey(1.0f, value);
        }
        curve->FloatCurves[3].AddKey(0.0f, 1.0f);
        curve->FloatCurves[3].AddKey(1.0f, 1.0f);
        return curve;
    };

    UDA_EnvironmentCurves* black = NewObject<UDA_EnvironmentCurves>(GetTransientPackage());
    black->moonIntensity = makeFloatCurve(0.0f);
    black->skyTint = makeColorCurve(0.2f);
    TestEqual(TEXT("A moon curve that reaches 0 gives a night minimum of 0 (the data check fails the build)"), black->getNightMinimum(), 0.0f);

    UDA_EnvironmentCurves* dark = NewObject<UDA_EnvironmentCurves>(GetTransientPackage());
    dark->moonIntensity = makeFloatCurve(0.1f);
    dark->skyTint = makeColorCurve(0.2f);
    TestTrue(TEXT("A dark but not black night passes"), dark->getNightMinimum() > 0.0f);

    UDA_EnvironmentCurves* blackSky = NewObject<UDA_EnvironmentCurves>(GetTransientPackage());
    blackSky->moonIntensity = makeFloatCurve(0.1f);
    blackSky->skyTint = makeColorCurve(0.0f);
    TestEqual(TEXT("A sky tint of black also fails"), blackSky->getNightMinimum(), 0.0f);

    UDA_EnvironmentCurves* empty = NewObject<UDA_EnvironmentCurves>(GetTransientPackage());
    TestEqual(TEXT("Missing curves count as 0"), empty->getNightMinimum(), 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGroundTypeMapTest, "Awaken.Environment.GroundTypeMap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FGroundTypeMapTest::RunTest(const FString& parameters){
    UDA_GroundTypeMap* map = NewObject<UDA_GroundTypeMap>(GetTransientPackage());
    map->cellSizeCm = 1000.0f;
    map->originCm = FVector2D(0.0, 0.0);
    map->gridWidth = 2;
    map->gridHeight = 2;
    map->defaultGround = T_Environment_Ground_Grass.GetTag();
    map->palette = { T_Environment_Ground_Mountain.GetTag() };
    map->cells = { 0, UDA_GroundTypeMap::noneIndex, UDA_GroundTypeMap::noneIndex, 0 };

    TestTrue(TEXT("Cell (0, 0) is mountain"), map->sampleAt(FVector(500.0, 500.0, 0.0)) == T_Environment_Ground_Mountain.GetTag());
    TestTrue(TEXT("Cell (1, 0) is unset and gives the default"), map->sampleAt(FVector(1500.0, 500.0, 0.0)) == T_Environment_Ground_Grass.GetTag());
    TestTrue(TEXT("Cell (1, 1) is mountain"), map->sampleAt(FVector(1500.0, 1500.0, 0.0)) == T_Environment_Ground_Mountain.GetTag());
    TestTrue(TEXT("Outside the grid gives the default"), map->sampleAt(FVector(-100.0, 500.0, 0.0)) == T_Environment_Ground_Grass.GetTag());
    TestTrue(TEXT("Far outside the grid gives the default"), map->sampleAt(FVector(99999.0, 99999.0, 0.0)) == T_Environment_Ground_Grass.GetTag());
    return true;
}

#endif
