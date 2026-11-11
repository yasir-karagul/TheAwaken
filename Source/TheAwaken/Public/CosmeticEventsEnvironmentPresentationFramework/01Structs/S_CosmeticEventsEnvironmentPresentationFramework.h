#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/Scene.h"
#include "GameplayTagContainer.h"
#include "Misc/Guid.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "CosmeticEventsEnvironmentPresentationFramework/00Enums/E_CosmeticEventsEnvironmentPresentationFramework.h"
#include "S_CosmeticEventsEnvironmentPresentationFramework.generated.h"

class AActor;

// ---------------- Part A: the cosmetic event channel ----------------

// The four fields of Doc02 19.2 and nothing more. A fifth field needs a change to Doc02 19.2 (gap G4).
USTRUCT(BlueprintType)
struct FS_CosmeticEvent{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTag eventTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector location = FVector::ZeroVector;

    // One stable id (Doc02 19.3). An empty GUID means none. For an NPC it is the NPCId itself, for a player a
    // deterministic GUID built from the PlayerIdentity string (makeInstigatorId, gap G11).
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid instigator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float magnitude = 0.0f;
};

// Row of DT_CosmeticEvents. The row name is the full tag string, or Default. The numbers are placeholders: numbers are data.
USTRUCT(BlueprintType)
struct FS_CosmeticEventRule : public FTableRowBase{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float relevanceRadius = 3000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 maxPerSecondPerArea = 8;

    // True for NPCVoice and PlayerBodySound: their radius is the one hearing distance owned by Doc R09.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool readsHearingDistance = false;
};

// A plain struct (not reflected) for the rate limiter. One entry per send, kept for the last second.
struct FS_RecentSend{
    double time = 0.0;
    FVector location = FVector::ZeroVector;
};

// ---------------- Part B: the environment manager ----------------

// One weather look. Every value of it is blended together so nothing pops (B.3).
USTRUCT(BlueprintType)
struct FS_WeatherLook{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0", ClampMax="1.0"))
    float cloudCoverage = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0", ClampMax="1.0"))
    float cloudDensity = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0"))
    float fogDensity = 0.005f;

    // Multiplies sun, moon and sky light.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0"))
    float lightScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor skyTint = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0", ClampMax="1.0"))
    float windStrength = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float windDirectionDegrees = 0.0f;

    // 0 to 1. The value wetness moves toward while this look is current (EF6).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0", ClampMax="1.0"))
    float wetnessTarget = 0.0f;
};

// What a cave or dungeon looks like (EF1). Field list PROPOSED, owned by B21 because B21 owns the zone volume (gap G3).
USTRUCT(BlueprintType)
struct FS_EnvironmentZoneSettings{
    GENERATED_BODY()

    // Only the fields whose bOverride flag is ticked are used.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FPostProcessSettings postProcess;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool overrideFog = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="overrideFog", ClampMin="0.0"))
    float fogDensity = 0.02f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="overrideFog", ClampMin="0.0"))
    float fogHeightFalloff = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="overrideFog"))
    FLinearColor fogInscatteringColor = FLinearColor(0.02f, 0.02f, 0.03f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="overrideFog", ClampMin="0.0"))
    float fogStartDistance = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="overrideFog", ClampMin="0.0", ClampMax="1.0"))
    float fogMaxOpacity = 1.0f;

    // Fades out the atmosphere, clouds, sky light, sun and moon.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool hideSky = true;

    // 0 means the default zone blend time in UDA_EnvironmentSettings.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0"))
    float blendSeconds = 0.0f;
};

// The console variables of one graphics preset (B17 3.13). Index 0 is Low.
USTRUCT(BlueprintType)
struct FS_EnvironmentQualityPreset{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FName, float> consoleVariables;
};

// ---------------- Part C: water messages (server only) and the lightning message (local) ----------------

USTRUCT(BlueprintType)
struct FS_WaterEnteredMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<AActor> actor;

    UPROPERTY(BlueprintReadOnly)
    E_WaterKind kind = E_WaterKind::Lake;

    UPROPERTY(BlueprintReadOnly)
    bool isFreezing = false;

    UPROPERTY(BlueprintReadOnly)
    bool isFrozenSurface = false;
};

USTRUCT(BlueprintType)
struct FS_WaterExitedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<AActor> actor;

    UPROPERTY(BlueprintReadOnly)
    E_WaterKind kind = E_WaterKind::Lake;
};

// Local on every machine. One distance per flash, so the bolt and the thunder agree (EF4).
USTRUCT(BlueprintType)
struct FS_LightningFlashMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FVector direction = FVector::ForwardVector;

    UPROPERTY(BlueprintReadOnly)
    float distance = 0.0f;
};
