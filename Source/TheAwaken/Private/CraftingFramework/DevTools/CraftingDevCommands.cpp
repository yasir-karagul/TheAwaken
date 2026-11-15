// Doc R02 guide CR9.1, CR9.2: the Awaken.Craft.* dev commands (Doc B22 3.2, 3.4). Host only, through the owner's public
// functions, each logs its result in LogAwakenCrafting, none saves, compiled out of Shipping. Optional last argument pN.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "CraftingFramework/CraftingLog.h"
#include "CraftingFramework/UBFL_CraftingValidation.h"
#include "CraftingFramework/UCraftingComponent.h"
#include "CraftingFramework/UCraftingStationComponent.h"
#include "CraftingFramework/03Interfaces/BPI_ProgressionQuery.h"
#include "CraftingFramework/Stubs/CraftingStubs.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"

// File-named namespace around the anonymous one: unity builds may merge dev-command files (maintenance 2026-11-14).
namespace CraftingDevCommands{
namespace{
    bool isPlayerArgument(const FString& argument){
        return argument.Len() > 1 && argument.StartsWith(TEXT("p"), ESearchCase::CaseSensitive) && argument.Mid(1).IsNumeric();
    }

    // Returns null with a log line when not on the host or when the player does not exist.
    APlayerState* resolvePlayer(UWorld* world, const TArray<FString>& arguments){
        if(world == nullptr || !UBFL_Inventory::isServerWorld(world)){
            CRAFTING_LOG(Warning, world, "Host command: refused on a client (the Doc B22 cheat manager forwards it)");
            return nullptr;
        }
        int32 playerIndex = 0;
        for(const FString& argument : arguments){
            if(isPlayerArgument(argument)){
                playerIndex = FCString::Atoi(*argument.Mid(1));
            }
        }
        const AGameStateBase* gameState = world->GetGameState();
        if(gameState == nullptr || !gameState->PlayerArray.IsValidIndex(playerIndex)){
            CRAFTING_LOG(Warning, world, "No player %d", playerIndex);
            return nullptr;
        }
        return gameState->PlayerArray[playerIndex];
    }

    // The arguments that are not the player selector, in order.
    TArray<FString> plainArguments(const TArray<FString>& arguments){
        TArray<FString> plain;
        for(const FString& argument : arguments){
            if(!isPlayerArgument(argument)){
                plain.Add(argument);
            }
        }
        return plain;
    }

    const TCHAR* stateName(E_RecipeListState state){
        return state == E_RecipeListState::Craftable ? TEXT("GREEN") : (state == E_RecipeListState::ExtraItems ? TEXT("WHITE") : TEXT("RED"));
    }

    void logList(UWorld* world, const UCraftingComponent* crafting){
        const TArray<FS_AvailableRecipeOption> list = crafting->getAvailableRecipes();
        CRAFTING_LOG(Log, world, "%d craft button(s):", list.Num());
        for(const FS_AvailableRecipeOption& option : list){
            FString detail;
            for(const FS_CraftingRecipeRequirement& missing : option.missingIngredients){
                detail += FString::Printf(TEXT(" missing %s x%d;"), *missing.itemId.ToString(), missing.quantity);
            }
            if(option.isStationMissing){
                detail += TEXT(" needs its station;");
            }
            for(const FName& extra : option.extraItems){
                detail += FString::Printf(TEXT(" extra %s;"), *extra.ToString());
            }
            CRAFTING_LOG(Log, world, "   %-5s %s (x%d)%s", stateName(option.state), *option.recipeId.ToString(), option.maxCraftableCount, *detail);
        }
    }

    void stageCommand(const TArray<FString>& arguments, UWorld* world){
        UCraftingComponent* crafting = UCraftingComponent::findFor(resolvePlayer(world, arguments));
        const TArray<FString> plain = plainArguments(arguments);
        if(crafting == nullptr || plain.Num() < 1){
            CRAFTING_LOG(Warning, world, "Usage: Awaken.Craft.Stage <itemId> [quantity] [pN]");
            return;
        }
        FS_ItemRef ref;
        ref.itemId = FName(*plain[0]);
        const int32 quantity = plain.Num() > 1 ? FMath::Max(1, FCString::Atoi(*plain[1])) : 1;
        const FS_InventoryResult result = crafting->stageIngredient(ref, quantity);
        CRAFTING_LOG(Log, world, "Stage %s x%d: %s", *plain[0], quantity, result.succeeded() ? TEXT("done") : *result.failReason.ToString());
        logList(world, crafting);
    }

