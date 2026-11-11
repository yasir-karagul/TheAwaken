#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "TimerManager.h"
#include "CosmeticEventsEnvironmentPresentationFramework/01Structs/S_CosmeticEventsEnvironmentPresentationFramework.h"
#include "EnvironmentZoneVolume.generated.h"

class AEnvironmentManager;

// A designer-placed volume of any shape that gives a cave or dungeon its look (B21 EF1, gap G3: decided Nov 6, B21 owns it, PROPOSED until EF1
// is amended). It calls pushZone when the local player enters and popZone when the player leaves.
//
// It is not replicated and not saved. It destroys itself on a dedicated server, and on every other machine it reacts only to that machine's own
// locally controlled pawn. It does not feed the shelter value or the cold exposure: a cave counts as indoors through its flagged sublevel (B14 3.7).
UCLASS()
class THEAWAKEN_API AEnvironmentZoneVolume : public AVolume{
    GENERATED_BODY()

public:
    AEnvironmentZoneVolume();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type reason) override;
    virtual void PostActorCreated() override;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& context) const override;
#endif

    // Stable id, kind 1: designer-placed (Doc02 19.3). The editor assigns it. The data check fails the build on an empty or duplicate id.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment")
    FGuid zoneId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment")
    FS_EnvironmentZoneSettings settings;

private:
    UFUNCTION()
    void onBeginOverlap(AActor* overlappedActor, AActor* otherActor);

    UFUNCTION()
    void onEndOverlap(AActor* overlappedActor, AActor* otherActor);

    UFUNCTION()
    void onPossessedPawnChanged(APawn* oldPawn, APawn* newPawn);

    void tryInitialCheck();
    bool isLocalPlayerPawn(const AActor* actor) const;
    AEnvironmentManager* findManager() const;
    void enterZone();
    void leaveZone();

    FTimerHandle initialCheckTimer;
    bool inside = false;
};
