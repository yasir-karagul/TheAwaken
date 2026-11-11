#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DA_GroundTypeMap.generated.h"

class UPhysicalMaterial;
class UWorld;

// The read-only layer that says what kind of ground lies at a point (B21 section on the ground type map). The B17 map reads it.
// A coarse grid over the world and a palette of Environment.Ground.* tags. Local on every machine, never saved or replicated.
// Loaded as DA_GroundTypeMap_Default, Primary Asset Type "GroundTypeMap".
UCLASS(BlueprintType)
class THEAWAKEN_API UDA_GroundTypeMap : public UPrimaryDataAsset{
    GENERATED_BODY()

public:
    static constexpr uint8 noneIndex = 255;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
    static UDA_GroundTypeMap* loadDefault();

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& context) const override;

    // Bakes the grid from the world by tracing straight down at the centre of every cell and mapping the physical material that is hit
    // (a landscape layer reports the physical material of its layer) through physicalMaterialTags. The method is not specified by B21,
    // this is the cheapest route that stays local and read-only. Run it with the console command Awaken.Ground.Bake in the editor.
    void bakeFromWorld(UWorld* world);
#endif

    // The ground tag at a point. defaultGround when the point is outside the grid or the cell is unset.
    UFUNCTION(BlueprintPure, Category="Ground")
    FGameplayTag sampleAt(const FVector& location) const;

    // Cell size in centimetres. Data.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grid", meta=(ClampMin="100.0"))
    float cellSizeCm = 10000.0f;

    // World X and Y of the corner of cell (0, 0). The default centres a 4 x 4 km world (D1) on the origin.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grid")
    FVector2D originCm = FVector2D(-200000.0, -200000.0);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grid", meta=(ClampMin="1"))
    int32 gridWidth = 40;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grid", meta=(ClampMin="1"))
    int32 gridHeight = 40;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grid")
    FGameplayTag defaultGround;

    // The ground types the cells point into. The set is open, content may extend it (Doc02 3).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grid")
    TArray<FGameplayTag> palette;

    // gridWidth times gridHeight palette indexes, row by row. 255 means unset.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grid", AdvancedDisplay)
    TArray<uint8> cells;

    // Editor bake only: which physical material means which ground type.
    UPROPERTY(EditAnywhere, Category="Bake")
    TMap<TObjectPtr<UPhysicalMaterial>, FGameplayTag> physicalMaterialTags;

    UPROPERTY(EditAnywhere, Category="Bake", meta=(ClampMin="0.0"))
    float bakeTraceHeightCm = 100000.0f;
};
