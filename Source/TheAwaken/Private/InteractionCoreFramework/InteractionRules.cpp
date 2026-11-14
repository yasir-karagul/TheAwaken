// Private/InteractionCoreFramework/InteractionRules.cpp
#include "InteractionCoreFramework/InteractionRules.h"
#include "InteractionCoreFramework/02Tags/T_InteractionCoreFramework.h"

int32 FInteractionRules::pickBestIndex(const TArray<FInteractionFocusCandidate>& candidates){
    if(candidates.IsEmpty()){
        return INDEX_NONE;
    }
    int32 best = 0;
    for(int32 index = 1; index < candidates.Num(); ++index){
        const FInteractionFocusCandidate& challenger = candidates[index];
        const FInteractionFocusCandidate& current = candidates[best];
        const float angleDifference = challenger.angleRadians - current.angleRadians;
        if(angleDifference < -angleEpsilon){
            best = index;
        }else if(FMath::Abs(angleDifference) <= angleEpsilon){
            if(challenger.priority > current.priority){
                best = index;
            }else if(challenger.priority == current.priority && challenger.distance < current.distance){
                best = index;
            }
        }
    }
    return best;
}

bool FInteractionRules::isWithinRange(float distance, float range, float slack){
    return distance <= range + slack;
}

bool FInteractionRules::stateAllows(bool isIncapacitated, bool isInScene, bool isEscape){
    if(isInScene){
        return false;
    }
    if(isIncapacitated && !isEscape){
        return false;
    }
    return true;
}

bool FInteractionRules::handsAllow(bool isCarrying, bool isTagAllowedWhileCarrying){
    return !isCarrying || isTagAllowedWhileCarrying;
}

bool FInteractionRules::lockAllows(E_InteractionLockPolicy policy, bool hasOtherHolder){
    switch(policy){
        case E_InteractionLockPolicy::None:
        case E_InteractionLockPolicy::Shared:
            return true;
        case E_InteractionLockPolicy::Exclusive:
            return !hasOtherHolder;
    }
    return true;
}

bool FInteractionRules::shouldCancelOnDamage(bool isEscapeHold, bool isDue, double now, double endTime){
    if(isEscapeHold){
        return false;
    }
    const bool isDueNow = isDue || now >= endTime;
    return !isDueNow;
}

E_InteractionRequestVerdict FInteractionRules::validate(const FInteractionRequestProbe& probe, FGameplayTag& failReason){
    failReason = FGameplayTag();

    // 1: no prompt existed for this target, so nothing is answered.
    if(probe.targetUsable && !probe.targetUsable()){
        return E_InteractionRequestVerdict::Ignore;
    }
    // 2
    if(probe.stateAllows && !probe.stateAllows()){
        failReason = T_Interaction_FailReason_NotAllowedInState;
        return E_InteractionRequestVerdict::Refuse;
    }
    // 3
    if(probe.inRange && !probe.inRange()){
        failReason = T_Interaction_FailReason_OutOfRange;
        return E_InteractionRequestVerdict::Refuse;
    }
    if(probe.hasLineOfSight && !probe.hasLineOfSight()){
        failReason = T_Interaction_FailReason_NoLineOfSight;
        return E_InteractionRequestVerdict::Refuse;
    }
    // 4
    if(probe.lockAllows && !probe.lockAllows()){
        failReason = T_Interaction_FailReason_InUse;
        return E_InteractionRequestVerdict::Refuse;
    }
    // 5
    if(probe.handsAllow && !probe.handsAllow()){
        failReason = T_Interaction_FailReason_HandsFull;
        return E_InteractionRequestVerdict::Refuse;
    }
    // 6: the object's own tag. A refusal that names no reason still refuses, with the generic one (Doc02 6: never a bare false).
    if(probe.objectAllows){
        FGameplayTag objectReason;
        if(!probe.objectAllows(objectReason)){
            failReason = objectReason.IsValid() ? objectReason : T_Interaction_FailReason_NotAllowedInState.GetTag();
            return E_InteractionRequestVerdict::Refuse;
        }
    }
    return E_InteractionRequestVerdict::Pass;
}
