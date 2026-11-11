#include "CosmeticEventsEnvironmentPresentationFramework/EnvironmentSources.h"
#include "GameFrameworkClassTable/AwakenGameState.h"
#include "GameFrameworkClassTable/TimeCarrierComponent.h"
#include "GameFrameworkClassTable/WeatherCarrierComponent.h"
#include "TimeFramework/TimeSubsystem.h"
#include "WeatherFramework/WeatherSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace{
    const UWorld* worldOf(const UObject* worldContext){
        return GEngine ? GEngine->GetWorldFromContextObject(worldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
    }
}

namespace EnvironmentSources{

    FS_EnvironmentTimeInput readTime(const UObject* worldContext){
        FS_EnvironmentTimeInput input;
        const UWorld* world = worldOf(worldContext);
        const UTimeSubsystem* timeSubsystem = UTimeSubsystem::get(worldContext);
        if(world == nullptr || timeSubsystem == nullptr){
            return input;
        }
        const AAwakenGameState* gameState = world->GetGameState<AAwakenGameState>();
        const UTimeCarrierComponent* carrier = gameState ? gameState->getTime() : nullptr;

        // isClockReady is set on the host only (initializeNewWorld or restoreFromSnapshot). A client is ready once the carrier exists.
        input.isReady = world->GetNetMode() == NM_Client ? carrier != nullptr : timeSubsystem->isClockReady();
        if(!input.isReady){
            return input;
        }
        input.timeOfDayHours = timeSubsystem->getCurrentTimeOfDay();
        input.phase = timeSubsystem->getCurrentPhase();
        input.season = timeSubsystem->getCurrentSeason();
        input.isSnowyBlock = timeSubsystem->isSnowyBlockActive();
        input.isGroupSleepSkipRunning = timeSubsystem->isGroupSleepSkipRunning();
        if(carrier != nullptr){
            input.hasSkipSerial = true;
            input.skipSerial = carrier->syncState.skipSerial;
        }
        return input;
    }

    FS_EnvironmentWeatherInput readWeather(const UObject* worldContext){
        FS_EnvironmentWeatherInput input;
        const UWorld* world = worldOf(worldContext);
        const UWeatherSubsystem* weatherSubsystem = UWeatherSubsystem::get(worldContext);
        if(world == nullptr || weatherSubsystem == nullptr){
            return input;
        }
        const AAwakenGameState* gameState = world->GetGameState<AAwakenGameState>();
        const UWeatherCarrierComponent* carrier = gameState ? gameState->getWeather() : nullptr;

        // isWeatherReady is set on the host only. A client is ready once the carrier exists.
        input.isReady = world->GetNetMode() == NM_Client ? carrier != nullptr : weatherSubsystem->isWeatherReady();
        if(!input.isReady){
            return input;
        }
        input.type = weatherSubsystem->getCurrentWeather();
        input.rainStrength = weatherSubsystem->getRainStrength();
        input.snowStrength = weatherSubsystem->getSnowStrength();
        input.snowLevel = weatherSubsystem->getSnowLevel();
        return input;
    }
}
