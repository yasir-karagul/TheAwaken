#include "ItemInventoryFramework/UInventorySettings.h"

UInventorySettings::UInventorySettings(){
    CategoryName = TEXT("Game");
    FS_ItemStack axe;
    axe.itemId = TEXT("StarterAxe");
    FS_ItemStack map;
    map.itemId = TEXT("Map");
    startingKit = {axe, map};
}
