// Doc B22 §3.5: the debug view Awaken.Debug.Carry. Draws locally, non-shipping only, never replicates.
// Doc B09 adds the rope / slider line part to the same view in its own guide.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "CarryEncumbranceFramework/UCarryComponent.h"

// The file-named outer namespace keeps these helpers apart from other DevTools files in a unity build.
namespace CarryDebugView{
namespace{
    TAutoConsoleVariable<int32> cvarDebugCarry(TEXT("Awaken.Debug.Carry"), 0, TEXT("1 = draw each player's carry slots, the active slot, the slowdown and the last interrupt cause on screen."), ECVF_Default);
    FTSTicker::FDelegateHandle carryTickerHandle;

    bool drawCarryDebug(float deltaTime){
        if(GEngine == nullptr){
            return true;
        }
        int32 messageKey = 9400;
        for(const FWorldContext& context : GEngine->GetWorldContexts()){
            const UWorld* world = context.World();
            if(world == nullptr || world->GetGameState() == nullptr){
                continue;
            }
            for(const APlayerState* player : world->GetGameState()->PlayerArray){
                const UCarryComponent* carry = UCarryComponent::findFor(player);
                if(carry == nullptr){
                    continue;
                }
                const FGameplayTag cause = carry->getLastInterruptCause();
                GEngine->AddOnScreenDebugMessage(messageKey++, 0.3f, carry->isCarryActive() ? FColor::Orange : FColor::Green, FString::Printf(TEXT("[Carry] %s: %s, %d/%d free, speed x%.2f, last interrupt %s"), *player->GetPlayerName(), carry->isCarryActive() ? TEXT("CARRYING") : TEXT("hands empty"), carry->getFreeSlotCount(), carry->getSlotCount(), carry->getSpeedMultiplier(), cause.IsValid() ? *cause.ToString() : TEXT("none")));
                const int32 activeIndex = carry->getActiveSlotIndex();
                for(int32 index = 0; index < carry->getCarrySlots().Num(); index++){
                    const FS_CarrySlotState& slot = carry->getCarrySlots()[index];
                    GEngine->AddOnScreenDebugMessage(messageKey++, 0.3f, FColor::White, FString::Printf(TEXT("   [%d] %s%s%s"), index, slot.isOccupied() ? *slot.itemId.ToString() : TEXT("-"), slot.holdsActor ? TEXT(" (actor)") : TEXT(""), index == activeIndex ? TEXT("  <- active") : TEXT("")));
                }
            }
        }
        return true;
    }

    void onCarryDebugChanged(IConsoleVariable* variable){
        if(variable->GetInt() != 0 && !carryTickerHandle.IsValid()){
            carryTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&drawCarryDebug), 0.25f);   // only while the view is on (Doc02 §5)
        }else if(variable->GetInt() == 0 && carryTickerHandle.IsValid()){
            FTSTicker::GetCoreTicker().RemoveTicker(carryTickerHandle);
            carryTickerHandle.Reset();
        }
    }

    struct FCarryDebugRegistrar{
        FCarryDebugRegistrar(){
            cvarDebugCarry->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&onCarryDebugChanged));
        }
    };
    FCarryDebugRegistrar carryDebugRegistrar;
}
}

#endif
