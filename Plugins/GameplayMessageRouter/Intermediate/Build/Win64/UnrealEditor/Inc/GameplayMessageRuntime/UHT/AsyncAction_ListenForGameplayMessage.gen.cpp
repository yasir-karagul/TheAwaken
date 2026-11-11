// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "GameFramework/AsyncAction_ListenForGameplayMessage.h"
#include "GameplayTagContainer.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeAsyncAction_ListenForGameplayMessage() {}

// ********** Begin Cross Module References ********************************************************
ENGINE_API UClass* Z_Construct_UClass_UCancellableAsyncAction(ETypeConstructPhase);
GAMEPLAYTAGS_API UScriptStruct* Z_Construct_UScriptStruct_FGameplayTag(ETypeConstructPhase);
COREUOBJECT_API UClass* Z_Construct_UClass_UObject(ETypeConstructPhase);
COREUOBJECT_API UClass* Z_Construct_UClass_UScriptStruct(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GameplayMessageRuntime(ETypeConstructPhase);
GAMEPLAYMESSAGERUNTIME_API UClass* Z_Construct_UClass_UAsyncAction_ListenForGameplayMessage(ETypeConstructPhase);
GAMEPLAYMESSAGERUNTIME_API UFunction* Z_Construct_UDelegateFunction_GameplayMessageRuntime_AsyncGameplayMessageDelegate__DelegateSignature(ETypeConstructPhase);
GAMEPLAYMESSAGERUNTIME_API UEnum* Z_Construct_UEnum_GameplayMessageRuntime_EGameplayMessageMatch(ETypeConstructPhase);
GAMEPLAYMESSAGERUNTIME_API UClass* Z_Construct_UClass_UAsyncAction_ListenForGameplayMessage(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Delegate FAsyncGameplayMessageDelegate *****************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UDelegateFunction_GameplayMessageRuntime_AsyncGameplayMessageDelegate__DelegateSignature_Statics
struct UHT_STATICS
{
	struct _Script_GameplayMessageRuntime_eventAsyncGameplayMessageDelegate_Parms
	{
		UAsyncAction_ListenForGameplayMessage* ProxyObject;
		FGameplayTag ActualChannel;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n * Proxy object pin will be hidden in K2Node_GameplayMessageAsyncAction. Is used to get a reference to the object triggering the delegate for the follow up call of 'GetPayload'.\n *\n * @param ActualChannel\x09\x09The actual message channel that we received Payload from (will always start with Channel, but may be more specific if partial matches were enabled)\n */" },
#endif
		{ "ModuleRelativePath", "Public/GameFramework/AsyncAction_ListenForGameplayMessage.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Proxy object pin will be hidden in K2Node_GameplayMessageAsyncAction. Is used to get a reference to the object triggering the delegate for the follow up call of 'GetPayload'.\n\n@param ActualChannel                The actual message channel that we received Payload from (will always start with Channel, but may be more specific if partial matches were enabled)" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Delegate FAsyncGameplayMessageDelegate constinit property declarations *********
	static const UECodeGen_Private::FObjectPropertyParams NewProp_ProxyObject;
	static const UECodeGen_Private::FStructPropertyParams NewProp_ActualChannel;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Delegate FAsyncGameplayMessageDelegate constinit property declarations ***********
	static const UECodeGen_Private::FDelegateFunctionParams FuncParams;
};

// ********** Begin Delegate FAsyncGameplayMessageDelegate Property Definitions ********************
const UECodeGen_Private::FObjectPropertyParams UHT_STATICS::NewProp_ProxyObject = { "ProxyObject", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Object, nullptr, nullptr, 1, STRUCT_OFFSET(_Script_GameplayMessageRuntime_eventAsyncGameplayMessageDelegate_Parms, ProxyObject), Z_Construct_UClass_UAsyncAction_ListenForGameplayMessage, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_ActualChannel = { "ActualChannel", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(_Script_GameplayMessageRuntime_eventAsyncGameplayMessageDelegate_Parms, ActualChannel), Z_Construct_UScriptStruct_FGameplayTag, METADATA_PARAMS(0, nullptr) }; // 63c9638e64e309ea70a1c1e4688171f6669f0b1b
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ProxyObject,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ActualChannel,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Delegate FAsyncGameplayMessageDelegate Property Definitions **********************
const UECodeGen_Private::FDelegateFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UPackage__Script_GameplayMessageRuntime, nullptr, "AsyncGameplayMessageDelegate__DelegateSignature", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::_Script_GameplayMessageRuntime_eventAsyncGameplayMessageDelegate_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x00130000, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::_Script_GameplayMessageRuntime_eventAsyncGameplayMessageDelegate_Parms) < MAX_uint16);
UFunction* Z_Construct_UDelegateFunction_GameplayMessageRuntime_AsyncGameplayMessageDelegate__DelegateSignature(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUDelegateFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
// ********** End Delegate FAsyncGameplayMessageDelegate *******************************************

// ********** Begin Class UAsyncAction_ListenForGameplayMessage Function GetPayload ****************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UAsyncAction_ListenForGameplayMessage_GetPayload_Statics
struct UHT_STATICS
{
	struct AsyncAction_ListenForGameplayMessage_eventGetPayload_Parms
	{
		int32 OutPayload;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Messaging" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n\x09 * Attempt to copy the payload received from the broadcasted gameplay message into the specified wildcard.\n\x09 * The wildcard's type must match the type from the received message.\n\x09 *\n\x09 * @param OutPayload\x09The wildcard reference the payload should be copied into\n\x09 * @return\x09\x09\x09\x09If the copy was a success\n\x09 */" },
#endif
		{ "CustomStructureParam", "OutPayload" },
		{ "CustomThunk", "true" },
		{ "ModuleRelativePath", "Public/GameFramework/AsyncAction_ListenForGameplayMessage.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Attempt to copy the payload received from the broadcasted gameplay message into the specified wildcard.\nThe wildcard's type must match the type from the received message.\n\n@param OutPayload    The wildcard reference the payload should be copied into\n@return                              If the copy was a success" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Function GetPayload constinit property declarations ****************************
	static const UECodeGen_Private::FIntPropertyParams NewProp_OutPayload;
	static void NewProp_ReturnValue_SetBit(void* Obj)
	{
		((AsyncAction_ListenForGameplayMessage_eventGetPayload_Parms*)Obj)->ReturnValue = 1;
	}
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetPayload constinit property declarations ******************************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetPayload Property Definitions ***************************************
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_OutPayload = { "OutPayload", nullptr, (EPropertyFlags)0x0010000008000180, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(AsyncAction_ListenForGameplayMessage_eventGetPayload_Parms, OutPayload), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBoolPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, nullptr, nullptr, 1, sizeof(bool), sizeof(AsyncAction_ListenForGameplayMessage_eventGetPayload_Parms), &UHT_STATICS::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_OutPayload,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetPayload Property Definitions *****************************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UAsyncAction_ListenForGameplayMessage, nullptr, "GetPayload", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::AsyncAction_ListenForGameplayMessage_eventGetPayload_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04420403, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::AsyncAction_ListenForGameplayMessage_eventGetPayload_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UAsyncAction_ListenForGameplayMessage_GetPayload(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
// ********** End Class UAsyncAction_ListenForGameplayMessage Function GetPayload ******************

// ********** Begin Class UAsyncAction_ListenForGameplayMessage Function ListenForGameplayMessages *
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UAsyncAction_ListenForGameplayMessage_ListenForGameplayMessages_Statics
struct UHT_STATICS
{
	struct AsyncAction_ListenForGameplayMessage_eventListenForGameplayMessages_Parms
	{
		UObject* WorldContextObject;
		FGameplayTag Channel;
		UScriptStruct* PayloadType;
		EGameplayMessageMatch MatchType;
		UAsyncAction_ListenForGameplayMessage* ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintInternalUseOnly", "true" },
		{ "Category", "Messaging" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n\x09 * Asynchronously waits for a gameplay message to be broadcast on the specified channel.\n\x09 *\n\x09 * @param Channel\x09\x09\x09The message channel to listen for\n\x09 * @param PayloadType\x09\x09The kind of message structure to use (this must match the same type that the sender is broadcasting)\n\x09 * @param MatchType\x09\x09\x09The rule used for matching the channel with broadcasted messages\n\x09 */" },
#endif
		{ "CPP_Default_MatchType", "ExactMatch" },
		{ "ModuleRelativePath", "Public/GameFramework/AsyncAction_ListenForGameplayMessage.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Asynchronously waits for a gameplay message to be broadcast on the specified channel.\n\n@param Channel                       The message channel to listen for\n@param PayloadType           The kind of message structure to use (this must match the same type that the sender is broadcasting)\n@param MatchType                     The rule used for matching the channel with broadcasted messages" },
#endif
		{ "WorldContext", "WorldContextObject" },
	};
#endif // WITH_METADATA

// ********** Begin Function ListenForGameplayMessages constinit property declarations *************
	static const UECodeGen_Private::FObjectPropertyParams NewProp_WorldContextObject;
	static const UECodeGen_Private::FStructPropertyParams NewProp_Channel;
	static const UECodeGen_Private::FObjectPropertyParams NewProp_PayloadType;
	static const UECodeGen_Private::FBytePropertyParams NewProp_MatchType_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_MatchType;
	static const UECodeGen_Private::FObjectPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function ListenForGameplayMessages constinit property declarations ***************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function ListenForGameplayMessages Property Definitions ************************
const UECodeGen_Private::FObjectPropertyParams UHT_STATICS::NewProp_WorldContextObject = { "WorldContextObject", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Object, nullptr, nullptr, 1, STRUCT_OFFSET(AsyncAction_ListenForGameplayMessage_eventListenForGameplayMessages_Parms, WorldContextObject), Z_Construct_UClass_UObject, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_Channel = { "Channel", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(AsyncAction_ListenForGameplayMessage_eventListenForGameplayMessages_Parms, Channel), Z_Construct_UScriptStruct_FGameplayTag, METADATA_PARAMS(0, nullptr) }; // 63c9638e64e309ea70a1c1e4688171f6669f0b1b
const UECodeGen_Private::FObjectPropertyParams UHT_STATICS::NewProp_PayloadType = { "PayloadType", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Object, nullptr, nullptr, 1, STRUCT_OFFSET(AsyncAction_ListenForGameplayMessage_eventListenForGameplayMessages_Parms, PayloadType), Z_Construct_UClass_UScriptStruct, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FBytePropertyParams UHT_STATICS::NewProp_MatchType_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams UHT_STATICS::NewProp_MatchType = { "MatchType", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Enum, nullptr, nullptr, 1, STRUCT_OFFSET(AsyncAction_ListenForGameplayMessage_eventListenForGameplayMessages_Parms, MatchType), Z_Construct_UEnum_GameplayMessageRuntime_EGameplayMessageMatch, METADATA_PARAMS(0, nullptr) }; // 2da7c410b482d96f2852589a61ec24c949a57017
const UECodeGen_Private::FObjectPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Object, nullptr, nullptr, 1, STRUCT_OFFSET(AsyncAction_ListenForGameplayMessage_eventListenForGameplayMessages_Parms, ReturnValue), Z_Construct_UClass_UAsyncAction_ListenForGameplayMessage, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_WorldContextObject,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Channel,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_PayloadType,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_MatchType_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_MatchType,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function ListenForGameplayMessages Property Definitions **************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UAsyncAction_ListenForGameplayMessage, nullptr, "ListenForGameplayMessages", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::AsyncAction_ListenForGameplayMessage_eventListenForGameplayMessages_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022403, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::AsyncAction_ListenForGameplayMessage_eventListenForGameplayMessages_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UAsyncAction_ListenForGameplayMessage_ListenForGameplayMessages(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(UAsyncAction_ListenForGameplayMessage::execListenForGameplayMessages)
{
	P_GET_OBJECT(UObject,Z_Param_WorldContextObject);
	P_GET_STRUCT(FGameplayTag,Z_Param_Channel);
	P_GET_OBJECT(UScriptStruct,Z_Param_PayloadType);
	P_GET_ENUM(EGameplayMessageMatch,Z_Param_MatchType);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(UAsyncAction_ListenForGameplayMessage**)Z_Param__Result=UAsyncAction_ListenForGameplayMessage::ListenForGameplayMessages(Z_Param_WorldContextObject,Z_Param_Channel,Z_Param_PayloadType,EGameplayMessageMatch(Z_Param_MatchType));
	P_NATIVE_END;
}
// ********** End Class UAsyncAction_ListenForGameplayMessage Function ListenForGameplayMessages ***

// ********** Begin Class UAsyncAction_ListenForGameplayMessage ************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UAsyncAction_ListenForGameplayMessage_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
		{ "HasDedicatedAsyncNode", "" },
		{ "IncludePath", "GameFramework/AsyncAction_ListenForGameplayMessage.h" },
		{ "ModuleRelativePath", "Public/GameFramework/AsyncAction_ListenForGameplayMessage.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_OnMessageReceived_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** Called when a message is broadcast on the specified channel. Use GetPayload() to request the message payload. */" },
#endif
		{ "ModuleRelativePath", "Public/GameFramework/AsyncAction_ListenForGameplayMessage.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Called when a message is broadcast on the specified channel. Use GetPayload() to request the message payload." },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UAsyncAction_ListenForGameplayMessage constinit property declarations ****
	static const UECodeGen_Private::FMulticastDelegatePropertyParams NewProp_OnMessageReceived;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Class UAsyncAction_ListenForGameplayMessage constinit property declarations ******
	static constexpr UE::CodeGen::FClassNativeFunction Funcs[] = {
		{ .NameUTF8 = UTF8TEXT("GetPayload"), .Pointer = &UAsyncAction_ListenForGameplayMessage::execGetPayload },
		{ .NameUTF8 = UTF8TEXT("ListenForGameplayMessages"), .Pointer = &UAsyncAction_ListenForGameplayMessage::execListenForGameplayMessages },
	};
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FClassFunctionLinkInfo FuncInfo[] = {
		{ &Z_Construct_UFunction_UAsyncAction_ListenForGameplayMessage_GetPayload, "GetPayload" }, // e0b9fe4234319e167edd78912736d7e3443cefee
		{ &Z_Construct_UFunction_UAsyncAction_ListenForGameplayMessage_ListenForGameplayMessages, "ListenForGameplayMessages" }, // 3bef8f2b8eeb0d5bb24b1f997412dbc9734b4964
	};
	static_assert(UE_ARRAY_COUNT(FuncInfo) < 2048);
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UAsyncAction_ListenForGameplayMessage>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS

// ********** Begin Class UAsyncAction_ListenForGameplayMessage Property Definitions ***************
const UECodeGen_Private::FMulticastDelegatePropertyParams UHT_STATICS::NewProp_OnMessageReceived = { "OnMessageReceived", nullptr, (EPropertyFlags)0x0010000010080000, UECodeGen_Private::EPropertyGenFlags::InlineMulticastDelegate, nullptr, nullptr, 1, STRUCT_OFFSET(UAsyncAction_ListenForGameplayMessage, OnMessageReceived), Z_Construct_UDelegateFunction_GameplayMessageRuntime_AsyncGameplayMessageDelegate__DelegateSignature, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_OnMessageReceived_MetaData), NewProp_OnMessageReceived_MetaData) }; // 03342226f37a05e0b910650932c2b9ca84c94b90
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_OnMessageReceived,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Class UAsyncAction_ListenForGameplayMessage Property Definitions *****************
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UCancellableAsyncAction,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GameplayMessageRuntime,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UAsyncAction_ListenForGameplayMessage,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	FuncInfo,
	UHT_STATICS::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	UE_ARRAY_COUNT(FuncInfo),
	UE_ARRAY_COUNT(UHT_STATICS::PropPointers),
	0,
	0x008800A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static void UAsyncAction_ListenForGameplayMessage_StaticRegisterNativesUAsyncAction_ListenForGameplayMessage()
{
	UClass* Class = UAsyncAction_ListenForGameplayMessage::StaticClass();
	FNativeFunctionRegistrar::RegisterFunctions(Class, 		MakeConstArrayView(UHT_STATICS::Funcs));
}
FClassRegistrationInfo Z_Registration_Info_UClass_UAsyncAction_ListenForGameplayMessage;
UClass* Z_Construct_UClass_UAsyncAction_ListenForGameplayMessage(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UAsyncAction_ListenForGameplayMessage;
		if (!Z_Registration_Info_UClass_UAsyncAction_ListenForGameplayMessage.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("AsyncAction_ListenForGameplayMessage"),
				Z_Registration_Info_UClass_UAsyncAction_ListenForGameplayMessage.InnerSingleton,
				UAsyncAction_ListenForGameplayMessage_StaticRegisterNativesUAsyncAction_ListenForGameplayMessage,
				DataSizeOf<TClass>(),
				alignof(TClass),
				TClass::StaticClassFlags,
				TClass::StaticClassCastFlags(),
				TClass::StaticConfigName(),
				(UClass::ClassConstructorType)InternalConstructor<TClass>,
				(UClass::ClassVTableHelperCtorCallerType)InternalVTableHelperCtorCaller<TClass>,
				UOBJECT_CPPCLASS_STATICFUNCTIONS_FORCLASS(TClass),
				&TClass::Super::StaticClass,
				&TClass::WithinClass::StaticClass
			);
		}
		return Z_Registration_Info_UClass_UAsyncAction_ListenForGameplayMessage.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UAsyncAction_ListenForGameplayMessage.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UAsyncAction_ListenForGameplayMessage.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UAsyncAction_ListenForGameplayMessage.OuterSingleton;
}
#undef UHT_STATICS
UAsyncAction_ListenForGameplayMessage::UAsyncAction_ListenForGameplayMessage(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UAsyncAction_ListenForGameplayMessage);
UAsyncAction_ListenForGameplayMessage::~UAsyncAction_ListenForGameplayMessage() {}
// ********** End Class UAsyncAction_ListenForGameplayMessage **************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Unreal_Projects_TheAwaken_Plugins_GameplayMessageRouter_Source_GameplayMessageRuntime_Public_GameFramework_AsyncAction_ListenForGameplayMessage_h__Script_GameplayMessageRuntime_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UAsyncAction_ListenForGameplayMessage, TEXT("UAsyncAction_ListenForGameplayMessage"), &Z_Registration_Info_UClass_UAsyncAction_ListenForGameplayMessage, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UAsyncAction_ListenForGameplayMessage), 3869598996U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Unreal_Projects_TheAwaken_Plugins_GameplayMessageRouter_Source_GameplayMessageRuntime_Public_GameFramework_AsyncAction_ListenForGameplayMessage_h__Script_GameplayMessageRuntime_a58c048a10464d1584016111d367c668987e1329{
	TEXT("/Script/GameplayMessageRuntime"),
	UHT_STATICS::ClassInfo, UE_ARRAY_COUNT(UHT_STATICS::ClassInfo),
	nullptr, 0,
	nullptr, 0,
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
