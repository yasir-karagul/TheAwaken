// Non-shipping dev commands of Step 15.1.
// NOT in the guide's code: the guide describes these commands in a table only. Written from that table.
// They register straight with the console, so they do not need the Doc B22 cheat router. When B22 exists,
// move the registration there and keep the UTimeSubsystem::dev* functions. Delete this file to remove them.
#include "TimeFramework/TimeSubsystem.h"

#if !UE_BUILD_SHIPPING

#include "TimeFramework/DA_TimeSettings.h"
#include "TimeFramework/TimeMathLibrary.h"
#include "TimeFramework/02Tags/T_TimeFramework.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"

bool UTimeSubsystem::devSetTimeOfDay(int32 hour, int32 minute){
    if(!isAuthority() || !clockReady || !settings){
        return false;
    }
    if(hour < 0 || hour > 23 || minute < 0 || minute > 59){
        UE_LOG(LogAwakenTime, Warning, TEXT("[%s] Dev time move refused: %d:%d is not a time of day"), *roleName(), hour, minute);
        return false;
    }
    const int64 now = extrapolatedGameTime();
    const FS_CalendarDate date = UTimeMathLibrary::deriveCalendar(now, makeCalendarConfig());
    const int64 targetSeconds = static_cast<int64>(hour) * UTimeMathLibrary::secondsPerHour + static_cast<int64>(minute) * 60;
    int64 delta = UTimeMathLibrary::floorMod(targetSeconds - date.timeOfDaySeconds, UTimeMathLibrary::secondsPerDay);
    if(delta == 0){
        delta = UTimeMathLibrary::secondsPerDay;   // time only moves forward: the same time tomorrow
    }
    return devSkipTo(now + delta);
}

bool UTimeSubsystem::devSkipToMorning(){
    if(!isAuthority() || !clockReady || !settings){
        return false;
    }
    const int64 now = extrapolatedGameTime();
    return devSkipTo(UTimeMathLibrary::nextWakeGameTime(now, makeCalendarConfig()));
}

bool UTimeSubsystem::devSetDay(int64 day){
    if(!isAuthority() || !clockReady || !settings){
        return false;
    }
    const int64 now = extrapolatedGameTime();
    const int64 target = UTimeMathLibrary::dayStartGameTime(day, makeCalendarConfig());
    if(target <= now){
        UE_LOG(LogAwakenTime, Warning, TEXT("[%s] Dev time move refused: day %lld is not later than today"), *roleName(), day);
        return false;
    }
    return devSkipTo(target);
}

bool UTimeSubsystem::devSetSeason(E_Season season, bool snowy){
    if(!isAuthority() || !clockReady || !settings){
        return false;
    }
    if(snowy && season != E_Season::Winter){
        UE_LOG(LogAwakenTime, Warning, TEXT("[%s] Dev time move refused: the snowy switch only works with Winter"), *roleName());
        return false;
    }
    const int64 now = extrapolatedGameTime();
    const FS_TimeCalendarConfig config = makeCalendarConfig();
    const FS_CalendarDate date = UTimeMathLibrary::deriveCalendar(now, config);
    const int64 daysPerSeason = settings->daysPerSeason;
    const int64 cycleLength = daysPerSeason * UTimeMathLibrary::seasonSlots;
    // Days from today to day 0 of the wanted season. Naming the current season skips a whole year, so it is never earlier.
    int64 daysAhead = (static_cast<int64>(season) - static_cast<int64>(date.season)) * daysPerSeason - date.dayInSeason;
    if(daysAhead <= 0){
        daysAhead += cycleLength;
    }
    const int64 seasonFirstDay = date.daysPassed + daysAhead;
    if(!devSkipTo(UTimeMathLibrary::dayStartGameTime(seasonFirstDay, config))){
        return false;
    }
    if(!snowy){
        return true;
    }
    // The Snowy draw is made when Winter begins, so it can only be read now.
    if(!snowyDraw.isValid){
        UE_LOG(LogAwakenTime, Warning, TEXT("[%s] Dev time move: this Winter has no Snowy block"), *roleName());
        return false;
    }
    if(snowyDraw.startDay == 0){
        return true;   // the block starts on the first day: already there
    }
    return devSkipTo(UTimeMathLibrary::dayStartGameTime(seasonFirstDay + snowyDraw.startDay, config));
}

bool UTimeSubsystem::devSimulateRegister(int32 playerIndex){
    if(!isAuthority()){
        return false;
    }
    const AGameStateBase* gameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    if(!gameState || !gameState->PlayerArray.IsValidIndex(playerIndex)){
        UE_LOG(LogAwakenTime, Warning, TEXT("[%s] SimulateRegister: no player at index %d"), *roleName(), playerIndex);
        return false;
    }
    APlayerState* player = gameState->PlayerArray[playerIndex];
    const APawn* pawn = player ? player->GetPawn() : nullptr;
    const FVector where = pawn ? pawn->GetActorLocation() : FVector::ZeroVector;   // as if the bed were at the player's feet
    const FGameplayTag result = requestSleepTimeSkip(player, where);
    UE_LOG(LogAwakenTime, Log, TEXT("[%s] SimulateRegister %d: %s"), *roleName(), playerIndex, result.IsValid() ? *result.ToString() : TEXT("accepted"));
    return !result.IsValid();
}

