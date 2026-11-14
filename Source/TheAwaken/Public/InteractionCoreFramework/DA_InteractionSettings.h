// Public/InteractionCoreFramework/DA_InteractionSettings.h
// All tunable numbers of Doc B18 live here and nowhere in code (Doc00 2.7). Loaded like UDA_LocalizationSettings: a static makeAssetId()
// and the Asset Manager. NOT in Doc B18's class table: approved by the designer on 2026-10-24, log it in the Index and amend B18.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "Misc/DataValidation.h"
#include "DA_InteractionSettings.generated.h"

class UInputAction;

UCLASS(BlueprintType)
class THEAWAKEN_API UDA_InteractionSettings : public UPrimaryDataAsset{
    GENERATED_BODY()

public:
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
    static FPrimaryAssetId makeAssetId();

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& context) const override;
#endif

    // B18 section 10: range and rate are data and are NOT chosen. The defaults below are obvious TEST values, not design values.
    // Name the asset instance "TEST ONLY" until a grey-box pass picks real numbers.

    // Candidate distance in focus and in the server check (centimetres).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Focus")
    float interactionRange = 300.0f;

    // Thickness of the focus sweep, so thin objects are easy to hit (centimetres).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Focus")
    float sweepRadius = 20.0f;

    // Seconds between focus updates. Focus is a timer, never a per-frame trace.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Focus")
    float focusInterval = 0.1f;

    // Collision channel the focus sweep and the line checks use (PROPOSED). Interactables must OVERLAP it and solids must BLOCK it.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Focus")
    TEnumAsByte<ECollisionChannel> interactionChannel = ECC_GameTraceChannel1;

    // Extra server range for movement between press and arrival (PROPOSED), centimetres.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Server")
    float rangeSlack = 50.0f;

    // Seconds between range checks. The watch runs only while a hold is running or a lock is held.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Server")
    float lockWatchInterval = 0.25f;

    // Interaction tags that still work while a carry slot is occupied (B18 3.5, section 10). Start with doors, switches, deposit,
    // place, rope send and confirm the list against Doc B07 3.6. Drop and throw are not interactions here.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Server")
    FGameplayTagContainer allowedWhileCarrying;

    // How long the refusal line stays up, seconds.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Presentation")
    float reasonLineSeconds = 2.0f;

    // IA_Interact: a Digital (bool) Enhanced Input action, mapped to E in the gameplay mapping context.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Input")
    TObjectPtr<UInputAction> interactAction;
};
