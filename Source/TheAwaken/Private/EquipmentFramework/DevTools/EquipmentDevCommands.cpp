#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "EquipmentFramework/UEquipmentComponent.h"
#include "EquipmentFramework/UBFL_Equipment.h"
#include "EquipmentFramework/EquipmentLog.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/UInventoryData.h"

namespace{
    bool isPlayerArgument(const FString& argument){
        return argument.Len() > 1 && argument.StartsWith(TEXT("p"), ESearchCase::CaseSensitive) && argument.Mid(1).IsNumeric();
    }

    // Returns null with a log line when not on the host or when the player does not exist.
    APlayerState* resolvePlayer(UWorld* world, const TArray<FString>& arguments){
        if(world == nullptr || !UBFL_Inventory::isServerWorld(world)){
            EQUIPMENT_LOG(Warning, world, "Host command: refused on a client (the Doc B22 cheat manager forwards it)");
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
            EQUIPMENT_LOG(Warning, world, "No player %d", playerIndex);
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

    // The first copy of an instanced item in the player's bag, or an invalid id.
    FGuid findFirstInBag(const APlayerState* player, FName itemId){
        const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
        const UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
        if(bag != nullptr){
            for(const FS_InventoryEntry& entry : bag->getEntries()){
                if(entry.itemId == itemId && entry.isInstanced()){
                    return entry.instanceId;
                }
            }
        }
        return FGuid();
    }

    void equipCommand(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = resolvePlayer(world, arguments);
        UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
        const TArray<FString> plain = plainArguments(arguments);
        if(equipment == nullptr || plain.Num() < 1){
            EQUIPMENT_LOG(Warning, world, "Usage: Awaken.Equipment.Equip <itemId> [pN]");
            return;
        }
        const FGuid instanceId = findFirstInBag(player, FName(*plain[0]));
        if(!instanceId.IsValid()){
            EQUIPMENT_LOG(Log, world, "Equip: no %s in the bag. Use Awaken.Item.Give %s 1 first", *plain[0], *plain[0]);
            return;
        }
        const FS_InventoryResult result = equipment->equipArmor(instanceId);
        EQUIPMENT_LOG(Log, world, "Equip %s: %s", *plain[0], result.succeeded() ? TEXT("done") : *result.failReason.ToString());
    }

    void unequipCommand(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = resolvePlayer(world, arguments);
        UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
        const TArray<FString> plain = plainArguments(arguments);
        if(equipment == nullptr || plain.Num() < 1){
            EQUIPMENT_LOG(Warning, world, "Usage: Awaken.Equipment.Unequip <stackIndex> [pN]");
            return;
        }
        const FS_InventoryResult result = equipment->unequipArmor(FCString::Atoi(*plain[0]));
        EQUIPMENT_LOG(Log, world, "Unequip position %s: %s", *plain[0], result.succeeded() ? TEXT("done") : *result.failReason.ToString());
    }

    // The situation most likely to break (Doc B22 §3.10): a full stack. Adds things, so it is not safe to repeat.
    void fillStackCommand(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = resolvePlayer(world, arguments);
        UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
        const TArray<FString> plain = plainArguments(arguments);
        if(equipment == nullptr || plain.Num() < 1){
            EQUIPMENT_LOG(Warning, world, "Usage: Awaken.Equipment.FillStack <itemId> [pN]");
            return;
        }
        const FName itemId(*plain[0]);
        FS_ItemStack single;
        single.itemId = itemId;
        single.quantity = 1;
        int32 equipped = 0;
        while(equipment->getArmorStack().Num() < equipment->getArmorStackCap()){
            UBFL_Inventory::grantOrDrop(player, single);   // through R01, so a full bag drops it (Doc02 §19.5)
            const FGuid instanceId = findFirstInBag(player, itemId);
            if(!instanceId.IsValid() || !equipment->equipArmor(instanceId).succeeded()){
                break;
            }
            equipped++;
        }
        // One more on purpose, to show the refusal at the cap.
        UBFL_Inventory::grantOrDrop(player, single);
        const FS_InventoryResult extra = equipment->equipArmor(findFirstInBag(player, itemId));
        EQUIPMENT_LOG(Log, world, "FillStack %s: equipped %d, stack %d of %d, one more -> %s", *plain[0], equipped, equipment->getArmorStack().Num(), equipment->getArmorStackCap(), extra.succeeded() ? TEXT("accepted (unexpected)") : *extra.failReason.ToString());
    }

    void hitCommand(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = resolvePlayer(world, arguments);
        const TArray<FString> plain = plainArguments(arguments);
        if(player == nullptr || plain.Num() < 1){
            EQUIPMENT_LOG(Warning, world, "Usage: Awaken.Equipment.Hit <damage> [penetration 0..1] [pN]");
            return;
        }
        const float damage = FCString::Atof(*plain[0]);
        const float penetration = plain.Num() > 1 ? FCString::Atof(*plain[1]) : 0.0f;
        // The reference step 4 only: health is NOT changed here. That is Doc R03's pipeline.
        const FS_GearAttackResult result = UBFL_Equipment::applyAttackToGear(player, damage, penetration);
        EQUIPMENT_LOG(Log, world, "Hit %.1f (penetration %.2f): clothes took %.1f, armor took %.1f, %d piece(s) broke, %.1f would reach health", damage, penetration, result.absorbedByClothes, result.absorbedByArmor, result.brokenPieces, result.damageToHealth);
    }

    void wearCommand(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = resolvePlayer(world, arguments);
        UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
        const TArray<FString> plain = plainArguments(arguments);
        if(equipment == nullptr || plain.Num() < 1){
            EQUIPMENT_LOG(Warning, world, "Usage: Awaken.Equipment.Wear <clothesItemId> [pN]");
            return;
        }
        const FS_InventoryResult result = equipment->equipClothes(FName(*plain[0]));
        EQUIPMENT_LOG(Log, world, "Wear %s: %s", *plain[0], result.succeeded() ? TEXT("done") : *result.failReason.ToString());
    }

    // Read only. Runs on any machine: it prints the replicated state this machine holds.
    void dumpCommand(const TArray<FString>& arguments, UWorld* world){
        const AGameStateBase* gameState = world != nullptr ? world->GetGameState() : nullptr;
        if(gameState == nullptr){
            return;
        }
        for(const APlayerState* player : gameState->PlayerArray){
            const UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
            if(equipment == nullptr){
                continue;
            }
            const FS_AggregatedArmorStats stats = equipment->getAggregatedArmorStats();
            const FS_WornClothesState clothes = equipment->getWornClothes();
            EQUIPMENT_LOG(Log, world, "%s: %d piece(s), armor %.1f, durability left %.1f, clothes %s (protection %.1f), warmth %.1f", *player->GetPlayerName(), stats.pieceCount, stats.armorValue, equipment->getTotalRemainingDurability(), clothes.isDefault() ? TEXT("default") : *clothes.itemId.ToString(), equipment->getClothesProtection().protection, equipment->getTotalWarmth());
            for(const FS_EquippedArmorState& piece : equipment->getArmorStack()){
                EQUIPMENT_LOG(Log, world, "   [%d] %s durability %.1f %s", piece.stackIndex, *piece.itemId.ToString(), piece.remainingDurability, *piece.instanceId.ToString().Left(8));
            }
        }
    }

    FAutoConsoleCommandWithWorldAndArgs cmdEquip(TEXT("Awaken.Equipment.Equip"), TEXT("<itemId> [pN]: equip the first copy of that armor in the bag"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&equipCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdUnequip(TEXT("Awaken.Equipment.Unequip"), TEXT("<stackIndex> [pN]: return a worn piece to the bag"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&unequipCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdFill(TEXT("Awaken.Equipment.FillStack"), TEXT("<itemId> [pN]: grant and equip up to the cap, then try one more (the full-stack situation)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&fillStackCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdHit(TEXT("Awaken.Equipment.Hit"), TEXT("<damage> [penetration] [pN]: run the reference armor step on the player's gear (health is not changed)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&hitCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdWear(TEXT("Awaken.Equipment.Wear"), TEXT("<clothesItemId> [pN]: put on clothes from the bag"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&wearCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdDump(TEXT("Awaken.Equipment.Dump"), TEXT(": print every player's worn armor and clothes as this machine sees them (read)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&dumpCommand));
}

#endif
