// Time guide 15.4, tests 1 and 2 (tier 1: no world, no game instance), plus a check of the save record and the settings maths.
// Tests 3 to 6 need a world (Standalone, then listen server with clients) and are run by hand with the commands of 15.1.
// Run: Automation RunTests Awaken.Time
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Math/RandomStream.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"
#include "StructUtils/InstancedStruct.h"
#include "TimeFramework/DA_TimeSettings.h"
#include "TimeFramework/TimeMathLibrary.h"
#include "TimeFramework/01Structs/S_TimeFramework.h"

namespace{
    FS_TimeCalendarConfig makeConfig(int32 startHour, int64 seasonStartOffset){
        FS_TimeCalendarConfig config;
        config.startOffsetSeconds = startHour * 3600;
        config.wakeSeconds = 5 * 3600;
        config.daysPerSeason = 30;
        config.seasonStartOffset = seasonStartOffset;
        return config;
    }
}

// ---- Test 1: clock arithmetic -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTimeClockMathTest, "Awaken.Time.ClockMath", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTimeClockMathTest::RunTest(const FString& parameters){
    // floorDiv and floorMod floor, also for negative numbers
    TestEqual(TEXT("floorDiv(7, 2)"), UTimeMathLibrary::floorDiv(7, 2), static_cast<int64>(3));
    TestEqual(TEXT("floorDiv(-1, 86400)"), UTimeMathLibrary::floorDiv(-1, 86400), static_cast<int64>(-1));
    TestEqual(TEXT("floorDiv(-86400, 86400)"), UTimeMathLibrary::floorDiv(-86400, 86400), static_cast<int64>(-1));
    TestEqual(TEXT("floorDiv(-86401, 86400)"), UTimeMathLibrary::floorDiv(-86401, 86400), static_cast<int64>(-2));
    TestEqual(TEXT("floorMod(-1, 86400)"), UTimeMathLibrary::floorMod(-1, 86400), static_cast<int64>(86399));

    // The six worked examples of Step 6
    {
        const FS_CalendarDate date = UTimeMathLibrary::deriveCalendar(0, makeConfig(7, 0));
        TestEqual(TEXT("07:00 start: time of day"), static_cast<int64>(date.timeOfDaySeconds), static_cast<int64>(7 * 3600));
        TestEqual(TEXT("07:00 start: daysPassed 0"), date.daysPassed, static_cast<int64>(0));
        TestTrue(TEXT("07:00 start: Spring day 0"), date.season == E_Season::Spring && date.dayInSeason == 0);
    }
    {
        const FS_CalendarDate date = UTimeMathLibrary::deriveCalendar(79200, makeConfig(7, 0));
        TestEqual(TEXT("22 h later: 05:00"), static_cast<int64>(date.timeOfDaySeconds), static_cast<int64>(5 * 3600));
        TestEqual(TEXT("22 h later: daysPassed 1"), date.daysPassed, static_cast<int64>(1));
    }
    {
        const FS_CalendarDate date = UTimeMathLibrary::deriveCalendar(0, makeConfig(3, 0));
        TestEqual(TEXT("03:00 start: time of day"), static_cast<int64>(date.timeOfDaySeconds), static_cast<int64>(3 * 3600));
        TestEqual(TEXT("03:00 start: daysPassed 0 (a negative intermediate value)"), date.daysPassed, static_cast<int64>(0));
    }
    {
        const FS_CalendarDate date = UTimeMathLibrary::deriveCalendar(7200, makeConfig(3, 0));
        TestEqual(TEXT("03:00 start, 2 h later: 05:00"), static_cast<int64>(date.timeOfDaySeconds), static_cast<int64>(5 * 3600));
        TestEqual(TEXT("03:00 start, 2 h later: daysPassed 1"), date.daysPassed, static_cast<int64>(1));
    }
    {
        const FS_CalendarDate date = UTimeMathLibrary::deriveCalendar(0, makeConfig(7, 119));
        TestTrue(TEXT("offset 119: Winter day 29"), date.season == E_Season::Winter && date.dayInSeason == 29);
        TestEqual(TEXT("offset 119: cycle 0"), date.cycleNumber, static_cast<int64>(0));
    }
    {
        const FS_CalendarDate date = UTimeMathLibrary::deriveCalendar(79200, makeConfig(7, 119));
        TestEqual(TEXT("offset 119, 22 h later: daysPassed 1"), date.daysPassed, static_cast<int64>(1));
        TestTrue(TEXT("offset 119, 22 h later: Spring day 0"), date.season == E_Season::Spring && date.dayInSeason == 0);
        TestEqual(TEXT("offset 119, 22 h later: cycle 1"), date.cycleNumber, static_cast<int64>(1));
    }

    // Seasons across a boundary: every 30 days the season advances, and a full cycle is 120 days
    {
        const FS_TimeCalendarConfig config = makeConfig(7, 0);
        const E_Season expected[4] = {E_Season::Spring, E_Season::Summer, E_Season::Fall, E_Season::Winter};
        for(int32 index = 0; index < 4; index++){
            const FS_CalendarDate first = UTimeMathLibrary::deriveCalendar(UTimeMathLibrary::dayStartGameTime(index * 30, config), config);
            const FS_CalendarDate last = UTimeMathLibrary::deriveCalendar(UTimeMathLibrary::dayStartGameTime(index * 30 + 29, config), config);
            TestTrue(*FString::Printf(TEXT("season slot %d starts on day 0"), index), first.season == expected[index] && first.dayInSeason == 0);
            TestTrue(*FString::Printf(TEXT("season slot %d ends on day 29"), index), last.season == expected[index] && last.dayInSeason == 29);
        }
        const FS_CalendarDate nextYear = UTimeMathLibrary::deriveCalendar(UTimeMathLibrary::dayStartGameTime(120, config), config);
        TestTrue(TEXT("day 120 is Spring day 0 of cycle 1"), nextYear.season == E_Season::Spring && nextYear.dayInSeason == 0 && nextYear.cycleNumber == 1);
    }

    // nextWakeGameTime is always strictly later, also when called exactly at 05:00
    {
        const FS_TimeCalendarConfig config = makeConfig(7, 0);
        TestEqual(TEXT("from 07:00 the next wake is 22 h later"), UTimeMathLibrary::nextWakeGameTime(0, config), static_cast<int64>(79200));
        TestEqual(TEXT("called exactly at 05:00 it gives the next day"), UTimeMathLibrary::nextWakeGameTime(79200, config), static_cast<int64>(79200 + 86400));
        TestTrue(TEXT("one second before 05:00 it is 05:00"), UTimeMathLibrary::nextWakeGameTime(79199, config) == 79200);
        for(int64 time = -100000; time < 400000; time += 7919){
            TestTrue(*FString::Printf(TEXT("nextWake(%lld) is later"), time), UTimeMathLibrary::nextWakeGameTime(time, config) > time);
        }
    }

    // dayStartGameTime(1) is the 05:00 of day 1 for both a 07:00 and a 03:00 start
    for(int32 startHour : {7, 3}){
        const FS_TimeCalendarConfig config = makeConfig(startHour, 0);
        const int64 start = UTimeMathLibrary::dayStartGameTime(1, config);
        const FS_CalendarDate date = UTimeMathLibrary::deriveCalendar(start, config);
        TestTrue(*FString::Printf(TEXT("dayStart(1) with a %02d:00 start is 05:00 on day 1"), startHour), date.daysPassed == 1 && date.timeOfDaySeconds == 5 * 3600);
        const FS_CalendarDate before = UTimeMathLibrary::deriveCalendar(start - 1, config);
        TestEqual(*FString::Printf(TEXT("one second before dayStart(1) with a %02d:00 start is still day 0"), startHour), before.daysPassed, static_cast<int64>(0));
    }
    return true;
}

