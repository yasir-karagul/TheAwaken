// Public/InteractionCoreFramework/InteractionLockComponent.h
// Doc B18 3.7. Add one to every actor whose lock policy is not None. NOT in B18's class table: approved on 2026-10-24.
// The holder list lives on the host and is never replicated. isInUse is the ONLY replicated lock state, so a prompt can show it.
// The owning actor must itself replicate, or the flag never reaches clients.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractionCoreFramework/00Enums/E_InteractionCoreFramework.h"
#include "InteractionLockComponent.generated.h"

UCLASS(ClassGroup=(Awaken), meta=(BlueprintSpawnableComponent))
class THEAWAKEN_API UInteractionLockComponent : public UActorComponent{
    GENERATED_BODY()
public:
    UInteractionLockComponent();

    // The parameter must be called OutLifetimeProps: the DOREPLIFETIME macros use that exact name (Doc02 20.5).
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

    // ---- Server only ----
    bool canAcquire(AActor* player, E_InteractionLockPolicy policy) const;
    void acquire(AActor* player, E_InteractionLockPolicy policy);
    void release(AActor* player);
    bool isHeldBy(const AActor* player) const;

    // True for an Exclusive lock while someone holds it. Shared locks never set it.
    UPROPERTY(ReplicatedUsing = OnRep_isInUse, BlueprintReadOnly, Category = "Interaction")
    bool isInUse = false;

    UFUNCTION()
    void OnRep_isInUse();

    // PROPOSED. A Press object normally frees its lock right after interact. A station keeps it until its own framework calls
    // UInteractionComponent::releaseLock (for example when the bag screen closes): tick this on such objects. No idle release exists.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
    bool keepsLockUntilReleased = false;

private:
    // Server only, never replicated (B18 section 7).
    TArray<TWeakObjectPtr<AActor>> holders;
    bool holdsExclusive = false;

    void pruneHolders();
};