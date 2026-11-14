// Doc B22 §3.2, §3.4: Awaken.Carry.* commands. Host only, through the owner's public functions, never save. Non-shipping only.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "CarryEncumbranceFramework/CarryLog.h"
#include "CarryEncumbranceFramework/UBFL_Carry.h"
#include "CarryEncumbranceFramework/UCarryComponent.h"
#include "CarryEncumbranceFramework/Stubs/CarryStubs.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"

// The file-named outer namespace keeps these helpers apart from other DevTools files in a unity build.
namespace CarryDevCommands{
namespace{
    bool isPlayerArgument(const FString& argument){
        return argument.Len() > 1 && argument.StartsWith(TEXT("p"), ESearchCase::CaseSensitive) && argument.Mid(1).IsNumeric();
    }

    // Returns null with a log line when not on the host or when the player does not exist.
    APlayerState* resolvePlayer(UWorld* world, const TArray<FString>& arguments){
        if(world == nullptr || !UBFL_Inventory::isServerWorld(world)){
            CARRY_LOG(Warning, world, "Host command: refused on a client (the Doc B22 cheat manager forwards it)");
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
            CARRY_LOG(Warning, world, "No player %d", playerIndex);
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

    // Through R01's grantOrDrop, so the real path runs: a free slot, otherwise loose beside the player.
    void giveCommand(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = resolvePlayer(world, arguments);
        const TArray<FString> plain = plainArguments(arguments);
        if(player == nullptr || plain.Num() < 1){
            CARRY_LOG(Warning, world, "Usage: Awaken.Carry.Give <itemId> [count] [pN]");
            return;
        }
        FS_ItemStack stack;
        stack.itemId = FName(*plain[0]);
        stack.quantity = plain.Num() > 1 ? FMath::Max(1, FCString::Atoi(*plain[1])) : 1;
        const FS_GrantResult result = UBFL_Inventory::grantOrDrop(player, stack);
        CARRY_LOG(Log, world, "Give %s x%d: carried %d, dropped %d, stored %d", *plain[0], stack.quantity, result.quantityCarried, result.quantityDropped, result.quantityStored);
    }

    void dropCommand(const TArray<FString>& arguments, UWorld* world){
        UCarryComponent* carry = UCarryComponent::findFor(resolvePlayer(world, arguments));
        const TArray<FString> plain = plainArguments(arguments);
        if(carry == nullptr){
            CARRY_LOG(Warning, world, "Usage: Awaken.Carry.Drop [slotIndex] [pN]");
            return;
        }
        const int32 slotIndex = plain.Num() > 0 ? FCString::Atoi(*plain[0]) : carry->getActiveSlotIndex();
        const FS_CarryEndResult result = carry->endCarry(slotIndex, E_EndCarryReason::Dropped);
        CARRY_LOG(Log, world, "Drop slot %d: %s", slotIndex, result.succeeded() ? TEXT("done") : *result.failReason.ToString());
    }

    void throwCommand(const TArray<FString>& arguments, UWorld* world){
        const APlayerState* player = resolvePlayer(world, arguments);
        UCarryComponent* carry = UCarryComponent::findFor(player);
        const APawn* pawn = player != nullptr ? player->GetPawn() : nullptr;
        const TArray<FString> plain = plainArguments(arguments);
        if(carry == nullptr || pawn == nullptr){
            CARRY_LOG(Warning, world, "Usage: Awaken.Carry.Throw [slotIndex] [pN]");
            return;
        }
        const int32 slotIndex = plain.Num() > 0 ? FCString::Atoi(*plain[0]) : carry->getActiveSlotIndex();
        const FS_CarryEndResult result = carry->throwCarried(slotIndex, pawn->GetBaseAimRotation().Vector());
        CARRY_LOG(Log, world, "Throw slot %d: %s", slotIndex, result.succeeded() ? TEXT("done") : *result.failReason.ToString());
    }

    void interruptCommand(const TArray<FString>& arguments, UWorld* world){
        UCarryComponent* carry = UCarryComponent::findFor(resolvePlayer(world, arguments));
        const TArray<FString> plain = plainArguments(arguments);
        if(carry == nullptr || plain.Num() < 1){
            CARRY_LOG(Warning, world, "Usage: Awaken.Carry.Interrupt <Downed|Death|Restrained|PlayerLeft|Swim|Sleep> [pN]");
            return;
        }
        const FGameplayTag causeTag = FGameplayTag::RequestGameplayTag(FName(*(TEXT("Carry.InterruptCause.") + plain[0])), false);
        if(!causeTag.IsValid()){
            CARRY_LOG(Warning, world, "Interrupt: unknown cause '%s'", *plain[0]);
            return;
        }
        // The forced drop only. It does NOT make the player Downed or asleep: those states belong to their owners.
        const int32 dropped = carry->interruptCarry(causeTag);
        CARRY_LOG(Log, world, "Interrupt %s: %d thing(s) dropped", *plain[0], dropped);
    }

    // The situation most likely to break (Doc B22 §3.10): full hands. Adds things, so it is not safe to repeat.
    void fillHandsCommand(const TArray<FString>& arguments, UWorld* world){
        UCarryComponent* carry = UCarryComponent::findFor(resolvePlayer(world, arguments));
        const TArray<FString> plain = plainArguments(arguments);
        if(carry == nullptr || plain.Num() < 1){
            CARRY_LOG(Warning, world, "Usage: Awaken.Carry.FillHands <itemId> [pN]");
            return;
        }
        FS_InventoryEntry entry;
        entry.itemId = FName(*plain[0]);
        entry.totalQuantity = 1;
        int32 taken = 0;
        while(carry->getFreeSlotCount() > 0 && carry->beginCarry(entry).succeeded()){
            taken++;
        }
        // One more on purpose, to show the refusal. Nothing is created by a refused carry, so nothing is lost.
        const FS_CarryBeginResult extra = carry->beginCarry(entry);
        CARRY_LOG(Log, world, "FillHands %s: took %d, %d of %d slots free, one more -> %s", *plain[0], taken, carry->getFreeSlotCount(), carry->getSlotCount(), extra.succeeded() ? TEXT("accepted (unexpected)") : *extra.failReason.ToString());
    }

    // TEMPORARY — owners: Docs B08, B12. Spawns the stub carryable actor and puts it in the player's hands.
    void carryDummyCommand(const TArray<FString>& arguments, UWorld* world){
        const APlayerState* player = resolvePlayer(world, arguments);
        UCarryComponent* carry = UCarryComponent::findFor(player);
        const APawn* pawn = player != nullptr ? player->GetPawn() : nullptr;
        if(carry == nullptr || pawn == nullptr){
            CARRY_LOG(Warning, world, "Usage: Awaken.Carry.CarryDummy [pN]");
            return;
        }
        AStubCarryableActor* dummy = world->SpawnActor<AStubCarryableActor>(pawn->GetActorLocation() + pawn->GetActorForwardVector() * 100.0f, FRotator::ZeroRotator);
        const FS_CarryBeginResult result = carry->beginCarryActor(dummy);
        if(!result.succeeded() && dummy != nullptr){
            dummy->Destroy();
        }
        CARRY_LOG(Log, world, "CarryDummy: %s", result.succeeded() ? TEXT("done") : *result.failReason.ToString());
    }

    // Read only. Runs on any machine: it prints the replicated state this machine holds.
    void dumpCommand(const TArray<FString>& arguments, UWorld* world){
        const AGameStateBase* gameState = world != nullptr ? world->GetGameState() : nullptr;
        if(gameState == nullptr){
            return;
        }
        for(const APlayerState* player : gameState->PlayerArray){
            const UCarryComponent* carry = UCarryComponent::findFor(player);
            if(carry == nullptr){
                continue;
            }
            const FGameplayTag cause = carry->getLastInterruptCause();
            CARRY_LOG(Log, world, "%s: %s, %d of %d slots free, active slot %d, speed x%.2f, last interrupt %s", *player->GetPlayerName(), carry->isCarryActive() ? TEXT("CARRYING") : TEXT("hands empty"), carry->getFreeSlotCount(), carry->getSlotCount(), carry->getActiveSlotIndex(), carry->getSpeedMultiplier(), cause.IsValid() ? *cause.ToString() : TEXT("none"));
            for(int32 index = 0; index < carry->getCarrySlots().Num(); index++){
                const FS_CarrySlotState& slot = carry->getCarrySlots()[index];
                CARRY_LOG(Log, world, "   [%d] %s%s", index, slot.isOccupied() ? *slot.itemId.ToString() : TEXT("-"), slot.holdsActor ? TEXT(" (actor)") : TEXT(""));
            }
        }
    }

    FAutoConsoleCommandWithWorldAndArgs cmdGive(TEXT("Awaken.Carry.Give"), TEXT("<itemId> [count] [pN]: grant a heavy item; it goes to the hands or lands loose"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&giveCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdDrop(TEXT("Awaken.Carry.Drop"), TEXT("[slotIndex] [pN]: drop a carried thing (default: the active slot)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&dropCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdThrow(TEXT("Awaken.Carry.Throw"), TEXT("[slotIndex] [pN]: throw a carried item where the player looks"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&throwCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdInterrupt(TEXT("Awaken.Carry.Interrupt"), TEXT("<cause> [pN]: the forced drop of B07 3.6 with that cause"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&interruptCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdFill(TEXT("Awaken.Carry.FillHands"), TEXT("<itemId> [pN]: fill every carry slot, then try one more (the full-hands situation)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&fillHandsCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdDummy(TEXT("Awaken.Carry.CarryDummy"), TEXT("[pN]: TEMPORARY, carry a stub actor (stands in for a corpse)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&carryDummyCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdDump(TEXT("Awaken.Carry.Dump"), TEXT(": print every player's carry slots as this machine sees them (read)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&dumpCommand));
}
}

#endif
