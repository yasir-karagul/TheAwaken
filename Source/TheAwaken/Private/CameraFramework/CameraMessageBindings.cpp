#include "CameraFramework/CameraMessageBindings.h"

#include "CameraFramework/CameraFramework.h"
#include "CameraFramework/FirstPersonCameraComponent.h"
#include "CameraFramework/ScreenEffectComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "StatsFramework/01Structs/S_StatsFramework.h"
#include "StatsFramework/02Tags/T_StatsFramework.h"
#include "WeatherFramework/01Structs/S_WeatherFramework.h"
#include "WeatherFramework/02Tags/T_WeatherFramework.h"

// =============================================================================================
// STATUS: wired so far: Stats.HPChanged (low health) and Weather.Changed (wetness), plus the low-health start-up snapshot.
// NOT wired (the owning frameworks do not exist yet in the project): Cosmetic.Received, Cinematic.SceneStarted/Ended,
// UI.Screen.Opened/Closed, Survival tier and blood. Those still log one Warning each.
//
// A Gameplay Message listener is typed by the payload struct the sender uses. The senders (Survival B03, Stats R08,
// Cosmetic B21, Weather B14, Cinematic R17, UI B17) were not visible when this module was written, and a listener for a
// struct nobody sends is silently never called. So each source below logs one Warning at start-up instead of pretending.
//
// To wire a source, once its owner exposes the payload struct header (Open items, row 12):
//   1. include the owner's header here,
//   2. replace the matching reportUnwired line with a RegisterListener call like the PATTERN below,
//   3. the adapter turns the payload into (sourceTag, filterTag, value) and calls effects->handleSourceValue(...).
// Nothing else in the module changes.
//
// PATTERN (Cosmetic.Received, assuming the owner calls its payload FS_CosmeticEvent with tag, location, instigator, magnitude):
//
//   // in the .h of a small adapter class, or as a lambda:
//   UGameplayMessageSubsystem& router = UGameplayMessageSubsystem::Get(camera);   // camera must have a valid world here
//   handles.Add(router.RegisterListener<FS_CosmeticEvent>(cosmeticTag, camera, &UFirstPersonCameraComponent::onCosmeticMessage));
//
//   // with, on the component (C++ only):
//   void UFirstPersonCameraComponent::onCosmeticMessage(FGameplayTag channel, const FS_CosmeticEvent& payload){
//       onCosmeticEvent(payload.eventTag, payload.location, payload.magnitude);
//   }
//
// Adapter rules from step 10 item 2:
//   Survival.Meter.TierChanged  -> value = new tier index, filter = the meter's Survival.Meter.* tag; skip meters Peaceful has switched off
//   Cosmetic.Received           -> value = magnitude, filter = the event tag, eventLocation = the event location (maxPawnDistance test)
//   Stats.HPChanged             -> value = 1 while Stats.State.LowHealth is on the Ability System Component, else 0; filter = none
//   Weather.Changed             -> value = weather type, filter = none
//   UI.Screen.Opened / Closed   -> for the bag screen's UI.Screen.* tag only: Opened gives value 1 for source UI.Screen.Opened, Closed gives value 0 for the SAME source tag
//   (blood amount message)      -> value = blood amount, filter = none (message name still open, Open items row 4)
// =============================================================================================

namespace{
    // Stats.State.LowHealth on the local pawn's Ability System Component: 1 while on, else 0 (step 10 item 2, R08 3.5).
    float readLowHealthValue(const UScreenEffectComponent* effects){
        const APlayerController* controller = effects != nullptr ? Cast<APlayerController>(effects->GetOwner()) : nullptr;
        const IAbilitySystemInterface* abilityOwner = controller != nullptr ? Cast<IAbilitySystemInterface>(controller->GetPawn()) : nullptr;
        const UAbilitySystemComponent* abilitySystem = abilityOwner != nullptr ? abilityOwner->GetAbilitySystemComponent() : nullptr;
        if(abilitySystem == nullptr){
            return -1.0f;   // the state is not valid yet (late replication on a client)
        }
        return abilitySystem->HasMatchingGameplayTag(T_Stats_State_LowHealth) ? 1.0f : 0.0f;
    }

