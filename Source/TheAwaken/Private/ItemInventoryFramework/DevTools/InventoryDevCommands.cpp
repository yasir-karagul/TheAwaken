#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/InventoryTime.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UBFL_InventoryBag.h"
#include "ItemInventoryFramework/UBFL_InventoryValidation.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UHeldItemComponent.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/03Interfaces/BPI_BagActorService.h"
#include "ItemInventoryFramework/Stubs/InventoryStubs.h"
#include "SaveFramework/03Interfaces/BPI_SaveParticipant.h"
#include "StructUtils/InstancedStruct.h"
#include "Templates/Function.h"

namespace InventoryDev{
    bool isPlayerArg(const FString& argument){
        return argument.Len() > 1 && argument.StartsWith(TEXT("p"), ESearchCase::CaseSensitive) && argument.Mid(1).IsNumeric();
    }

    // The arguments that are not the player selector, in order.
    TArray<FString> plainArgs(const TArray<FString>& arguments){
        TArray<FString> plain;
        for(const FString& argument : arguments){
            if(!isPlayerArg(argument)){
                plain.Add(argument);
            }
        }
        return plain;
    }

    // Null, with a log line, when this is not the host or the player does not exist.
    APlayerState* findPlayer(UWorld* world, const TArray<FString>& arguments){
        if(world == nullptr || !UBFL_Inventory::isServerWorld(world)){
            INVENTORY_LOG(Warning, world, "Host command: refused on a client (the Doc B22 cheat manager forwards it)");
            return nullptr;
        }
        int32 playerIndex = 0;
        for(const FString& argument : arguments){
            if(isPlayerArg(argument)){
                playerIndex = FCString::Atoi(*argument.Mid(1));
            }
        }
        const AGameStateBase* gameState = world->GetGameState();
        if(gameState == nullptr || !gameState->PlayerArray.IsValidIndex(playerIndex)){
            INVENTORY_LOG(Warning, world, "No player %d", playerIndex);
            return nullptr;
        }
        return gameState->PlayerArray[playerIndex];
    }

    FS_ItemStack makeStack(const FString& itemId, int32 quantity){
        FS_ItemStack stack;
        stack.itemId = FName(*itemId);
        stack.quantity = quantity;
        return stack;
    }

    // Awaken.Item.Give <itemId> [amount] [pN]. Adds things, so it is not safe to repeat.
    void give(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = findPlayer(world, arguments);
        const TArray<FString> plain = plainArgs(arguments);
        if(player == nullptr || plain.Num() < 1){
            INVENTORY_LOG(Warning, world, "Usage: Awaken.Item.Give <itemId> [amount] [pN]");
            return;
        }
        const int32 amount = plain.Num() > 1 ? FMath::Max(1, FCString::Atoi(*plain[1])) : 1;
        const FS_GrantResult result = UBFL_Inventory::grantOrDrop(player, makeStack(plain[0], amount));   // a full bag drops it (Doc02 §19.5)
        INVENTORY_LOG(Log, world, "Give %s x%d: stored %d, carried %d, dropped %d", *plain[0], amount, result.quantityStored, result.quantityCarried, result.quantityDropped);
    }

    // Awaken.Item.FillBag <itemId> [pN]. The situation most likely to break: a type at its limit, plus three more.
    void fillBag(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = findPlayer(world, arguments);
        const TArray<FString> plain = plainArgs(arguments);
        const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(world);
        const UDA_ItemDefinition* definition = (catalog != nullptr && plain.Num() > 0) ? catalog->getDefinition(FName(*plain[0])) : nullptr;
        if(player == nullptr || definition == nullptr){
            INVENTORY_LOG(Warning, world, "Usage: Awaken.Item.FillBag <itemId> [pN] (unknown item or no player)");
            return;
        }
        const FS_GrantResult result = UBFL_Inventory::grantOrDrop(player, makeStack(plain[0], definition->perTypeLimit + 3));
        INVENTORY_LOG(Log, world, "FillBag %s (limit %d): stored %d, dropped %d as loose items", *plain[0], definition->perTypeLimit, result.quantityStored, result.quantityDropped);
    }

