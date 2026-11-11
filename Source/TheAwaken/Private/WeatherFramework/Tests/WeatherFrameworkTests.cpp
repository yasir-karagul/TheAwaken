// Weather guide Step 18. Tier 1 tests: no world, no game instance. Run: Automation RunTests Awaken.Weather
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WeatherFramework/WeatherMathLibrary.h"
#include "WeatherFramework/WeatherSimulation.h"
#include "WeatherFramework/DA_WeatherTuning.h"
#include "WeatherFramework/WeatherValidation.h"
#include "WeatherFramework/01Structs/S_WeatherFramework.h"
#include "Engine/DataTable.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"
#include "StructUtils/InstancedStruct.h"

namespace{

    // Test values for the numbers no document has written yet (section 7 of this guide).
    UDA_WeatherTuning* makeTestTuning(){
        UDA_WeatherTuning* tuning = NewObject<UDA_WeatherTuning>();
        tuning->snowAccumulatePerHourAtFull = 0.05f;
        tuning->snowMeltPerHour = 0.02f;
        tuning->rainFillUnitsPerHourAtFull = 4.0f;
        return tuning;
    }

    FS_SeasonWeatherRow makeRow(E_WeatherSeasonKey key, E_WeatherType type, float weight, float strengthMin, float strengthMax){
        FS_SeasonWeatherRow row;
        row.seasonKey = key;
        row.weatherType = type;
        row.weight = weight;
        row.strengthMin = strengthMin;
        row.strengthMax = strengthMax;
        // Short periods, so that nine hours hold several of them and the skip crosses real period ends.
        row.durationMinHours = 1.0f;
        row.durationMaxHours = 2.0f;
        return row;
    }

