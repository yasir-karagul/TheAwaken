// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "GameFramework/AsyncAction_ListenForGameplayMessage.h"

#ifdef GAMEPLAYMESSAGERUNTIME_AsyncAction_ListenForGameplayMessage_generated_h
#error "AsyncAction_ListenForGameplayMessage.generated.h already included, missing '#pragma once' in AsyncAction_ListenForGameplayMessage.h"
#endif
#define GAMEPLAYMESSAGERUNTIME_AsyncAction_ListenForGameplayMessage_generated_h

#include "UObject/ObjectMacros.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
class UAsyncAction_ListenForGameplayMessage;
class UObject;
class UScriptStruct;
enum class EGameplayMessageMatch : uint8;
struct FGameplayTag;

// ********** Begin Class UAsyncAction_ListenForGameplayMessage ************************************
#define FID_Unreal_Projects_TheAwaken_Plugins_GameplayMessageRouter_Source_GameplayMessageRuntime_Public_GameFramework_AsyncAction_ListenForGameplayMessage_h_27_RPC_WRAPPERS_NO_PURE_DECLS \
	DECLARE_FUNCTION(execListenForGameplayMessages);


struct Z_Construct_UClass_UAsyncAction_ListenForGameplayMessage_Statics;
	struct Z_Construct_UFunction_UAsyncAction_ListenForGameplayMessage_GetPayload_Statics; \
GAMEPLAYMESSAGERUNTIME_API UClass* Z_Construct_UClass_UAsyncAction_ListenForGameplayMessage(ETypeConstructPhase);

#define FID_Unreal_Projects_TheAwaken_Plugins_GameplayMessageRouter_Source_GameplayMessageRuntime_Public_GameFramework_AsyncAction_ListenForGameplayMessage_h_27_INCLASS_NO_PURE_DECLS \
private: \
	friend struct ::Z_Construct_UClass_UAsyncAction_ListenForGameplayMessage_Statics; \
	friend struct ::Z_Construct_UFunction_UAsyncAction_ListenForGameplayMessage_GetPayload_Statics; \
	friend GAMEPLAYMESSAGERUNTIME_API UClass* ::Z_Construct_UClass_UAsyncAction_ListenForGameplayMessage(ETypeConstructPhase); \
public: \
	DECLARE_CLASS2(UAsyncAction_ListenForGameplayMessage, UCancellableAsyncAction, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/GameplayMessageRuntime"), Z_Construct_UClass_UAsyncAction_ListenForGameplayMessage) \
	DECLARE_SERIALIZER(UAsyncAction_ListenForGameplayMessage)


#define FID_Unreal_Projects_TheAwaken_Plugins_GameplayMessageRouter_Source_GameplayMessageRuntime_Public_GameFramework_AsyncAction_ListenForGameplayMessage_h_27_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	GAMEPLAYMESSAGERUNTIME_API UAsyncAction_ListenForGameplayMessage(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	/** Deleted move- and copy-constructors, should never be used */ \
	UAsyncAction_ListenForGameplayMessage(UAsyncAction_ListenForGameplayMessage&&) = delete; \
	UAsyncAction_ListenForGameplayMessage(const UAsyncAction_ListenForGameplayMessage&) = delete; \
	DECLARE_VTABLE_PTR_HELPER_CTOR(GAMEPLAYMESSAGERUNTIME_API, UAsyncAction_ListenForGameplayMessage); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UAsyncAction_ListenForGameplayMessage); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UAsyncAction_ListenForGameplayMessage) \
	GAMEPLAYMESSAGERUNTIME_API virtual ~UAsyncAction_ListenForGameplayMessage();


#define FID_Unreal_Projects_TheAwaken_Plugins_GameplayMessageRouter_Source_GameplayMessageRuntime_Public_GameFramework_AsyncAction_ListenForGameplayMessage_h_24_PROLOG
#define FID_Unreal_Projects_TheAwaken_Plugins_GameplayMessageRouter_Source_GameplayMessageRuntime_Public_GameFramework_AsyncAction_ListenForGameplayMessage_h_27_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_Unreal_Projects_TheAwaken_Plugins_GameplayMessageRouter_Source_GameplayMessageRuntime_Public_GameFramework_AsyncAction_ListenForGameplayMessage_h_27_RPC_WRAPPERS_NO_PURE_DECLS \
	FID_Unreal_Projects_TheAwaken_Plugins_GameplayMessageRouter_Source_GameplayMessageRuntime_Public_GameFramework_AsyncAction_ListenForGameplayMessage_h_27_INCLASS_NO_PURE_DECLS \
	FID_Unreal_Projects_TheAwaken_Plugins_GameplayMessageRouter_Source_GameplayMessageRuntime_Public_GameFramework_AsyncAction_ListenForGameplayMessage_h_27_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


class UAsyncAction_ListenForGameplayMessage;

// ********** End Class UAsyncAction_ListenForGameplayMessage **************************************

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_Unreal_Projects_TheAwaken_Plugins_GameplayMessageRouter_Source_GameplayMessageRuntime_Public_GameFramework_AsyncAction_ListenForGameplayMessage_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