    // Awaken.Item.SpawnLoose <itemId> [amount]. A loose item in front of player 0.
    void spawnLoose(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = findPlayer(world, arguments);
        const TArray<FString> plain = plainArgs(arguments);
        const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(world);
        const UInventoryServices* services = UInventoryServices::get(world);
        const UDA_ItemDefinition* definition = (catalog != nullptr && plain.Num() > 0) ? catalog->getDefinition(FName(*plain[0])) : nullptr;
        if(player == nullptr || services == nullptr || definition == nullptr){
            INVENTORY_LOG(Warning, world, "Usage: Awaken.Item.SpawnLoose <itemId> [amount] (unknown item or no player)");
            return;
        }
        FS_InventoryEntry entry = definition->isInstanced() ? UBFL_Inventory::makeFreshInstance(world, *definition) : FS_InventoryEntry();
        entry.itemId = definition->itemId;
        entry.totalQuantity = definition->isInstanced() ? 1 : (plain.Num() > 1 ? FMath::Max(1, FCString::Atoi(*plain[1])) : 1);
        if(definition->canSpoil){
            entry.spoilAtTime = services->getGameTime() + InventoryTime::hoursToGameSeconds(definition->spoilTimeHours);
        }
        services->spawnLooseItem(player, entry, UBFL_Inventory::getDropLocation(player));
        INVENTORY_LOG(Log, world, "SpawnLoose %s x%d", *plain[0], entry.totalQuantity);
    }

    // Awaken.Item.ClearBag [pN]. Keeps the Axe and the map (Doc B22 §3.4: a dev command uses the normal rule). Safe to repeat.
    void clearBag(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = findPlayer(world, arguments);
        const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
        const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(world);
        UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(world);
        UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
        if(bag == nullptr || catalog == nullptr || registry == nullptr){
            INVENTORY_LOG(Log, world, "ClearBag: nothing to do (no player or no bag)");
            return;
        }
        UBFL_Inventory::releaseAllStaged(player);
        const TArray<FS_InventoryEntry> entries = bag->getEntries();   // a copy: removing changes the array
        int32 removed = 0;
        for(const FS_InventoryEntry& entry : entries){
            const UDA_ItemDefinition* definition = catalog->getDefinition(entry.itemId);
            if(definition != nullptr && definition->isPermanent()){
                continue;
            }
            if(entry.isInstanced()){
                registry->destroyInventory(entry.state.containedInventoryId);   // the contents of a pouch go with it
                bag->removeInstance(entry.instanceId);
            }else{
                bag->removeStack(entry.itemId, entry.totalQuantity);
            }
            removed++;
        }
        INVENTORY_LOG(Log, world, "ClearBag: removed %d entries, kept the permanent items", removed);
    }

    // Awaken.Bag.DropHere [pN] and Awaken.Bag.SetBagless [pN].
    void dropBag(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = findPlayer(world, arguments);
        if(player == nullptr){
            return;
        }
        const APawn* pawn = player->GetPawn();
        const FS_BagDetachResult result = UBFL_InventoryBag::detachBag(player, pawn != nullptr ? pawn->GetActorLocation() : FVector::ZeroVector);
        INVENTORY_LOG(Log, world, "Bag drop for %s: %s", *player->GetPlayerName(), result.succeeded() ? *result.bagId.ToString() : *result.failReason.ToString());
    }

    // Awaken.Bag.Recover [pN]. Recovers the bag that this player's character left in the world.
    void recoverBag(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = findPlayer(world, arguments);
        const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
        const UInventoryServices* services = UInventoryServices::get(world);
        UObject* bagService = services != nullptr ? services->getBagService() : nullptr;
        if(inventory == nullptr || bagService == nullptr){
            return;
        }
        const FS_BagInfo info = IBPI_BagActorService::Execute_findBagOwnedBy(bagService, inventory->getCharacterId());
        if(!info.isValid){
            INVENTORY_LOG(Log, world, "Recover: %s has no bag lying in the world", *player->GetPlayerName());
            return;
        }
        const FS_InventoryResult result = UBFL_InventoryBag::reattachBag(player, info.bagId);
        INVENTORY_LOG(Log, world, "Recover for %s: %s", *player->GetPlayerName(), result.succeeded() ? TEXT("done") : *result.failReason.ToString());
    }