    void setUpSimulation(FWeatherSimulation& simulation, const UDA_WeatherTuning* tuning, int32 seed){
        TArray<FS_SeasonWeatherRow> rows;
        rows.Add(makeRow(E_WeatherSeasonKey::Winter, E_WeatherType::Clear, 30.0f, 0.0f, 0.0f));
        rows.Add(makeRow(E_WeatherSeasonKey::Winter, E_WeatherType::Cloudy, 20.0f, 0.0f, 0.0f));
        rows.Add(makeRow(E_WeatherSeasonKey::Winter, E_WeatherType::Rain, 30.0f, 20.0f, 90.0f));
        rows.Add(makeRow(E_WeatherSeasonKey::Winter, E_WeatherType::Snow, 20.0f, 10.0f, 100.0f));

        simulation.tuning = tuning;
        simulation.setRows(rows);
        simulation.random.Initialize(seed);
        simulation.activeSeasonKey = E_WeatherSeasonKey::Winter;

        FS_WaterBodyState river;
        river.bodyId = FGuid::NewGuid();
        river.hasCurrent = true;
        river.canFreeze = true;
        river.riverStrength = tuning->riverNormalLevel;
        simulation.bodies.Add(river);

        // Start in heavy rain, so the river rise, the rain hours and the fill units are all exercised from the first minute.
        simulation.state.type = E_WeatherType::Rain;
        simulation.state.strength = 80.0f;
        simulation.state.periodEndGameTime = 2 * gameSecondsPerHour;
        simulation.state.periodIndex = 1;
        simulation.lastAdvanceGameTime = 0;
    }

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeatherRiverTest, "Awaken.Weather.River", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWeatherRiverTest::RunTest(const FString& parameters){
    UDA_WeatherTuning* tuning = NewObject<UDA_WeatherTuning>();

    float level = tuning->riverNormalLevel;
    level = UWeatherMathLibrary::stepRiverStrength(level, tuning->riverTargetHeavyRain, 12.0f, *tuning);
    TestEqual(TEXT("Heavy rain takes a river from 30 to 100 in 12 hours"), level, 100.0f, 0.01f);

    level = UWeatherMathLibrary::stepRiverStrength(level, tuning->riverNormalLevel, 24.0f, *tuning);
    TestEqual(TEXT("After rain it is back to normal in 24 hours"), level, 30.0f, 0.01f);

    const float light = UWeatherMathLibrary::stepRiverStrength(30.0f, tuning->riverTargetLightRain, 6.0f, *tuning);
    TestEqual(TEXT("Light rain rises slowly: half of the way to 60 after 6 hours"), light, 45.0f, 0.01f);

    TestEqual(TEXT("A storm always targets 100"), UWeatherMathLibrary::getRiverTarget(E_WeatherType::Storm, 10.0f, *tuning), 100.0f, 0.01f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeatherIceTest, "Awaken.Weather.Ice", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWeatherIceTest::RunTest(const FString& parameters){
    UDA_WeatherTuning* tuning = NewObject<UDA_WeatherTuning>();

    TestEqual(TEXT("Winter freezes in about 3 days"), UWeatherMathLibrary::stepIceLevel(0.0f, E_WeatherSeasonKey::Winter, 72.0f, *tuning), 1.0f, 0.001f);
    TestEqual(TEXT("The Snowy block freezes in about 1 day"), UWeatherMathLibrary::stepIceLevel(0.0f, E_WeatherSeasonKey::Snowy, 24.0f, *tuning), 1.0f, 0.001f);
    TestEqual(TEXT("Spring thaws in about 2 days"), UWeatherMathLibrary::stepIceLevel(1.0f, E_WeatherSeasonKey::Spring, 48.0f, *tuning), 0.0f, 0.001f);

    // The collision switches on at 1 and off only below 1 minus the margin (Index A97, A99).
    TestTrue(TEXT("Level 1 turns the ice solid"), UWeatherMathLibrary::updateIceSolid(false, 1.0f, *tuning));
    TestFalse(TEXT("Just below 1 does not turn it solid"), UWeatherMathLibrary::updateIceSolid(false, 0.99f, *tuning));
    TestTrue(TEXT("Solid ice stays solid inside the margin"), UWeatherMathLibrary::updateIceSolid(true, 1.0f - tuning->iceThawMargin * 0.5f, *tuning));
    TestFalse(TEXT("Solid ice thaws below the margin"), UWeatherMathLibrary::updateIceSolid(true, 1.0f - tuning->iceThawMargin * 2.0f, *tuning));

    // World creation: Winter day d gives min(1, d / 3) (B14 3.7).
    TestEqual(TEXT("Winter day 2 gives 2/3"), UWeatherMathLibrary::deriveCalendarLevel(E_WeatherSeasonKey::Winter, 2, 0, 0, 3.0f, 1.0f, 2.0f), 2.0f / 3.0f, 0.001f);
    TestEqual(TEXT("Winter day 5 is capped at 1"), UWeatherMathLibrary::deriveCalendarLevel(E_WeatherSeasonKey::Winter, 5, 0, 0, 3.0f, 1.0f, 2.0f), 1.0f, 0.001f);
    return true;
}

// The central claim of the framework (W-IG4): a skip equals the same time passing normally.
// One simulation advances in 540 one-minute steps, the other in one nine-hour jump, with the same seed.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeatherSkipEqualsTimeTest, "Awaken.Weather.SkipEqualsTime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWeatherSkipEqualsTimeTest::RunTest(const FString& parameters){
    UDA_WeatherTuning* tuning = makeTestTuning();
    const int64 nineHours = 9 * gameSecondsPerHour;
    const int32 seeds[] = {1, 7, 42, 1234, 99001};

    for(const int32 seed : seeds){
        FWeatherSimulation stepped;
        FWeatherSimulation skipped;
        setUpSimulation(stepped, tuning, seed);
        setUpSimulation(skipped, tuning, seed);

        for(int64 time = 60; time <= nineHours; time += 60){
            stepped.advanceTo(time);
        }
        skipped.advanceTo(nineHours);

        const FString label = FString::Printf(TEXT("seed %d"), seed);
        TestEqual(*(label + TEXT(": weather type")), static_cast<int32>(stepped.state.type), static_cast<int32>(skipped.state.type));
        TestEqual(*(label + TEXT(": period index")), stepped.state.periodIndex, skipped.state.periodIndex);
        TestEqual(*(label + TEXT(": period end")), stepped.state.periodEndGameTime, skipped.state.periodEndGameTime);
        TestEqual(*(label + TEXT(": strength")), stepped.state.strength, skipped.state.strength, 0.01f);
        TestEqual(*(label + TEXT(": snow level")), stepped.state.snowLevel, skipped.state.snowLevel, 0.001f);
        TestEqual(*(label + TEXT(": river strength")), stepped.bodies[0].riverStrength, skipped.bodies[0].riverStrength, 0.01f);
        TestEqual(*(label + TEXT(": ice level")), stepped.bodies[0].iceLevel, skipped.bodies[0].iceLevel, 0.001f);
        TestEqual(*(label + TEXT(": ice solid")), stepped.bodies[0].iceSolid, skipped.bodies[0].iceSolid);
        TestEqual(*(label + TEXT(": rain fill units")), stepped.pendingFillUnits, skipped.pendingFillUnits, 0.01f);

        const float steppedRain = stepped.rainHoursByDay.Contains(0) ? stepped.rainHoursByDay[0] : 0.0f;
        const float skippedRain = skipped.rainHoursByDay.Contains(0) ? skipped.rainHoursByDay[0] : 0.0f;
        TestEqual(*(label + TEXT(": rain hours")), steppedRain, skippedRain, 0.01f);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeatherSeasonRulesTest, "Awaken.Weather.SeasonRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWeatherSeasonRulesTest::RunTest(const FString& parameters){
    UDA_WeatherTuning* tuning = makeTestTuning();
    FWeatherSimulation simulation;
    setUpSimulation(simulation, tuning, 5);

    TArray<FS_SeasonWeatherRow> rows;
    rows.Add(makeRow(E_WeatherSeasonKey::Winter, E_WeatherType::Clear, 50.0f, 0.0f, 0.0f));
    rows.Add(makeRow(E_WeatherSeasonKey::Winter, E_WeatherType::Rain, 25.0f, 0.0f, 34.0f));
    rows.Add(makeRow(E_WeatherSeasonKey::Winter, E_WeatherType::Snow, 25.0f, 0.0f, 34.0f));
    rows.Add(makeRow(E_WeatherSeasonKey::Snowy, E_WeatherType::Snow, 85.0f, 35.0f, 100.0f));
    rows.Add(makeRow(E_WeatherSeasonKey::Snowy, E_WeatherType::Blizzard, 15.0f, 100.0f, 100.0f));
    simulation.setRows(rows);

    TestTrue(TEXT("Rain is allowed in Winter"), simulation.isTypeAllowed(E_WeatherSeasonKey::Winter, E_WeatherType::Rain));
    TestFalse(TEXT("Rain does not survive into the Snowy block"), simulation.isTypeAllowed(E_WeatherSeasonKey::Snowy, E_WeatherType::Rain));
    TestTrue(TEXT("Snow continues into the Snowy block"), simulation.isTypeAllowed(E_WeatherSeasonKey::Snowy, E_WeatherType::Snow));
    TestFalse(TEXT("The Snowy block has no Clear"), simulation.isTypeAllowed(E_WeatherSeasonKey::Snowy, E_WeatherType::Clear));

    // Strength bands: below 35 light, 35 to 70 normal, above 70 heavy (B14 3.7).
    TestTrue(TEXT("34 is Light"), UWeatherMathLibrary::getStrengthBand(34.0f, *tuning) == E_WeatherStrengthBand::Light);
    TestTrue(TEXT("35 is Normal"), UWeatherMathLibrary::getStrengthBand(35.0f, *tuning) == E_WeatherStrengthBand::Normal);
    TestTrue(TEXT("70 is Normal"), UWeatherMathLibrary::getStrengthBand(70.0f, *tuning) == E_WeatherStrengthBand::Normal);
    TestTrue(TEXT("71 is Heavy"), UWeatherMathLibrary::getStrengthBand(71.0f, *tuning) == E_WeatherStrengthBand::Heavy);

    // The table pick follows the weights.
    FRandomStream stream(5);
    int32 rainDraws = 0;
    for(int32 draw = 0; draw < 10000; draw++){
        const FS_SeasonWeatherRow* picked = UWeatherMathLibrary::pickWeightedRow(*simulation.rowsBySeason.Find(E_WeatherSeasonKey::Winter), stream.FRand());
        if(picked != nullptr && picked->weatherType == E_WeatherType::Rain){
            rainDraws++;
        }
    }
    TestTrue(TEXT("Rain is drawn about 25 percent of the time"), FMath::Abs(rainDraws - 2500) < 200);
    return true;
}


// ---------------- Cold monotonic (Step 18.2, tier 1) ----------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeatherColdMonotonicTest, "Awaken.Weather.ColdMonotonic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWeatherColdMonotonicTest::RunTest(const FString& parameters){
    UDA_WeatherTuning* tuning = NewObject<UDA_WeatherTuning>();
    tuning->rainColdAddendAtFull = 0.5f;
    tuning->snowDepthColdFactor = 1.0f;
    tuning->shelteredColdMultiplier = 0.6f;
    tuning->indoorsColdMultiplier = 0.3f;
    tuning->fireColdMultiplier = 0.5f;

    const float base = 1.0f;
    FS_ShelterState open;
    FS_ShelterState roofed;
    roofed.shelterValue = 1.0f;
    FS_ShelterState half;
    half.shelterValue = 0.5f;
    FS_ShelterState indoors;
    indoors.isIndoors = true;
    indoors.shelterValue = 1.0f;
    FS_ShelterState openByFire;
    openByFire.nearFire = true;

    const float openRate = UWeatherMathLibrary::getColdRate(base, 0.0f, 0.0f, open, *tuning);
    const float halfRate = UWeatherMathLibrary::getColdRate(base, 0.0f, 0.0f, half, *tuning);
    const float roofedRate = UWeatherMathLibrary::getColdRate(base, 0.0f, 0.0f, roofed, *tuning);
    const float indoorsRate = UWeatherMathLibrary::getColdRate(base, 0.0f, 0.0f, indoors, *tuning);

    TestTrue(TEXT("open air with no rain or snow is the base rate"), FMath::IsNearlyEqual(openRate, base));
    TestTrue(TEXT("more cover never means more cold: open >= half >= roofed >= indoors"), openRate >= halfRate && halfRate >= roofedRate && roofedRate >= indoorsRate);
    TestTrue(TEXT("a roof lowers the rate"), roofedRate < openRate);
    TestTrue(TEXT("indoors is the lowest"), indoorsRate < roofedRate);
    TestTrue(TEXT("a fire lowers the rate"), UWeatherMathLibrary::getColdRate(base, 0.0f, 0.0f, openByFire, *tuning) < openRate);

    float previous = UWeatherMathLibrary::getColdRate(base, 0.0f, 0.0f, open, *tuning);
    bool snowRises = true;
    for(int32 step = 1; step <= 10; ++step){
        const float rate = UWeatherMathLibrary::getColdRate(base, 0.0f, step / 10.0f, open, *tuning);
        snowRises = snowRises && rate >= previous;
        previous = rate;
    }
    TestTrue(TEXT("deeper snow never lowers the rate"), snowRises);

    previous = openRate;
    bool rainRises = true;
    for(int32 step = 1; step <= 10; ++step){
        const float rate = UWeatherMathLibrary::getColdRate(base, step / 10.0f, 0.0f, open, *tuning);
        rainRises = rainRises && rate >= previous;
        previous = rate;
    }
    TestTrue(TEXT("harder rain never lowers the rate in the open"), rainRises);

    const float rainOpen = UWeatherMathLibrary::getColdRate(base, 1.0f, 0.0f, open, *tuning);
    const float rainRoofed = UWeatherMathLibrary::getColdRate(base, 1.0f, 0.0f, roofed, *tuning);
    TestTrue(TEXT("a full roof takes the rain part away: only the sheltered multiplier remains"), FMath::IsNearlyEqual(rainRoofed, base * tuning->shelteredColdMultiplier, 0.0001f));
    TestTrue(TEXT("rain in the open is colder than dry"), rainOpen > openRate);

    // Every cold coefficient set to 0 on purpose, so this does not depend on the defaults of the tuning asset.
    UDA_WeatherTuning* zero = NewObject<UDA_WeatherTuning>();
    zero->rainColdAddendAtFull = 0.0f;
    zero->snowDepthColdFactor = 0.0f;
    TestTrue(TEXT("a base rate of 0 with no rain or snow coefficients gives 0 for every input"), FMath::IsNearlyZero(UWeatherMathLibrary::getColdRate(0.0f, 1.0f, 1.0f, open, *zero)));
    TestTrue(TEXT("the rate is never negative"), UWeatherMathLibrary::getColdRate(-1.0f, 0.0f, 0.0f, open, *tuning) >= 0.0f);
    return true;
}

// ---------------- Data check (Step 18.2, tier 1) ----------------
#if WITH_EDITOR
namespace{

    // A season table built in memory from CSV lines (the format of Step 6.3), so the check needs no asset.
    UDataTable* makeSeasonTable(const TArray<FString>& lines){
        UDataTable* table = NewObject<UDataTable>(GetTransientPackage());
        table->RowStruct = FS_SeasonWeatherRow::StaticStruct();
        FString csv = TEXT("---,seasonKey,weatherType,weight,strengthMin,strengthMax,durationMinHours,durationMaxHours\n");
        for(const FString& line : lines){
            csv += line + TEXT("\n");
        }
        table->CreateTableFromCSVString(csv);
        return table;
    }

    TArray<FString> goodLines(){
        TArray<FString> lines;
        for(const TCHAR* season : {TEXT("Spring"), TEXT("Summer"), TEXT("Fall"), TEXT("Winter")}){
            lines.Add(FString::Printf(TEXT("%s_clear,%s,Clear,60,0,0,0,0"), season, season));
            lines.Add(FString::Printf(TEXT("%s_rain,%s,Rain,40,10,60,1,2"), season, season));
        }
        lines.Add(TEXT("Snowy_snow,Snowy,Snow,100,10,100,1,2"));
        return lines;
    }

    bool hasMessage(const TArray<FString>& messages, const TCHAR* part){
        return messages.ContainsByPredicate([part](const FString& message){ return message.Contains(part); });
    }

}
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeatherDataCheckTest, "Awaken.Weather.DataCheck", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWeatherDataCheckTest::RunTest(const FString& parameters){
#if WITH_EDITOR
    UDA_WeatherTuning* tuning = NewObject<UDA_WeatherTuning>();
    TArray<FString> errors;
    TArray<FString> warnings;

    UDataTable* good = makeSeasonTable(goodLines());
    TestTrue(TEXT("the baseline table is valid"), WeatherValidation::validateWeatherData(good, tuning, errors, warnings));
    TestEqual(TEXT("the baseline table has no errors"), errors.Num(), 0);

    {
        TArray<FString> lines = goodLines();
        lines.Add(TEXT("bad_weight,Winter,Clear,-5,0,0,0,0"));
        errors.Reset();
        warnings.Reset();
        TestFalse(TEXT("a negative weight fails"), WeatherValidation::validateWeatherData(makeSeasonTable(lines), tuning, errors, warnings));
        TestTrue(TEXT("the error names the row"), hasMessage(errors, TEXT("bad_weight")));
    }
    {
        TArray<FString> lines = goodLines();
        lines.Add(TEXT("bad_rain,Winter,Rain,5,0,0,0,0"));
        errors.Reset();
        warnings.Reset();
        TestFalse(TEXT("a rain row with strength 0 fails"), WeatherValidation::validateWeatherData(makeSeasonTable(lines), tuning, errors, warnings));
        TestTrue(TEXT("the error names the row"), hasMessage(errors, TEXT("bad_rain")));
    }
    {
        TArray<FString> lines = goodLines();
        lines.Add(TEXT("bad_range,Winter,Snow,5,80,20,0,0"));
        errors.Reset();
        warnings.Reset();
        TestFalse(TEXT("strengthMin above strengthMax fails"), WeatherValidation::validateWeatherData(makeSeasonTable(lines), tuning, errors, warnings));
        TestTrue(TEXT("the error names the row"), hasMessage(errors, TEXT("bad_range")));
    }
    {
        TArray<FString> lines = goodLines();
        lines.Add(TEXT("bad_snowy,Snowy,Clear,10,0,0,0,0"));
        errors.Reset();
        warnings.Reset();
        TestFalse(TEXT("Clear in the Snowy block fails"), WeatherValidation::validateWeatherData(makeSeasonTable(lines), tuning, errors, warnings));
        TestTrue(TEXT("the error names the row"), hasMessage(errors, TEXT("bad_snowy")));
    }
    {
        TArray<FString> lines = goodLines();
        lines.Add(TEXT("bad_strength,Winter,Rain,5,10,120,0,0"));
        errors.Reset();
        warnings.Reset();
        TestFalse(TEXT("a strength of 120 fails"), WeatherValidation::validateWeatherData(makeSeasonTable(lines), tuning, errors, warnings));
        TestTrue(TEXT("the error names the row"), hasMessage(errors, TEXT("bad_strength")));
    }
    {
        // Every Summer row has weight 0: the season has rows, but nothing can be drawn from them.
        TArray<FString> lines;
        for(const FString& line : goodLines()){
            if(line.StartsWith(TEXT("Summer_clear"))){
                lines.Add(TEXT("Summer_clear,Summer,Clear,0,0,0,0,0"));
            }else if(line.StartsWith(TEXT("Summer_rain"))){
                lines.Add(TEXT("Summer_rain,Summer,Rain,0,10,60,1,2"));
            }else{
                lines.Add(line);
            }
        }
        errors.Reset();
        warnings.Reset();
        TestFalse(TEXT("a season whose weights are all 0 fails"), WeatherValidation::validateWeatherData(makeSeasonTable(lines), tuning, errors, warnings));
        TestTrue(TEXT("the error names the season key"), hasMessage(errors, TEXT("Season key")));
    }
    {
        TArray<FString> lines;
        for(const FString& line : goodLines()){
            if(!line.StartsWith(TEXT("Summer"))){
                lines.Add(line);
            }
        }
        errors.Reset();
        warnings.Reset();
        TestFalse(TEXT("a season key with no rows fails"), WeatherValidation::validateWeatherData(makeSeasonTable(lines), tuning, errors, warnings));
        TestTrue(TEXT("the error names the season key"), hasMessage(errors, TEXT("Season key")));
    }
    {
        TArray<FString> lines = goodLines();
        lines.Add(TEXT("extra,Winter,Clear,10,0,0,0,0"));
        errors.Reset();
        warnings.Reset();
        TestTrue(TEXT("weights that do not sum to 100 only warn"), WeatherValidation::validateWeatherData(makeSeasonTable(lines), tuning, errors, warnings));
        TestTrue(TEXT("and the warning says so"), hasMessage(warnings, TEXT("weights sum")));
    }
    {
        UDA_WeatherTuning* broken = NewObject<UDA_WeatherTuning>();
        broken->lightBelow = 80.0f;
        broken->heavyAbove = 40.0f;
        errors.Reset();
        warnings.Reset();
        TestFalse(TEXT("lightBelow above heavyAbove fails"), WeatherValidation::validateWeatherData(good, broken, errors, warnings));
        TestTrue(TEXT("the error names the setting"), hasMessage(errors, TEXT("lightBelow")));
    }
    {
        errors.Reset();
        warnings.Reset();
        TestFalse(TEXT("a missing table fails"), WeatherValidation::validateWeatherData(nullptr, tuning, errors, warnings));
        TestFalse(TEXT("a missing tuning asset fails"), WeatherValidation::validateWeatherData(good, nullptr, errors, warnings));
    }
#else
    AddInfo(TEXT("The data check test builds its table with an editor-only call; skipped in a non-editor build"));
#endif
    return true;
}

// ---------------- Save record (struct level part of the Save round trip, Step 18.2) ----------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeatherSaveRecordTest, "Awaken.Weather.SaveRecord", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWeatherSaveRecordTest::RunTest(const FString& parameters){
    FS_WeatherSaveRecord record;
    record.type = E_WeatherType::Snow;
    record.strength = 42.0f;
    record.periodEndGameTime = 987654321012;
    record.snowLevel = 0.65f;

    FS_WaterBodySaveRecord body;
    body.bodyId = FGuid::NewGuid();
    body.iceLevel = 0.97f;
    body.riverStrength = 1.4f;
    body.iceSolid = true;
    body.hasCurrent = true;
    body.canFreeze = true;
    record.waterBodies.Add(body);

    FS_RainDayRecord day;
    day.dayNumber = 111;
    day.rainHours = 5.5f;
    record.rainDays.Add(day);

    const FInstancedStruct wrapped = FInstancedStruct::Make(record);
    const FS_WeatherSaveRecord* copy = wrapped.GetPtr<FS_WeatherSaveRecord>();
    TestNotNull(TEXT("the record comes back out of the instanced struct"), copy);
    if(copy != nullptr){
        TestEqual(TEXT("version 1"), copy->version, 1);
        TestEqual(TEXT("type"), copy->type, record.type);
        TestTrue(TEXT("strength"), FMath::IsNearlyEqual(copy->strength, record.strength));
        TestEqual(TEXT("periodEndGameTime"), copy->periodEndGameTime, record.periodEndGameTime);
        TestTrue(TEXT("snowLevel"), FMath::IsNearlyEqual(copy->snowLevel, record.snowLevel));
        TestEqual(TEXT("one water body"), copy->waterBodies.Num(), 1);
        if(copy->waterBodies.Num() == 1){
            const FS_WaterBodySaveRecord& back = copy->waterBodies[0];
            TestTrue(TEXT("the body id survives"), back.bodyId == body.bodyId);
            TestTrue(TEXT("ice level, strength and flags survive"), FMath::IsNearlyEqual(back.iceLevel, body.iceLevel) && FMath::IsNearlyEqual(back.riverStrength, body.riverStrength) && back.iceSolid && back.hasCurrent && back.canFreeze);
        }
        TestEqual(TEXT("one rain day"), copy->rainDays.Num(), 1);
        if(copy->rainDays.Num() == 1){
            TestTrue(TEXT("the rain day survives"), copy->rainDays[0].dayNumber == 111 && FMath::IsNearlyEqual(copy->rainDays[0].rainHours, 5.5f));
        }
    }

    // Every field of the record and of the nested records must carry SaveGame: SaveGameToMemory skips the others (Step 4).
    auto allSaveGame = [](const UStruct* type){
        for(TFieldIterator<FProperty> iterator(type); iterator; ++iterator){
            if(!iterator->HasAnyPropertyFlags(CPF_SaveGame)){
                return false;
            }
        }
        return true;
    };
    TestTrue(TEXT("every field of FS_WeatherSaveRecord is SaveGame"), allSaveGame(FS_WeatherSaveRecord::StaticStruct()));
    TestTrue(TEXT("every field of FS_WaterBodySaveRecord is SaveGame"), allSaveGame(FS_WaterBodySaveRecord::StaticStruct()));
    TestTrue(TEXT("every field of FS_RainDayRecord is SaveGame"), allSaveGame(FS_RainDayRecord::StaticStruct()));
    return true;
}

#endif
