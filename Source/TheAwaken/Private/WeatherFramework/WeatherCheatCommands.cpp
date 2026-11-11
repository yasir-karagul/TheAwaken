// Non-shipping dev commands of Weather guide section 9.1. They register straight with the console,
// like TimeCheatCommands.cpp, and call the dev functions of UWeatherSubsystem.
#include "WeatherFramework/WeatherSubsystem.h"

#if !UE_BUILD_SHIPPING

#include "WeatherFramework/WeatherLog.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace{
    UWeatherSubsystem* weatherOf(UWorld* world){
        return world ? world->GetSubsystem<UWeatherSubsystem>() : nullptr;
    }

    void report(const TCHAR* name, bool done){
        UE_LOG(LogAwakenWeather, Log, TEXT("%s: %s"), name, done ? TEXT("done") : TEXT("refused, see the log"));
    }

    FAutoConsoleCommandWithWorldAndArgs cmdSet(TEXT("Awaken.Weather.Set"), TEXT("Awaken.Weather.Set <clear|cloudy|rain|storm|fog|snow|blizzard> <strength 0 to 100>. Starts a new weather period."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            UWeatherSubsystem* weather = weatherOf(world);
            if(!weather || args.Num() < 2){
                UE_LOG(LogAwakenWeather, Warning, TEXT("Usage: Awaken.Weather.Set <clear|cloudy|rain|storm|fog|snow|blizzard> <strength 0 to 100>"));
                return;
            }
            const int64 value = StaticEnum<E_WeatherType>()->GetValueByNameString(args[0]);
            if(value == INDEX_NONE){
                UE_LOG(LogAwakenWeather, Warning, TEXT("Unknown weather type %s"), *args[0]);
                return;
            }
            report(TEXT("Awaken.Weather.Set"), weather->devForceWeather(static_cast<E_WeatherType>(value), FCString::Atof(*args[1])));
        }));

    FAutoConsoleCommandWithWorldAndArgs cmdDump(TEXT("Awaken.Weather.Dump"), TEXT("Awaken.Weather.Dump. Logs the weather, the snow level and every water body."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            if(UWeatherSubsystem* weather = weatherOf(world)){
                weather->devDump();
            }
        }));
}

#endif
