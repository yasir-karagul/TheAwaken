// Doc R02 guide CR9.3: debug view Awaken.Debug.Crafting (Doc B22 3.5). Draws locally, never replicates, non-shipping only.
// On a client it shows only the local player's session, because nobody else's replicates.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "CraftingFramework/UCraftingComponent.h"

namespace CraftingDebugView{
namespace{
    TAutoConsoleVariable<int32> cvarDebugCrafting(TEXT("Awaken.Debug.Crafting"), 0, TEXT("1 = draw each visible crafting session: station, staged items, running craft and the craft buttons with their states."), ECVF_Default);
    FTSTicker::FDelegateHandle craftingTickerHandle;

    bool drawCraftingDebug(float deltaTime){
        if(GEngine == nullptr){
            return true;
        }
        int32 messageKey = 9500;
        for(const FWorldContext& context : GEngine->GetWorldContexts()){
            const UWorld* world = context.World();
            if(world == nullptr || world->GetGameState() == nullptr){
                continue;
            }
            for(const APlayerState* player : world->GetGameState()->PlayerArray){
                const UCraftingComponent* crafting = UCraftingComponent::findFor(player);
                if(crafting == nullptr){
                    continue;
                }
                const FS_CraftingSessionState state = crafting->getSessionState();
                if(!state.isOpen && !state.hasStation){
                    continue;
                }
                GEngine->AddOnScreenDebugMessage(messageKey++, 0.3f, FColor::Cyan, FString::Printf(TEXT("[Crafting] %s: session %s, station %s, crafting %s"), *player->GetPlayerName(), state.isOpen ? TEXT("OPEN") : TEXT("not yet"), state.hasStation ? *state.stationTags.ToStringSimple() : TEXT("none"), state.activeRecipeId.IsNone() ? TEXT("nothing") : *state.activeRecipeId.ToString()));
                for(const TPair<FName, int32>& staged : crafting->getStagedQuantities()){
                    GEngine->AddOnScreenDebugMessage(messageKey++, 0.3f, FColor::White, FString::Printf(TEXT("   staged %s x%d"), *staged.Key.ToString(), staged.Value));
                }
                for(const FS_AvailableRecipeOption& option : crafting->getAvailableRecipes()){
                    const FColor colour = option.state == E_RecipeListState::Craftable ? FColor::Green : (option.state == E_RecipeListState::ExtraItems ? FColor::White : FColor::Red);
                    GEngine->AddOnScreenDebugMessage(messageKey++, 0.3f, colour, FString::Printf(TEXT("   [%s] missing %d, extra %d%s"), *option.recipeId.ToString(), option.getMissingCount(), option.extraItems.Num(), option.isStationMissing ? TEXT(", needs station") : TEXT("")));
                }
            }
        }
        return true;
    }

    void onCraftingDebugChanged(IConsoleVariable* variable){
        if(variable->GetInt() != 0 && !craftingTickerHandle.IsValid()){
            craftingTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&drawCraftingDebug), 0.25f);   // only while the view is on (Doc02 §5)
        }else if(variable->GetInt() == 0 && craftingTickerHandle.IsValid()){
            FTSTicker::GetCoreTicker().RemoveTicker(craftingTickerHandle);
            craftingTickerHandle.Reset();
        }
    }

    struct FCraftingDebugRegistrar{
        FCraftingDebugRegistrar(){
            cvarDebugCrafting->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&onCraftingDebugChanged));
        }
    };
    FCraftingDebugRegistrar craftingDebugRegistrar;
}
}

#endif