    void reportUnwired(const TCHAR* listener, FName tagName){
        const FGameplayTag tag = CameraFramework::lookupExternalTag(tagName);
        if(tag.IsValid()){
            UE_LOG(LogAwakenCamera, Warning, TEXT("Local: %s: tag %s exists but no payload adapter is wired in CameraMessageBindings.cpp yet"), listener, *tagName.ToString());
        }
    }
}

void CameraMessageBindings::bindFirstPersonCamera(UFirstPersonCameraComponent* camera, TArray<FGameplayMessageListenerHandle>& handles){
    if(camera == nullptr){
        return;
    }
    // Step 7 item 1: one Cosmetic.Received listener, registered only while listening. Calls camera->onCosmeticEvent(tag, location, magnitude).
    reportUnwired(TEXT("shake"), TEXT("Cosmetic.Received"));
    // Step 12: the scene takes the view and gives it back (R17). Calls camera->onSceneStarted() and camera->onSceneEnded().
    reportUnwired(TEXT("scene start"), TEXT("Cinematic.SceneStarted"));
    reportUnwired(TEXT("scene end"), TEXT("Cinematic.SceneEnded"));
    // Step 12: the bag screen really opened or closed (B17, PROPOSED names). Filter to the bag screen's tag.
    // Calls camera->onBagScreenOpened() and camera->onBagScreenClosed().
    reportUnwired(TEXT("bag opened"), TEXT("UI.Screen.Opened"));
    reportUnwired(TEXT("bag closed"), TEXT("UI.Screen.Closed"));
}

void CameraMessageBindings::bindScreenEffects(UScreenEffectComponent* effects, const TArray<FGameplayTag>& sourceTags, TArray<FGameplayMessageListenerHandle>& handles){
    if(effects == nullptr){
        return;
    }
    UGameplayMessageSubsystem& router = UGameplayMessageSubsystem::Get(effects);
    const TWeakObjectPtr<UScreenEffectComponent> weakEffects(effects);
    for(const FGameplayTag& sourceTag : sourceTags){
        if(sourceTag == T_Stats_HPChanged.GetTag()){
            // The message is only the trigger; the Stats.State.LowHealth tag is the truth (Open items row 5).
            // R08 must grant or remove the tag before it raises the message, or this row is one change late.
            handles.Add(router.RegisterListener<FS_HPChangedMessage>(sourceTag, [weakEffects, sourceTag](FGameplayTag, const FS_HPChangedMessage&){
                if(UScreenEffectComponent* target = weakEffects.Get()){
                    const float value = readLowHealthValue(target);
                    if(value >= 0.0f){
                        target->handleSourceValue(sourceTag, FGameplayTag(), value);
                    }
                }
            }));
        }else if(sourceTag == T_Weather_Changed.GetTag()){
            // Step 11 wetness row: the curve maps the weather type index (E_WeatherType) to droplets.
            handles.Add(router.RegisterListener<FS_WeatherChangedMessage>(sourceTag, [weakEffects, sourceTag](FGameplayTag, const FS_WeatherChangedMessage& payload){
                if(UScreenEffectComponent* target = weakEffects.Get()){
                    target->handleSourceValue(sourceTag, FGameplayTag(), static_cast<float>(static_cast<uint8>(payload.newType)));
                }
            }));
        }else{
            // A row exists for this source, so the message is meant to work. Every source needs one adapter function (step 10 item 2);
            // a new row on a source that already has an adapter is a data change only.
            UE_LOG(LogAwakenCamera, Warning, TEXT("Local: mapping source %s has no payload adapter wired in CameraMessageBindings.cpp yet; rows on it never fire from messages (handleSourceValue still works from code and dev commands)"), *sourceTag.ToString());
        }
    }
}

bool CameraMessageBindings::applyStartupSnapshot(UScreenEffectComponent* effects){
    if(effects == nullptr){
        return true;
    }
    // Step 10 item 6. Wired so far: the low-health tag. Still waiting for the owners (Survival has no blood or tier readers yet): blood amount, meter tiers.
    const float lowHealth = readLowHealthValue(effects);
    if(lowHealth < 0.0f){
        return false;   // the Ability System Component is not valid yet, the retry timer runs again
    }
    effects->handleSourceValue(T_Stats_HPChanged.GetTag(), FGameplayTag(), lowHealth, nullptr, true);
    UE_LOG(LogAwakenCamera, Warning, TEXT("Local: start-up snapshot applied for low health only; blood amount and meter tiers have no readers wired yet"));
    return true;
}