    void unstageCommand(const TArray<FString>& arguments, UWorld* world){
        UCraftingComponent* crafting = UCraftingComponent::findFor(resolvePlayer(world, arguments));
        const TArray<FString> plain = plainArguments(arguments);
        if(crafting == nullptr || plain.Num() < 1){
            CRAFTING_LOG(Warning, world, "Usage: Awaken.Craft.Unstage <itemId> [quantity] [pN]");
            return;
        }
        FS_ItemRef ref;
        ref.itemId = FName(*plain[0]);
        const int32 quantity = plain.Num() > 1 ? FMath::Max(1, FCString::Atoi(*plain[1])) : 1;
        const FS_InventoryResult result = crafting->unstageIngredient(ref, quantity);
        CRAFTING_LOG(Log, world, "Unstage %s x%d: %s", *plain[0], quantity, result.succeeded() ? TEXT("done") : *result.failReason.ToString());
        logList(world, crafting);
    }

    // The situation most likely to break (Doc B22 §3.10): a full crafting area. Everything that can be staged is staged,
    // so the list holds every recipe the bag touches: the cap, the order and the white entries all show at once.
    void stageAllCommand(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = resolvePlayer(world, arguments);
        UCraftingComponent* crafting = UCraftingComponent::findFor(player);
        const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
        if(crafting == nullptr || inventory == nullptr){
            CRAFTING_LOG(Warning, world, "Usage: Awaken.Craft.StageAll [pN]");
            return;
        }
        int32 stagedTypes = 0;
        int32 refusedTypes = 0;
        for(const FS_InventoryEntry& entry : inventory->getBagEntries()){
            if(entry.getAvailableQuantity() < 1){
                continue;
            }
            FS_ItemRef ref;
            ref.itemId = entry.itemId;
            ref.instanceId = entry.instanceId;
            if(crafting->stageIngredient(ref, entry.getAvailableQuantity()).succeeded()){
                stagedTypes++;
            }else{
                refusedTypes++;   // the Axe, the map, the weapon in the hand, a container that holds something
            }
        }
        CRAFTING_LOG(Log, world, "StageAll: %d entries staged, %d refused", stagedTypes, refusedTypes);
        logList(world, crafting);
    }

    void craftCommand(const TArray<FString>& arguments, UWorld* world){
        UCraftingComponent* crafting = UCraftingComponent::findFor(resolvePlayer(world, arguments));
        const TArray<FString> plain = plainArguments(arguments);
        if(crafting == nullptr || plain.Num() < 1){
            CRAFTING_LOG(Warning, world, "Usage: Awaken.Craft.Craft <recipeId> [pN]");
            return;
        }
        const FS_InventoryResult result = crafting->requestCraft(FName(*plain[0]));
        CRAFTING_LOG(Log, world, "Craft %s: %s", *plain[0], result.succeeded() ? TEXT("started") : *result.failReason.ToString());
    }

    void closeCommand(const TArray<FString>& arguments, UWorld* world){
        UCraftingComponent* crafting = UCraftingComponent::findFor(resolvePlayer(world, arguments));
        if(crafting == nullptr){
            CRAFTING_LOG(Warning, world, "Usage: Awaken.Craft.Close [pN]");
            return;
        }
        crafting->closeCraftingSession(E_CraftingCloseReason::BagClosed);
        CRAFTING_LOG(Log, world, "Close: the session and the station were released");
    }

    // TEMPORARY — owner: Doc B01. Spawns a stub station in front of the player and enters it.
    void spawnStationCommand(const TArray<FString>& arguments, UWorld* world){
        const APlayerState* player = resolvePlayer(world, arguments);
        UCraftingComponent* crafting = UCraftingComponent::findFor(player);
        const APawn* pawn = player != nullptr ? player->GetPawn() : nullptr;
        const TArray<FString> plain = plainArguments(arguments);
        const FGameplayTag stationTag = plain.Num() > 0 ? FGameplayTag::RequestGameplayTag(FName(*(TEXT("Station.") + plain[0])), false) : FGameplayTag();
        if(crafting == nullptr || pawn == nullptr || !stationTag.IsValid()){
            CRAFTING_LOG(Warning, world, "Usage: Awaken.Craft.SpawnStation <CraftingTable|Forge> [pN]");
            return;
        }
        AStubCraftingStation* stationActor = world->SpawnActor<AStubCraftingStation>(pawn->GetActorLocation() + pawn->GetActorForwardVector() * 150.0f, FRotator::ZeroRotator);
        if(stationActor == nullptr){
            return;
        }
        stationActor->stationComponent->stationTags.AddTag(stationTag);
        const FS_InventoryResult result = crafting->enterStation(stationActor->stationComponent);
        CRAFTING_LOG(Log, world, "SpawnStation %s: %s", *plain[0], result.succeeded() ? TEXT("spawned and entered") : *result.failReason.ToString());
    }

