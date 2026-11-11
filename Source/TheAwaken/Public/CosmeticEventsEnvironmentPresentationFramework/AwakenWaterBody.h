#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "CosmeticEventsEnvironmentPresentationFramework/00Enums/E_CosmeticEventsEnvironmentPresentationFramework.h"
#include "AwakenWaterBody.generated.h"

class ACharacter;
class UBoxComponent;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;
class UStaticMeshComponent;
class UWaterRegistrySubsystem;
class UDA_EnvironmentSettings;

// Part C of B21: every river, stream, lake and sea is one AAwakenWaterBody. It replaces AWeatherWaterBody of B14.
// A placed actor in a streamed cell, loaded the same way on every machine, not replicated. It holds no state that is not also in B14's record
// (S_WaterBodyState): the ice level and the river strength live in Weather's carrier, and the body reads them by bodyId.
//
// Unlike Parts A and B this part affects gameplay (the current, the ice collision, the water messages), so every machine computes it from
// replicated state alone.
//
// Swim volumes and ice surfaces: the body has one box of each. An irregular lake adds more components (in a Blueprint child) and tags them
// "SwimVolume" or "IceCollision". Every component with the tag is used.
UCLASS()
class THEAWAKEN_API AAwakenWaterBody : public AActor{
    GENERATED_BODY()

public:
    AAwakenWaterBody();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type reason) override;
    virtual void PostActorCreated() override;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& context) const override;
#endif

    // Stable id, kind 1: designer-placed (Doc02 19.3). B14 saves one record per id. The editor assigns it, and keeps it when an old
    // AWeatherWaterBody is replaced. The data check fails the build on an empty or duplicate id.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water")
    FGuid bodyId;

    // Decides what rain and cold do to the body and how it washes blood (B03).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water")
    E_WaterKind kind = E_WaterKind::Lake;

    // The current of a river or stream. Direction only, the length is ignored. Zero for lakes and the sea.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water")
    FVector flowDirection = FVector::ForwardVector;

    // The per-body factor of the current (default 1). The data scale of B14 3.3 is separate (flowSpeedPerStrength in the settings).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(ClampMin="0.0"))
    float baseFlowSpeed = 1.0f;

    // Rivers, streams and lakes freeze, the Sea never does (decided). Untick it on a river, stream or lake that must stay open.
    // On the Sea this flag is ignored: freezes() is always false there.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water")
    bool canFreeze = true;

    // The water level. On by default: the top of the swim volume. Used for splashes, the swim test and the ice surface.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water")
    bool useVolumeTopAsSurface = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(EditCondition="!useVolumeTopAsSurface"))
    float surfaceHeight = 0.0f;

    // ---- Queries ----

    // True when the point is inside any swim volume of this body. No allocation.
    bool containsPoint(const FVector& point) const;

    bool hasCurrent() const{
        return kind == E_WaterKind::River || kind == E_WaterKind::Stream;
    }

    // The one answer to "does this body ever freeze": the flag, and never the Sea. Registration, the ice timer, the ice collision,
    // the freezing-water test and the water messages all read this.
    bool freezes() const{
        return canFreeze && kind != E_WaterKind::Sea;
    }

    float getSurfaceHeight() const;

    // True while the ice collision is on.
    bool isIceSolidNow() const{
        return isIceSolid;
    }

    // Follows Weather's iceSolid flag. Called on Weather.WaterFroze and Weather.WaterThawed, by the reconcile timer, and in BeginPlay.
    void syncIce();

#if !UE_BUILD_SHIPPING
    void drawDebug(float lifetime) const;
#endif

private:
    UFUNCTION()
    void onSwimVolumeBeginOverlap(UPrimitiveComponent* component, AActor* other, UPrimitiveComponent* otherComponent, int32 bodyIndex, bool fromSweep, const FHitResult& hit);

    UFUNCTION()
    void onSwimVolumeEndOverlap(UPrimitiveComponent* component, AActor* other, UPrimitiveComponent* otherComponent, int32 bodyIndex);

    void collectVolumes();
    void registerWithWeather();
    void bindOverlaps();

    // Enter and leave (C.4). Only the host runs these.
    void noteActorEntered(AActor* actor);
    void noteActorLeft(AActor* actor);
    void processPendingExits();
    void raiseExited(const TWeakObjectPtr<AActor>& actor);
    bool isTrackedActor(const AActor* actor) const;
    bool isFreezingNow() const;
    void raiseSplash(AActor* actor);

    // Ice (C.3)
    void setIceSolid(bool solid);
    void setIceMaterialParameter(float level);
    void onIceTimer();
    void liftSwimmersOntoIce();
    bool computeIceLiftTarget(ACharacter* character, FVector& outTarget) const;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> root;

    // The water itself. A simple-collision component, no prompt, no gate.
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UBoxComponent> swimVolume;

    // A surface at surfaceHeight. Collision is OFF by default and switched by setIceSolid.
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UBoxComponent> iceCollision;

    // The ice look. A dynamic material parameter iceLevel, 0 to 1. Optional: no mesh set means no ice look.
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> iceMesh;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> iceMaterial;

    UPROPERTY()
    TObjectPtr<UWaterRegistrySubsystem> registry;

    UPROPERTY()
    TObjectPtr<UDA_EnvironmentSettings> settings;

    TArray<TWeakObjectPtr<UPrimitiveComponent>> swimVolumes;
    TArray<TWeakObjectPtr<UPrimitiveComponent>> iceCollisions;
    FBox worldBounds = FBox(ForceInit);

    // Host only. The number of swim volumes of this body that each actor overlaps. Water.Entered goes out on 0 to 1, Water.Exited on 1 to 0.
    TMap<TWeakObjectPtr<AActor>, int32> overlapCounts;

    // Host only. An actor that left is announced only after the re-entry delay, so a player who flickers across the edge raises nothing.
    TMap<TWeakObjectPtr<AActor>, double> pendingExits;
    FTimerHandle exitTimer;

    FTimerHandle iceTimer;
    bool isIceSolid = false;
    float lastIceLevel = -1.0f;
};