    // Awaken.Hotbar.Bind <slot> <itemId|none> [pN] and Awaken.Hotbar.Select <slot> [pN].
    void bindSlot(const TArray<FString>& arguments, UWorld* world){
        UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(findPlayer(world, arguments));
        const TArray<FString> plain = plainArgs(arguments);
        if(inventory == nullptr || plain.Num() < 2){
            INVENTORY_LOG(Warning, world, "Usage: Awaken.Hotbar.Bind <slot> <itemId|none> [pN]");
            return;
        }
        const FName itemId = plain[1].Equals(TEXT("none"), ESearchCase::IgnoreCase) ? FName() : FName(*plain[1]);
        const FS_InventoryResult result = inventory->bindHotbarSlot(FCString::Atoi(*plain[0]), itemId);
        INVENTORY_LOG(Log, world, "Bind slot %s to %s: %s", *plain[0], *plain[1], result.succeeded() ? TEXT("done") : *result.failReason.ToString());
    }

    void selectSlot(const TArray<FString>& arguments, UWorld* world){
        UHeldItemComponent* heldItem = UHeldItemComponent::findFor(findPlayer(world, arguments));
        const TArray<FString> plain = plainArgs(arguments);
        if(heldItem == nullptr || plain.Num() < 1){
            INVENTORY_LOG(Warning, world, "Usage: Awaken.Hotbar.Select <slot> [pN] (the player needs a pawn)");
            return;
        }
        const FS_InventoryResult result = heldItem->selectHotbarSlot(FCString::Atoi(*plain[0]));
        INVENTORY_LOG(Log, world, "Select slot %s: %s", *plain[0], result.succeeded() ? TEXT("done") : *result.failReason.ToString());
    }

    // Awaken.Item.Dump (read). Runs on any machine and prints what THIS machine knows.
    void dump(const TArray<FString>& arguments, UWorld* world){
        const AGameStateBase* gameState = world != nullptr ? world->GetGameState() : nullptr;
        if(gameState == nullptr){
            return;
        }
        for(const APlayerState* player : gameState->PlayerArray){
            const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
            if(inventory == nullptr){
                continue;
            }
            const FS_HeldItem held = UBFL_Inventory::getHeldState(player);
            INVENTORY_LOG(Log, world, "%s: bag %s, %d entries, usable slots %d, hand %s%s", *player->GetPlayerName(), inventory->hasBag() ? TEXT("yes") : TEXT("NO (Bagless)"), inventory->getBagEntries().Num(), inventory->getUsableHotbarSlotCount(), held.isEmpty() ? TEXT("empty") : *held.itemId.ToString(), held.isWielded ? TEXT(" (wielded)") : TEXT(""));
            for(const FS_InventoryEntry& entry : inventory->getBagEntries()){
                INVENTORY_LOG(Log, world, "   %s x%d (reserved %d) durability %.1f spoilAt %lld %s", *entry.itemId.ToString(), entry.totalQuantity, entry.reservedQuantity, entry.state.durability, entry.spoilAtTime, entry.isInstanced() ? *entry.instanceId.ToString().Left(8) : TEXT(""));
            }
            for(const FS_HotbarBinding& binding : inventory->getHotbarBindings()){
                if(!binding.boundItemId.IsNone()){
                    INVENTORY_LOG(Log, world, "   key %d -> %s", binding.slotIndex, *binding.boundItemId.ToString());
                }
            }
            const TArray<FS_BaglessHoldSlot> holdSlots = inventory->getHoldSlots();
            for(int32 index = 0; index < holdSlots.Num(); index++){
                if(holdSlots[index].isOccupied){
                    INVENTORY_LOG(Log, world, "   hold slot (key %d): %s", index + 1, *holdSlots[index].entry.itemId.ToString());
                }
            }
        }
        const UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(world);
        if(registry != nullptr){
            INVENTORY_LOG(Log, world, "Registry: %d inventories (0 on a client)", registry->getAllInventories().Num());
        }
    }

    // Awaken.Item.Validate (read). Every item's own check plus the catalog checks of S10.4.
    void validate(const TArray<FString>& arguments, UWorld* world){
        TArray<FString> errors;
        const int32 count = UBFL_InventoryValidation::validateCatalog(world, errors);
        for(const FString& error : errors){
            INVENTORY_LOG(Error, world, "Validate: %s", *error);
        }
        INVENTORY_LOG(Log, world, "Validate: %d error(s)", count);
    }

    // ---- TEMPORARY commands for the stubs. Delete each with its stub (S13.8) ----

