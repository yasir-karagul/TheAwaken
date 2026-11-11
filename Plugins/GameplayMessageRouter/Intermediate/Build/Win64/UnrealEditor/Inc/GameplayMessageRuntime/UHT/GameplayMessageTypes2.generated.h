// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "GameFramework/GameplayMessageTypes2.h"

#ifdef GAMEPLAYMESSAGERUNTIME_GameplayMessageTypes2_generated_h
#error "GameplayMessageTypes2.generated.h already included, missing '#pragma once' in GameplayMessageTypes2.h"
#endif
#define GAMEPLAYMESSAGERUNTIME_GameplayMessageTypes2_generated_h

#include "UObject/ObjectMacros.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "Templates/IsUEnumClass.h"
#include "Templates/NoDestroy.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_Unreal_Projects_TheAwaken_Plugins_GameplayMessageRouter_Source_GameplayMessageRuntime_Public_GameFramework_GameplayMessageTypes2_h

// ********** Begin Enum EGameplayMessageMatch *****************************************************
#define FOREACH_ENUM_EGAMEPLAYMESSAGEMATCH(op) \
	op(EGameplayMessageMatch::ExactMatch) \
	op(EGameplayMessageMatch::PartialMatch) 

enum class EGameplayMessageMatch : uint8;
template<> struct TIsUEnumClass<EGameplayMessageMatch> { enum { Value = true }; };
template<> UE_NODEBUG GAMEPLAYMESSAGERUNTIME_NON_ATTRIBUTED_API UEnum* StaticEnum<EGameplayMessageMatch>();
// ********** End Enum EGameplayMessageMatch *******************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
