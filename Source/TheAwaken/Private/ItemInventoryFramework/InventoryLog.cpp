#include "ItemInventoryFramework/InventoryLog.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogAwakenInventory);

FString InventoryLog::roleName(const UObject* worldContext){
    const UWorld* world = (GEngine != nullptr && worldContext != nullptr) ? GEngine->GetWorldFromContextObject(worldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
    if(world == nullptr){
        return TEXT("NoWorld");
    }
    switch(world->GetNetMode()){
        case NM_Standalone:{
            return TEXT("Offline");
        }
        case NM_Client:{
            return TEXT("Client");
        }
        default:{
            return TEXT("Host");
        }
    }
}