    // Awaken.Inventory.StubClock <hours>. Moves the stub game clock forward. Owner: Doc R12 (Awaken.Time.*).
    void stubClock(const TArray<FString>& arguments, UWorld* world){
        const UInventoryServices* services = UInventoryServices::get(world);
        UStubGameClock* clock = services != nullptr ? Cast<UStubGameClock>(services->getSeam(UBPI_GameClock::StaticClass())) : nullptr;
        if(clock == nullptr || arguments.Num() < 1 || !UBFL_Inventory::isServerWorld(world)){
            INVENTORY_LOG(Warning, world, "Usage: Awaken.Inventory.StubClock <hours> (host only; does nothing once Doc R12's clock is registered)");
            return;
        }
        clock->gameTime += InventoryTime::hoursToGameSeconds(FCString::Atof(*arguments[0]));
        INVENTORY_LOG(Log, world, "Stub clock is now at %lld game seconds", clock->gameTime);
    }

    // Awaken.Inventory.StubFlag <carrying|downed> <0|1> [pN]. Owners: Doc B07 and Docs B02, R03.
    void stubFlag(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = findPlayer(world, arguments);
        const TArray<FString> plain = plainArgs(arguments);
        const UInventoryServices* services = UInventoryServices::get(world);
        if(player == nullptr || services == nullptr || plain.Num() < 2){
            INVENTORY_LOG(Warning, world, "Usage: Awaken.Inventory.StubFlag <carrying|downed> <0|1> [pN]");
            return;
        }
        const bool isOn = FCString::Atoi(*plain[1]) != 0;
        UStubCarryService* carry = Cast<UStubCarryService>(services->getSeam(UBPI_CarryService::StaticClass()));
        UStubPlayerStateQuery* state = Cast<UStubPlayerStateQuery>(services->getSeam(UBPI_PlayerStateQuery::StaticClass()));
        TSet<TWeakObjectPtr<APlayerState>>* flagged = plain[0] == TEXT("carrying") ? (carry != nullptr ? &carry->carryingPlayers : nullptr) : (state != nullptr ? &state->incapacitatedPlayers : nullptr);
        if(flagged == nullptr){
            INVENTORY_LOG(Log, world, "StubFlag: that stub is no longer registered");
            return;
        }
        if(isOn){
            flagged->Add(player);
        }else{
            flagged->Remove(player);
        }
        INVENTORY_LOG(Log, world, "StubFlag %s = %d for %s", *plain[0], isOn ? 1 : 0, *player->GetPlayerName());
    }

    // Awaken.Inventory.StubSave and Awaken.Inventory.StubLoad. An in-memory round trip through Doc R13's
    // IBPI_SaveParticipant (R13 has no save console command yet). Nothing is written to disk. Remove when R13 adds Awaken.Save.*.
    struct FCapturedSnapshot{
        TWeakObjectPtr<UObject> participant;
        FInstancedStruct snapshot;
        int32 version = 0;
    };
    TArray<FCapturedSnapshot> capturedWorld;
    TArray<FCapturedSnapshot> capturedCharacters;

