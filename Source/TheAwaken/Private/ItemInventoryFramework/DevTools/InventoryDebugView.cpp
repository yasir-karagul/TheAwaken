#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UContainerViewComponent.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"

namespace InventoryDebug{
    TAutoConsoleVariable<int32> cvarDebugInventory(TEXT("Awaken.Debug.Inventory"), 0, TEXT("1 = draw each player's bag, staged quantities, hotbar, hold slots, hand and open container as THIS machine knows them."), ECVF_Default);
    FTSTicker::FDelegateHandle tickerHandle;

    bool draw(float deltaTime){
        if(GEngine == nullptr){
            return true;
        }
        int32 messageKey = 9100;
        for(const FWorldContext& context : GEngine->GetWorldContexts()){
            const UWorld* world = context.World();
            if(world == nullptr || world->GetGameState() == nullptr){
                continue;
            }
            for(const APlayerState* player : world->GetGameState()->PlayerArray){
                const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
                if(inventory == nullptr){
                    continue;
                }
                const FS_HeldItem held = UBFL_Inventory::getHeldState(player);
                const UContainerViewComponent* view = UContainerViewComponent::findFor(player);
                GEngine->AddOnScreenDebugMessage(messageKey++, 0.3f, FColor::Cyan, FString::Printf(TEXT("[Inventory] %s: %s, hand %s%s, open container %s"), *player->GetPlayerName(), inventory->hasBag() ? TEXT("bag") : TEXT("BAGLESS"), held.isEmpty() ? TEXT("empty") : *held.itemId.ToString(), held.isWielded ? TEXT(" (wielded)") : TEXT(""), (view != nullptr && view->getViewedContainerId().IsValid()) ? *view->getViewedContainerId().ToString().Left(8) : TEXT("none")));
                for(const FS_InventoryEntry& entry : inventory->getBagEntries()){
                    GEngine->AddOnScreenDebugMessage(messageKey++, 0.3f, FColor::White, FString::Printf(TEXT("   %s  available %d  staged %d"), *entry.itemId.ToString(), entry.getAvailableQuantity(), entry.reservedQuantity));
                }
                for(const FS_HotbarBinding& binding : inventory->getHotbarBindings()){
                    if(!binding.boundItemId.IsNone()){
                        GEngine->AddOnScreenDebugMessage(messageKey++, 0.3f, FColor::Yellow, FString::Printf(TEXT("   key %d -> %s (%d)"), binding.slotIndex, *binding.boundItemId.ToString(), inventory->resolveHotbarSlot(binding.slotIndex).availableQuantity));
                    }
                }
                const TArray<FS_BaglessHoldSlot> holdSlots = inventory->getHoldSlots();
                for(int32 index = 0; index < holdSlots.Num(); index++){
                    if(holdSlots[index].isOccupied){
                        GEngine->AddOnScreenDebugMessage(messageKey++, 0.3f, FColor::Orange, FString::Printf(TEXT("   hold key %d: %s"), index + 1, *holdSlots[index].entry.itemId.ToString()));
                    }
                }
            }
        }
        return true;
    }

    void onChanged(IConsoleVariable* variable){
        if(variable->GetInt() != 0 && !tickerHandle.IsValid()){
            tickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&draw), 0.25f);   // only while the view is on (Doc02 §5)
        }else if(variable->GetInt() == 0 && tickerHandle.IsValid()){
            FTSTicker::GetCoreTicker().RemoveTicker(tickerHandle);
            tickerHandle.Reset();
        }
    }

    struct FRegistrar{
        FRegistrar(){
            cvarDebugInventory->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&onChanged));
        }
    };
    FRegistrar registrar;
}

#endif
