#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UCarrySettings.generated.h"

// Project Settings -> Game -> Awaken Carry. Every value is tuning data (Doc00 2.7).
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Awaken Carry"))
class THEAWAKEN_API UCarrySettings : public UDeveloperSettings{
    GENERATED_BODY()

public:
    UCarrySettings();

    static const UCarrySettings* get(){
        return GetDefault<UCarrySettings>();
    }

    // B07 3.1: "1-2, per the original design intent ... the number of slots stays tuning data".
    UPROPERTY(Config, EditAnywhere, Category = "Slots", meta = (ClampMin = "1")) int32 carrySlotCount = 2;
    // Doc R07 3.5 collects this with the other speed multipliers. PROPOSED value.
    UPROPERTY(Config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0.05", ClampMax = "1.0")) float carrySpeedMultiplier = 0.6f;

    // Where the item of each slot is drawn on the body. Entry 0 = slot 0. Visual only; Doc R07 owns the pose.
    UPROPERTY(Config, EditAnywhere, Category = "Visual") TArray<FName> carrySocketNames;
    // False when Doc R07 draws the carried item itself.
    UPROPERTY(Config, EditAnywhere, Category = "Visual") bool showCarriedMeshes = true;

    // B07 3.4. All PROPOSED values.
    UPROPERTY(Config, EditAnywhere, Category = "Throw", meta = (ClampMin = "0")) float throwSpeed = 900.0f;
    // How far in front of the eye a thrown item starts, so it does not start inside the thrower.
    UPROPERTY(Config, EditAnywhere, Category = "Throw", meta = (ClampMin = "0")) float throwStartDistance = 60.0f;
    UPROPERTY(Config, EditAnywhere, Category = "Throw", meta = (ClampMin = "1")) float throwCollisionRadius = 20.0f;
    // A throw that has not hit anything after this many seconds lands where it is. Nothing is ever lost in flight.
    UPROPERTY(Config, EditAnywhere, Category = "Throw", meta = (ClampMin = "0.5")) float maxThrowFlightSeconds = 8.0f;
    // Doc02 19.6: a requested throw direction further than this from the pawn's view is refused (C9).
    UPROPERTY(Config, EditAnywhere, Category = "Throw", meta = (ClampMin = "1", ClampMax = "180")) float maxThrowAngleDegrees = 60.0f;
};