    // The participants of this framework: the registry (World), and every player's inventory component (Character).
    void forEachParticipant(UWorld* world, TFunctionRef<void(UObject*, bool)> visit){
        if(UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(world)){
            visit(registry, true);
        }
        const AGameStateBase* gameState = world != nullptr ? world->GetGameState() : nullptr;
        if(gameState != nullptr){
            for(APlayerState* player : gameState->PlayerArray){
                if(UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player)){
                    visit(inventory, false);
                }
            }
        }
    }

    void stubSave(const TArray<FString>& arguments, UWorld* world){
        if(!UBFL_Inventory::isServerWorld(world)){
            return;
        }
        capturedWorld.Reset();
        capturedCharacters.Reset();
        forEachParticipant(world, [](UObject* participant, bool isWorldScope){
            FCapturedSnapshot captured;
            captured.participant = participant;
            captured.version = IBPI_SaveParticipant::Execute_getSnapshotVersion(participant);
            // False = "keep my previous record" (a Bagless player): nothing is captured for it.
            if(IBPI_SaveParticipant::Execute_getSnapshotForSave(participant, captured.snapshot)){
                (isWorldScope ? capturedWorld : capturedCharacters).Add(MoveTemp(captured));
            }
        });
        INVENTORY_LOG(Log, world, "StubSave: captured %d world and %d character snapshot(s) in memory", capturedWorld.Num(), capturedCharacters.Num());
    }

    void stubLoad(const TArray<FString>& arguments, UWorld* world){
        if(!UBFL_Inventory::isServerWorld(world)){
            return;
        }
        int32 restored = 0;
        // World scope first, then Character (Doc B06 3.6, guide S11.7).
        for(const TArray<FCapturedSnapshot>* pass : {&capturedWorld, &capturedCharacters}){
            for(const FCapturedSnapshot& captured : *pass){
                UObject* participant = captured.participant.Get();
                if(participant != nullptr && IBPI_SaveParticipant::Execute_restoreFromSnapshot(participant, captured.snapshot, captured.version)){
                    restored++;
                }
            }
        }
        INVENTORY_LOG(Log, world, "StubLoad: restored %d of %d snapshot(s)", restored, capturedWorld.Num() + capturedCharacters.Num());
    }

    FAutoConsoleCommandWithWorldAndArgs cmdGive(TEXT("Awaken.Item.Give"), TEXT("<itemId> [amount] [pN]: grant through grantOrDrop, so a full bag drops it"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&give));
    FAutoConsoleCommandWithWorldAndArgs cmdFillBag(TEXT("Awaken.Item.FillBag"), TEXT("<itemId> [pN]: grant the per-type limit plus three (the full-bag situation)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&fillBag));
    FAutoConsoleCommandWithWorldAndArgs cmdSpawnLoose(TEXT("Awaken.Item.SpawnLoose"), TEXT("<itemId> [amount]: a loose item in front of the player"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&spawnLoose));
    FAutoConsoleCommandWithWorldAndArgs cmdClearBag(TEXT("Awaken.Item.ClearBag"), TEXT("[pN]: empty the bag, keeping the Axe and the map"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&clearBag));
    FAutoConsoleCommandWithWorldAndArgs cmdDump(TEXT("Awaken.Item.Dump"), TEXT(": print every player's bag, hotbar, hold slots and hand as this machine sees them (read)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&dump));
    FAutoConsoleCommandWithWorldAndArgs cmdValidate(TEXT("Awaken.Item.Validate"), TEXT(": run the item data checks (read)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&validate));
    FAutoConsoleCommandWithWorldAndArgs cmdDropHere(TEXT("Awaken.Bag.DropHere"), TEXT("[pN]: detach the bag where the player stands"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&dropBag));
    FAutoConsoleCommandWithWorldAndArgs cmdSetBagless(TEXT("Awaken.Bag.SetBagless"), TEXT("[pN]: force the Bagless state by detaching the bag the normal way"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&dropBag));
    FAutoConsoleCommandWithWorldAndArgs cmdRecover(TEXT("Awaken.Bag.Recover"), TEXT("[pN]: recover the bag this player's character left in the world"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&recoverBag));
    FAutoConsoleCommandWithWorldAndArgs cmdBind(TEXT("Awaken.Hotbar.Bind"), TEXT("<slot> <itemId|none> [pN]: bind a hotbar slot"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&bindSlot));
    FAutoConsoleCommandWithWorldAndArgs cmdSelect(TEXT("Awaken.Hotbar.Select"), TEXT("<slot> [pN]: take what the slot resolves to in the hand"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&selectSlot));
    FAutoConsoleCommandWithWorldAndArgs cmdStubClock(TEXT("Awaken.Inventory.StubClock"), TEXT("<hours>: TEMPORARY, move the stub game clock forward"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&stubClock));
    FAutoConsoleCommandWithWorldAndArgs cmdStubFlag(TEXT("Awaken.Inventory.StubFlag"), TEXT("<carrying|downed> <0|1> [pN]: TEMPORARY, set a stub player state"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&stubFlag));
    FAutoConsoleCommandWithWorldAndArgs cmdStubSave(TEXT("Awaken.Inventory.StubSave"), TEXT(": TEMPORARY, keep every inventory save snapshot (Doc R13 interface) in memory"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&stubSave));
    FAutoConsoleCommandWithWorldAndArgs cmdStubLoad(TEXT("Awaken.Inventory.StubLoad"), TEXT(": TEMPORARY, restore the snapshots kept by StubSave"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&stubLoad));
}

#endif
