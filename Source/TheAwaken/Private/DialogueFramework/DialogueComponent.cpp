// Private/DialogueFramework/DialogueComponent.cpp (TEMPORARY)
#include "DialogueFramework/DialogueComponent.h"

UDialogueComponent::UDialogueComponent(){
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}
