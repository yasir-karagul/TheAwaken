#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "EquipmentFramework/UEquipmentComponent.h"

namespace{
    TAutoConsoleVariable<int32> cvarDebugEquipment(TEXT("Awaken.Debug.Equipment"), 0, TEXT("1 = draw each player's armor stack, wear, aggregate, clothes and warmth on screen."), ECVF_Default);
    FTSTicker::FDelegateHandle equipmentTickerHandle;

    bool drawEquipmentDebug(float deltaTime){
        if(GEngine == nullptr){
            return true;
        }
        int32 messageKey = 9300;
        for(const FWorldContext& context : GEngine->GetWorldContexts()){
            const UWorld* world = context.World();
            if(world == nullptr || world->GetGameState() == nullptr){
                continue;
            }
            for(const APlayerState* player : world->GetGameState()->PlayerArray){
                const UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
                if(equipment == nullptr){
                    continue;
                }
                const FS_AggregatedArmorStats stats = equipment->getAggregatedArmorStats();
                const FS_WornClothesState clothes = equipment->getWornClothes();
                GEngine->AddOnScreenDebugMessage(messageKey++, 0.3f, FColor::Green, FString::Printf(TEXT("[Equipment] %s: %d/%d pieces, armor %.1f, clothes %s (%.1f), warmth %.1f"), *player->GetPlayerName(), stats.pieceCount, equipment->getArmorStackCap(), stats.armorValue, clothes.isDefault() ? TEXT("default") : *clothes.itemId.ToString(), equipment->getClothesProtection().protection, equipment->getTotalWarmth()));
                // Drawn top of the stack first: that is the order the pieces absorb a hit in.
                for(int32 index = equipment->getArmorStack().Num() - 1; index >= 0; index--){
                    const FS_EquippedArmorState& piece = equipment->getArmorStack()[index];
                    GEngine->AddOnScreenDebugMessage(messageKey++, 0.3f, FColor::White, FString::Printf(TEXT("   [%d] %s durability %.1f"), piece.stackIndex, *piece.itemId.ToString(), piece.remainingDurability));
                }
            }
        }
        return true;
    }

    void onEquipmentDebugChanged(IConsoleVariable* variable){
        if(variable->GetInt() != 0 && !equipmentTickerHandle.IsValid()){
            equipmentTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&drawEquipmentDebug), 0.25f);   // only while the view is on (Doc02 §5)
        }else if(variable->GetInt() == 0 && equipmentTickerHandle.IsValid()){
            FTSTicker::GetCoreTicker().RemoveTicker(equipmentTickerHandle);
            equipmentTickerHandle.Reset();
        }
    }

    struct FEquipmentDebugRegistrar{
        FEquipmentDebugRegistrar(){
            cvarDebugEquipment->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&onEquipmentDebugChanged));
        }
    };
    FEquipmentDebugRegistrar equipmentDebugRegistrar;
}

#endif