    // TEMPORARY — owner: Doc R06 (its real command is Awaken.Unlock.Grant). For maps whose GameState has no Progression
    // component (TestMap_Inventory): replaces the progression seam on THIS machine with the local stub, then sets a tag.
    void stubUnlockCommand(const TArray<FString>& arguments, UWorld* world){
        UInventoryServices* services = UInventoryServices::get(world);
        if(services == nullptr || arguments.Num() < 1){
            CRAFTING_LOG(Warning, world, "Usage: Awaken.Craft.StubUnlock <tag|all> [0|1]  (switches this machine to the local progression stub)");
            return;
        }
        UStubProgressionQuery* stub = Cast<UStubProgressionQuery>(services->getSeam(UBPI_ProgressionQuery::StaticClass()));
        if(stub == nullptr){
            stub = NewObject<UStubProgressionQuery>(services);
            services->registerSeam(UBPI_ProgressionQuery::StaticClass(), stub);
            CRAFTING_LOG(Warning, world, "StubUnlock: this machine now answers unlocks from the local stub, not Doc R06 (until restart)");
        }
        const bool isOn = arguments.Num() < 2 || arguments[1] != TEXT("0");
        if(arguments[0] == TEXT("all")){
            stub->unlockEverything = isOn;
        }else{
            const FGameplayTag tag = FGameplayTag::RequestGameplayTag(FName(*arguments[0]), false);
            if(!tag.IsValid()){
                CRAFTING_LOG(Warning, world, "StubUnlock: unknown tag '%s'", *arguments[0]);
                return;
            }
            if(isOn){
                stub->unlockedTags.AddTag(tag);
            }else{
                stub->unlockedTags.RemoveTag(tag);
            }
        }
        CRAFTING_LOG(Log, world, "StubUnlock %s = %d on this machine", *arguments[0], isOn ? 1 : 0);
    }

    // Read only.
    void listCommand(const TArray<FString>& arguments, UWorld* world){
        const UCraftingComponent* crafting = UCraftingComponent::findFor(resolvePlayer(world, arguments));
        if(crafting == nullptr){
            return;
        }
        const FS_CraftingSessionState state = crafting->getSessionState();
        CRAFTING_LOG(Log, world, "Session %s, station %s, running craft %s", state.isOpen ? TEXT("OPEN") : TEXT("closed"), state.hasStation ? *state.stationTags.ToStringSimple() : TEXT("none"), state.activeRecipeId.IsNone() ? TEXT("none") : *state.activeRecipeId.ToString());
        for(const TPair<FName, int32>& staged : crafting->getStagedQuantities()){
            CRAFTING_LOG(Log, world, "   staged %s x%d", *staged.Key.ToString(), staged.Value);
        }
        logList(world, crafting);
    }

    // Read only. The editor data check of R02 §12.
    void validateCommand(const TArray<FString>& arguments, UWorld* world){
        TArray<FString> errors;
        const int32 errorCount = UBFL_CraftingValidation::validateRecipes(world, errors);
        for(const FString& error : errors){
            CRAFTING_LOG(Error, world, "%s", *error);
        }
        CRAFTING_LOG(Log, world, "Recipe validation: %d error(s)", errorCount);
    }

    FAutoConsoleCommandWithWorldAndArgs cmdStage(TEXT("Awaken.Craft.Stage"), TEXT("<itemId> [quantity] [pN]: stage an ingredient, then print the craft buttons"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&stageCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdUnstage(TEXT("Awaken.Craft.Unstage"), TEXT("<itemId> [quantity] [pN]: return staged units"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&unstageCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdStageAll(TEXT("Awaken.Craft.StageAll"), TEXT("[pN]: stage everything in the bag that can be staged (the full crafting area)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&stageAllCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdCraft(TEXT("Awaken.Craft.Craft"), TEXT("<recipeId> [pN]: press that craft button"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&craftCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdClose(TEXT("Awaken.Craft.Close"), TEXT("[pN]: close the bag: end the session and leave the station"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&closeCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdStation(TEXT("Awaken.Craft.SpawnStation"), TEXT("<CraftingTable|Forge> [pN]: TEMPORARY, spawn a stub station and enter it"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&spawnStationCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdUnlock(TEXT("Awaken.Craft.StubUnlock"), TEXT("<tag|all> [0|1]: TEMPORARY, set an unlock in the local progression stub"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&stubUnlockCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdList(TEXT("Awaken.Craft.List"), TEXT("[pN]: print the session, what is staged and the craft buttons (read)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&listCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdValidate(TEXT("Awaken.Craft.Validate"), TEXT(": check every recipe against the item catalog (read)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&validateCommand));
}
}

#endif
