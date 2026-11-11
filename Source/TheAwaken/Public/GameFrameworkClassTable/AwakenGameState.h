// Public/GameFrameworkClassTable/AwakenGameState.h
#pragma once
#include "GameFramework/GameStateBase.h"
#include "AwakenGameState.generated.h"

class UTimeCarrierComponent;
class UWeatherCarrierComponent;
class UDifficultyCarrierComponent;
class UBuildingRegistryCarrierComponent;
class UProgressionComponent;
class UCutTreesCarrierComponent;
class UTimeSaveParticipant;
class UWeatherSaveParticipant;
class UProgressionSaveParticipant;

UCLASS()
class THEAWAKEN_API AAwakenGameState : public AGameStateBase{
    GENERATED_BODY()
public:
    AAwakenGameState();

    UTimeCarrierComponent* getTime() const{
        return timeCarrier;
    }

    UWeatherCarrierComponent* getWeather() const{
        return weatherCarrier;
    }

    UDifficultyCarrierComponent* getDifficulty() const{
        return difficultyCarrier;
    }

    UBuildingRegistryCarrierComponent* getBuildingRegistry() const{
        return buildingRegistryCarrier;
    }

    UProgressionComponent* getProgression() const{
        return progression;
    }

    UCutTreesCarrierComponent* getCutTrees() const{
        return cutTreesCarrier;
    }

    UTimeSaveParticipant* getTimeSave() const{
        return timeSave;
    }

    UWeatherSaveParticipant* getWeatherSave() const{
        return weatherSave;
    }

    UProgressionSaveParticipant* getProgressionSave() const{
        return progressionSave;
    }

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UTimeCarrierComponent> timeCarrier;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UWeatherCarrierComponent> weatherCarrier;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UDifficultyCarrierComponent> difficultyCarrier;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UBuildingRegistryCarrierComponent> buildingRegistryCarrier;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UProgressionComponent> progression;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UCutTreesCarrierComponent> cutTreesCarrier;

    // World-scope save participant of Doc R12 (Time guide step 13). It exists on every machine and registers with Save on the host only.
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UTimeSaveParticipant> timeSave;

    // World-scope save participant of Doc B14 (Weather guide, Step 16). It exists on every machine and registers with Save on the host only.
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UWeatherSaveParticipant> weatherSave;

    // World-scope save participant of Doc R06 (Progression guide, Step 9). It exists on every machine and registers with Save on the host only.
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UProgressionSaveParticipant> progressionSave;
};