// ---- Test 2: Snowy draw rules -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTimeSnowyDrawTest, "Awaken.Time.SnowyDraw", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTimeSnowyDrawTest::RunTest(const FString& parameters){
    constexpr int32 winterLength = 30;
    constexpr int32 minDays = 5;
    FRandomStream stream(12345);

    // A normal Winter: 10,000 draws
    bool allValid = true;
    bool lengthOk = true;
    bool endsInside = true;
    bool startsOk = true;
    int32 earliest = winterLength;
    int32 latest = 0;
    for(int32 index = 0; index < 10000; index++){
        const FS_SnowyDraw draw = UTimeMathLibrary::drawSnowyBlock(stream, winterLength, minDays, 4, 0);
        allValid = allValid && draw.isValid;
        lengthOk = lengthOk && draw.lengthDays >= minDays;
        endsInside = endsInside && draw.startDay + draw.lengthDays <= winterLength;   // the last day of the block is day 29 at the latest
        startsOk = startsOk && draw.startDay >= 0 && draw.startDay <= winterLength - minDays && draw.winterCycle == 4;
        earliest = FMath::Min(earliest, draw.startDay);
        latest = FMath::Max(latest, draw.startDay);
    }
    TestTrue(TEXT("a normal Winter always gets a block"), allValid);
    TestTrue(TEXT("the block lasts at least 5 days"), lengthOk);
    TestTrue(TEXT("the block ends inside Winter, on day 29 at the latest"), endsInside);
    TestTrue(TEXT("the start is between day 0 and day 25, and the draw remembers its Winter"), startsOk);
    TestTrue(TEXT("the start is spread over the allowed days"), earliest <= 1 && latest >= winterLength - minDays - 1);

    // Creation inside Winter: only later days count (the argument is the first allowed day, which is today + 1)
    {
        const FS_SnowyDraw draw = UTimeMathLibrary::drawSnowyBlock(stream, winterLength, minDays, 0, 25);
        TestTrue(TEXT("created on Winter day 24: still room for exactly 5 days"), draw.isValid && draw.startDay == 25 && draw.lengthDays == 5);
    }
    {
        const FS_SnowyDraw draw = UTimeMathLibrary::drawSnowyBlock(stream, winterLength, minDays, 0, 26);
        TestFalse(TEXT("created on Winter day 25: fewer than 5 full days left, no block"), draw.isValid);
    }
    {
        bool later = true;
        for(int32 index = 0; index < 1000; index++){
            const FS_SnowyDraw draw = UTimeMathLibrary::drawSnowyBlock(stream, winterLength, minDays, 0, 12);
            later = later && draw.isValid && draw.startDay >= 12;
        }
        TestTrue(TEXT("a block drawn at creation never starts before the first allowed day"), later);
    }

    // The flag is one continuous run: on once, off once inside Winter
    {
        FS_SnowyDraw draw;
        draw.isValid = true;
        draw.winterCycle = 3;
        draw.startDay = 10;
        draw.lengthDays = 7;
        int32 turnedOn = 0;
        int32 turnedOff = 0;
        bool previous = false;
        for(int32 day = 0; day < winterLength; day++){
            FS_CalendarDate date;
            date.season = E_Season::Winter;
            date.cycleNumber = 3;
            date.dayInSeason = day;
            const bool snowy = UTimeMathLibrary::isSnowyDay(draw, date);
            turnedOn += (snowy && !previous) ? 1 : 0;
            turnedOff += (!snowy && previous) ? 1 : 0;
            TestTrue(*FString::Printf(TEXT("day %d snowy flag"), day), snowy == (day >= 10 && day < 17));
            previous = snowy;
        }
        TestEqual(TEXT("the flag turns on once"), turnedOn, 1);
        TestEqual(TEXT("the flag turns off once, inside Winter"), turnedOff, 1);

        FS_CalendarDate otherWinter;
        otherWinter.season = E_Season::Winter;
        otherWinter.cycleNumber = 4;
        otherWinter.dayInSeason = 12;
        TestFalse(TEXT("a draw does not apply to the next Winter"), UTimeMathLibrary::isSnowyDay(draw, otherWinter));

        FS_CalendarDate fall;
        fall.season = E_Season::Fall;
        fall.cycleNumber = 3;
        fall.dayInSeason = 12;
        TestFalse(TEXT("Snowy never happens outside Winter"), UTimeMathLibrary::isSnowyDay(draw, fall));
    }
    {
        FS_SnowyDraw none;
        FS_CalendarDate date;
        date.season = E_Season::Winter;
        date.dayInSeason = 12;
        TestFalse(TEXT("an invalid draw is never snowy"), UTimeMathLibrary::isSnowyDay(none, date));
    }
    return true;
}

