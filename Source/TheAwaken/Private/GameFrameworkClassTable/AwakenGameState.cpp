// Private/GameFrameworkClassTable/AwakenGameState.cpp
#include "GameFrameworkClassTable/AwakenGameState.h"
#include "GameFrameworkClassTable/TimeCarrierComponent.h"
#include "GameFrameworkClassTable/WeatherCarrierComponent.h"
#include "GameFrameworkClassTable/DifficultyCarrierComponent.h"
#include "GameFrameworkClassTable/BuildingRegistryCarrierComponent.h"
#include "GameFrameworkClassTable/CutTreesCarrierComponent.h"
#include "ProgressionFramework/ProgressionComponent.h"
#include "TimeFramework/TimeSaveParticipant.h"
#include "WeatherFramework/WeatherSaveParticipant.h"
#include "ProgressionFramework/ProgressionSaveParticipant.h"

AAwakenGameState::AAwakenGameState(){
    timeCarrier = CreateDefaultSubobject<UTimeCarrierComponent>(TEXT("TimeCarrier"));
    weatherCarrier = CreateDefaultSubobject<UWeatherCarrierComponent>(TEXT("WeatherCarrier"));
    difficultyCarrier = CreateDefaultSubobject<UDifficultyCarrierComponent>(TEXT("DifficultyCarrier"));
    buildingRegistryCarrier = CreateDefaultSubobject<UBuildingRegistryCarrierComponent>(TEXT("BuildingRegistryCarrier"));
    progression = CreateDefaultSubobject<UProgressionComponent>(TEXT("Progression"));
    cutTreesCarrier = CreateDefaultSubobject<UCutTreesCarrierComponent>(TEXT("CutTreesCarrier"));
    timeSave = CreateDefaultSubobject<UTimeSaveParticipant>(TEXT("TimeSave"));
    weatherSave = CreateDefaultSubobject<UWeatherSaveParticipant>(TEXT("WeatherSave"));
    progressionSave = CreateDefaultSubobject<UProgressionSaveParticipant>(TEXT("ProgressionSave"));
}