namespace{
    UTimeSubsystem* timeOf(UWorld* world){
        return world ? world->GetSubsystem<UTimeSubsystem>() : nullptr;
    }

    void report(const TCHAR* command, bool done){
        UE_LOG(LogAwakenTime, Log, TEXT("%s %s"), command, done ? TEXT("done") : TEXT("refused or not on the host (see the lines above)"));
    }

    bool parseSeason(const FString& name, E_Season& season){
        if(name.Equals(TEXT("spring"), ESearchCase::IgnoreCase)){
            season = E_Season::Spring;
        }else if(name.Equals(TEXT("summer"), ESearchCase::IgnoreCase)){
            season = E_Season::Summer;
        }else if(name.Equals(TEXT("fall"), ESearchCase::IgnoreCase)){
            season = E_Season::Fall;
        }else if(name.Equals(TEXT("winter"), ESearchCase::IgnoreCase)){
            season = E_Season::Winter;
        }else{
            return false;
        }
        return true;
    }

    FAutoConsoleCommandWithWorldAndArgs cmdSet(TEXT("Awaken.Time.Set"), TEXT("Awaken.Time.Set HH:MM. Skips to the next time of day that matches."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            UTimeSubsystem* time = timeOf(world);
            FString hourText;
            FString minuteText;
            if(!time || args.Num() < 1 || !args[0].Split(TEXT(":"), &hourText, &minuteText)){
                UE_LOG(LogAwakenTime, Warning, TEXT("Usage: Awaken.Time.Set HH:MM"));
                return;
            }
            report(TEXT("Awaken.Time.Set"), time->devSetTimeOfDay(FCString::Atoi(*hourText), FCString::Atoi(*minuteText)));
        }));

    FAutoConsoleCommandWithWorldAndArgs cmdSkipToMorning(TEXT("Awaken.Time.SkipToMorning"), TEXT("Skips to the next 05:00 through the same protocol as a sleep, with no sleep and no fade."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            if(UTimeSubsystem* time = timeOf(world)){
                report(TEXT("Awaken.Time.SkipToMorning"), time->devSkipToMorning());
            }
        }));

    FAutoConsoleCommandWithWorldAndArgs cmdSetDay(TEXT("Awaken.Time.SetDay"), TEXT("Awaken.Time.SetDay N. Skips to the 05:00 that starts day N. Refused if N is not later than today."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            UTimeSubsystem* time = timeOf(world);
            if(!time || args.Num() < 1){
                UE_LOG(LogAwakenTime, Warning, TEXT("Usage: Awaken.Time.SetDay N"));
                return;
            }
            report(TEXT("Awaken.Time.SetDay"), time->devSetDay(FCString::Atoi64(*args[0])));
        }));

    FAutoConsoleCommandWithWorldAndArgs cmdSetSeason(TEXT("Awaken.Time.SetSeason"), TEXT("Awaken.Time.SetSeason spring|summer|fall|winter [snowy]. Skips to the first day of the next such season."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            UTimeSubsystem* time = timeOf(world);
            E_Season season = E_Season::Spring;
            if(!time || args.Num() < 1 || !parseSeason(args[0], season)){
                UE_LOG(LogAwakenTime, Warning, TEXT("Usage: Awaken.Time.SetSeason spring|summer|fall|winter [snowy]"));
                return;
            }
            const bool snowy = args.Num() > 1 && args[1].Equals(TEXT("snowy"), ESearchCase::IgnoreCase);
            report(TEXT("Awaken.Time.SetSeason"), time->devSetSeason(season, snowy));
        }));

    FAutoConsoleCommandWithWorldAndArgs cmdWinterNight(TEXT("Awaken.Time.WinterNight"), TEXT("SetSeason winter snowy, then Set 21:00. Create the world first with Awaken.Time.StartSeasonOffset 95."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            if(UTimeSubsystem* time = timeOf(world)){
                const bool done = time->devSetSeason(E_Season::Winter, true) && time->devSetTimeOfDay(21, 0);
                report(TEXT("Awaken.Time.WinterNight"), done);
            }
        }));

    FAutoConsoleCommandWithWorldAndArgs cmdSimulateRegister(TEXT("Awaken.Time.SimulateRegister"), TEXT("Awaken.Time.SimulateRegister playerIdx. Registers the Nth player as if they held the sleep interaction at their own position."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            UTimeSubsystem* time = timeOf(world);
            if(!time || args.Num() < 1){
                UE_LOG(LogAwakenTime, Warning, TEXT("Usage: Awaken.Time.SimulateRegister playerIdx"));
                return;
            }
            report(TEXT("Awaken.Time.SimulateRegister"), time->devSimulateRegister(FCString::Atoi(*args[0])));
        }));
}

#endif