// ---- Save record and settings maths ----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTimeSaveRecordTest, "Awaken.Time.SaveRecord", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTimeSaveRecordTest::RunTest(const FString& parameters){
    FS_TimeSaveRecord record;
    record.gameTime = 123456789012;
    record.startOffsetSeconds = 10800;
    record.seasonStartOffset = 95;
    record.snowyDraw.isValid = true;
    record.snowyDraw.winterCycle = 2;
    record.snowyDraw.startDay = 11;
    record.snowyDraw.lengthDays = 9;

    const FInstancedStruct wrapped = FInstancedStruct::Make(record);
    const FS_TimeSaveRecord* copy = wrapped.GetPtr<FS_TimeSaveRecord>();
    TestNotNull(TEXT("the record comes back out of the instanced struct"), copy);
    if(copy != nullptr){
        TestEqual(TEXT("gameTime"), copy->gameTime, record.gameTime);
        TestEqual(TEXT("startOffsetSeconds"), copy->startOffsetSeconds, record.startOffsetSeconds);
        TestEqual(TEXT("seasonStartOffset"), copy->seasonStartOffset, record.seasonStartOffset);
        TestTrue(TEXT("the Snowy draw survives"), copy->snowyDraw.isValid && copy->snowyDraw.winterCycle == 2 && copy->snowyDraw.startDay == 11 && copy->snowyDraw.lengthDays == 9);
        TestEqual(TEXT("version 1"), copy->version, 1);
    }

    // Every field of the record, and of the nested draw, must carry SaveGame: SaveGameToMemory skips the others (Step 4)
    auto allSaveGame = [](const UStruct* type){
        for(TFieldIterator<FProperty> iterator(type); iterator; ++iterator){
            if(!iterator->HasAnyPropertyFlags(CPF_SaveGame)){
                return false;
            }
        }
        return true;
    };
    TestTrue(TEXT("every field of FS_TimeSaveRecord is SaveGame"), allSaveGame(FS_TimeSaveRecord::StaticStruct()));
    TestTrue(TEXT("every field of FS_SnowyDraw is SaveGame"), allSaveGame(FS_SnowyDraw::StaticStruct()));

    UDA_TimeSettings* settings = NewObject<UDA_TimeSettings>(GetTransientPackage());
    TestTrue(TEXT("one real hour is one game day: 24 game seconds per real second"), FMath::IsNearlyEqual(settings->gameSecondsPerRealSecond(), 24.0f));
    TestTrue(TEXT("a game hour is 2.5 real minutes"), FMath::IsNearlyEqual(3600.0f / settings->gameSecondsPerRealSecond(), 150.0f));
    TestTrue(TEXT("the default fade is shorter than one game hour"), settings->skipFadeOutSeconds * settings->gameSecondsPerRealSecond() < 3600.0f);
    return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
