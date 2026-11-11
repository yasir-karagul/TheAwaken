// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameplayTagContainer.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGameplayMessageSubsystem() {}

// ********** Begin Cross Module References ********************************************************
ENGINE_API UClass* Z_Construct_UClass_UGameInstanceSubsystem(ETypeConstructPhase);
GAMEPLAYTAGS_API UScriptStruct* Z_Construct_UScriptStruct_FGameplayTag(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GameplayMessageRuntime(ETypeConstructPhase);
GAMEPLAYMESSAGERUNTIME_API UScriptStruct* Z_Construct_UScriptStruct_FGameplayMessageListenerData(ETypeConstructPhase);
GAMEPLAYMESSAGERUNTIME_API UScriptStruct* Z_Construct_UScriptStruct_FGameplayMessageListenerHandle(ETypeConstructPhase);
GAMEPLAYMESSAGERUNTIME_API UClass* Z_Construct_UClass_UGameplayMessageSubsystem(ETypeConstructPhase);
GAMEPLAYMESSAGERUNTIME_API UClass* Z_Construct_UClass_UGameplayMessageSubsystem(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin ScriptStruct FGameplayMessageListenerHandle ************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UScriptStruct_FGameplayMessageListenerHandle_Statics
struct UHT_STATICS
{
	static inline consteval int32 GetStructSize() { return DataSizeOf<FGameplayMessageListenerHandle>(); }
	static inline consteval int16 GetStructAlignment() { return alignof(FGameplayMessageListenerHandle); }
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n * An opaque handle that can be used to remove a previously registered message listener\n * @see UGameplayMessageSubsystem::RegisterListener and UGameplayMessageSubsystem::UnregisterListener\n */" },
#endif
		{ "ModuleRelativePath", "Public/GameFramework/GameplayMessageSubsystem.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "An opaque handle that can be used to remove a previously registered message listener\n@see UGameplayMessageSubsystem::RegisterListener and UGameplayMessageSubsystem::UnregisterListener" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Subsystem_MetaData[] = {
		{ "ModuleRelativePath", "Public/GameFramework/GameplayMessageSubsystem.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Channel_MetaData[] = {
		{ "ModuleRelativePath", "Public/GameFramework/GameplayMessageSubsystem.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ID_MetaData[] = {
		{ "ModuleRelativePath", "Public/GameFramework/GameplayMessageSubsystem.h" },
	};
#endif // WITH_METADATA

// ********** Begin ScriptStruct FGameplayMessageListenerHandle constinit property declarations ****
	static const UECodeGen_Private::FWeakObjectPropertyParams NewProp_Subsystem;
	static const UECodeGen_Private::FStructPropertyParams NewProp_Channel;
	static const UECodeGen_Private::FIntPropertyParams NewProp_ID;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End ScriptStruct FGameplayMessageListenerHandle constinit property declarations ******
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FGameplayMessageListenerHandle>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
}; // struct UHT_STATICS

// ********** Begin ScriptStruct FGameplayMessageListenerHandle Property Definitions ***************
const UECodeGen_Private::FWeakObjectPropertyParams UHT_STATICS::NewProp_Subsystem = { "Subsystem", nullptr, (EPropertyFlags)0x0044000000002000, UECodeGen_Private::EPropertyGenFlags::WeakObject, nullptr, nullptr, 1, STRUCT_OFFSET(FGameplayMessageListenerHandle, Subsystem), Z_Construct_UClass_UGameplayMessageSubsystem, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Subsystem_MetaData), NewProp_Subsystem_MetaData) };
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_Channel = { "Channel", nullptr, (EPropertyFlags)0x0040000000002000, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(FGameplayMessageListenerHandle, Channel), Z_Construct_UScriptStruct_FGameplayTag, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Channel_MetaData), NewProp_Channel_MetaData) }; // 63c9638e64e309ea70a1c1e4688171f6669f0b1b
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_ID = { "ID", nullptr, (EPropertyFlags)0x0040000000002000, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(FGameplayMessageListenerHandle, ID), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ID_MetaData), NewProp_ID_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Subsystem,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Channel,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ID,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End ScriptStruct FGameplayMessageListenerHandle Property Definitions *****************
const UECodeGen_Private::FStructParams UHT_STATICS::StructParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GameplayMessageRuntime,
	nullptr,
	&NewStructOps,
	"GameplayMessageListenerHandle",
	UHT_STATICS::PropPointers,
	UE_ARRAY_COUNT(UHT_STATICS::PropPointers),
	DataSizeOf<FGameplayMessageListenerHandle>(),
	alignof(FGameplayMessageListenerHandle),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000001),
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_FGameplayMessageListenerHandle;
UScriptStruct* Z_Construct_UScriptStruct_FGameplayMessageListenerHandle(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!Z_Registration_Info_UScriptStruct_FGameplayMessageListenerHandle.OuterSingleton)
		{
			Z_Registration_Info_UScriptStruct_FGameplayMessageListenerHandle.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FGameplayMessageListenerHandle, (UObject*)Z_Construct_UPackage__Script_GameplayMessageRuntime(ETypeConstructPhase::Outer), TEXT("GameplayMessageListenerHandle"));
		}
		return Z_Registration_Info_UScriptStruct_FGameplayMessageListenerHandle.OuterSingleton;
	}
	if (!Z_Registration_Info_UScriptStruct_FGameplayMessageListenerHandle.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_FGameplayMessageListenerHandle.InnerSingleton, UHT_STATICS::StructParams);
	}
	return CastChecked<UScriptStruct>(Z_Registration_Info_UScriptStruct_FGameplayMessageListenerHandle.InnerSingleton);
}
#undef UHT_STATICS
// ********** End ScriptStruct FGameplayMessageListenerHandle **************************************

// ********** Begin ScriptStruct FGameplayMessageListenerData **************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UScriptStruct_FGameplayMessageListenerData_Statics
struct UHT_STATICS
{
	static inline consteval int32 GetStructSize() { return DataSizeOf<FGameplayMessageListenerData>(); }
	static inline consteval int16 GetStructAlignment() { return alignof(FGameplayMessageListenerData); }
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \n * Entry information for a single registered listener\n */" },
#endif
		{ "ModuleRelativePath", "Public/GameFramework/GameplayMessageSubsystem.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Entry information for a single registered listener" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin ScriptStruct FGameplayMessageListenerData constinit property declarations ******
// ********** End ScriptStruct FGameplayMessageListenerData constinit property declarations ********
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FGameplayMessageListenerData>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
}; // struct UHT_STATICS
const UECodeGen_Private::FStructParams UHT_STATICS::StructParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GameplayMessageRuntime,
	nullptr,
	&NewStructOps,
	"GameplayMessageListenerData",
	nullptr,
	0,
	DataSizeOf<FGameplayMessageListenerData>(),
	alignof(FGameplayMessageListenerData),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000001),
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_FGameplayMessageListenerData;
UScriptStruct* Z_Construct_UScriptStruct_FGameplayMessageListenerData(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!Z_Registration_Info_UScriptStruct_FGameplayMessageListenerData.OuterSingleton)
		{
			Z_Registration_Info_UScriptStruct_FGameplayMessageListenerData.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FGameplayMessageListenerData, (UObject*)Z_Construct_UPackage__Script_GameplayMessageRuntime(ETypeConstructPhase::Outer), TEXT("GameplayMessageListenerData"));
		}
		return Z_Registration_Info_UScriptStruct_FGameplayMessageListenerData.OuterSingleton;
	}
	if (!Z_Registration_Info_UScriptStruct_FGameplayMessageListenerData.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_FGameplayMessageListenerData.InnerSingleton, UHT_STATICS::StructParams);
	}
	return CastChecked<UScriptStruct>(Z_Registration_Info_UScriptStruct_FGameplayMessageListenerData.InnerSingleton);
}
#undef UHT_STATICS
// ********** End ScriptStruct FGameplayMessageListenerData ****************************************

// ********** Begin Class UGameplayMessageSubsystem Function K2_BroadcastMessage *******************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_UGameplayMessageSubsystem_K2_BroadcastMessage_Statics
struct UHT_STATICS
{
	struct GameplayMessageSubsystem_eventK2_BroadcastMessage_Parms
	{
		FGameplayTag Channel;
		int32 Message;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "AllowAbstract", "false" },
		{ "Category", "Messaging" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n\x09 * Broadcast a message on the specified channel\n\x09 *\n\x09 * @param Channel\x09\x09\x09The message channel to broadcast on\n\x09 * @param Message\x09\x09\x09The message to send (must be the same type of UScriptStruct expected by the listeners for this channel, otherwise an error will be logged)\n\x09 */" },
#endif
		{ "CustomStructureParam", "Message" },
		{ "CustomThunk", "true" },
		{ "DisplayName", "Broadcast Message" },
		{ "ModuleRelativePath", "Public/GameFramework/GameplayMessageSubsystem.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Broadcast a message on the specified channel\n\n@param Channel                       The message channel to broadcast on\n@param Message                       The message to send (must be the same type of UScriptStruct expected by the listeners for this channel, otherwise an error will be logged)" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Message_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function K2_BroadcastMessage constinit property declarations *******************
	static const UECodeGen_Private::FStructPropertyParams NewProp_Channel;
	static const UECodeGen_Private::FIntPropertyParams NewProp_Message;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function K2_BroadcastMessage constinit property declarations *********************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function K2_BroadcastMessage Property Definitions ******************************
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_Channel = { "Channel", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Struct, nullptr, nullptr, 1, STRUCT_OFFSET(GameplayMessageSubsystem_eventK2_BroadcastMessage_Parms, Channel), Z_Construct_UScriptStruct_FGameplayTag, METADATA_PARAMS(0, nullptr) }; // 63c9638e64e309ea70a1c1e4688171f6669f0b1b
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_Message = { "Message", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(GameplayMessageSubsystem_eventK2_BroadcastMessage_Parms, Message), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Message_MetaData), NewProp_Message_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Channel,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Message,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function K2_BroadcastMessage Property Definitions ********************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_UGameplayMessageSubsystem, nullptr, "K2_BroadcastMessage", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::GameplayMessageSubsystem_eventK2_BroadcastMessage_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04480403, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::GameplayMessageSubsystem_eventK2_BroadcastMessage_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UGameplayMessageSubsystem_K2_BroadcastMessage(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
// ********** End Class UGameplayMessageSubsystem Function K2_BroadcastMessage *********************

// ********** Begin Class UGameplayMessageSubsystem ************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UGameplayMessageSubsystem_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n * This system allows event raisers and listeners to register for messages without\n * having to know about each other directly, though they must agree on the format\n * of the message (as a USTRUCT() type).\n *\n *\n * You can get to the message router from the game instance:\n *    UGameInstance::GetSubsystem<UGameplayMessageSubsystem>(GameInstance)\n * or directly from anything that has a route to a world:\n *    UGameplayMessageSubsystem::Get(WorldContextObject)\n *\n * Note that call order when there are multiple listeners for the same channel is\n * not guaranteed and can change over time!\n */" },
#endif
		{ "IncludePath", "GameFramework/GameplayMessageSubsystem.h" },
		{ "ModuleRelativePath", "Public/GameFramework/GameplayMessageSubsystem.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "This system allows event raisers and listeners to register for messages without\nhaving to know about each other directly, though they must agree on the format\nof the message (as a USTRUCT() type).\n\n\nYou can get to the message router from the game instance:\n   UGameInstance::GetSubsystem<UGameplayMessageSubsystem>(GameInstance)\nor directly from anything that has a route to a world:\n   UGameplayMessageSubsystem::Get(WorldContextObject)\n\nNote that call order when there are multiple listeners for the same channel is\nnot guaranteed and can change over time!" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UGameplayMessageSubsystem constinit property declarations ****************
// ********** End Class UGameplayMessageSubsystem constinit property declarations ******************
	static constexpr UE::CodeGen::FClassNativeFunction Funcs[] = {
		{ .NameUTF8 = UTF8TEXT("K2_BroadcastMessage"), .Pointer = &UGameplayMessageSubsystem::execK2_BroadcastMessage },
	};
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FClassFunctionLinkInfo FuncInfo[] = {
		{ &Z_Construct_UFunction_UGameplayMessageSubsystem_K2_BroadcastMessage, "K2_BroadcastMessage" }, // 9994e649d7a432e09d4c83e17953bb615c7090dc
	};
	static_assert(UE_ARRAY_COUNT(FuncInfo) < 2048);
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGameplayMessageSubsystem>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UGameInstanceSubsystem,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GameplayMessageRuntime,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UGameplayMessageSubsystem,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	FuncInfo,
	nullptr,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	UE_ARRAY_COUNT(FuncInfo),
	0,
	0,
	0x000800A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static void UGameplayMessageSubsystem_StaticRegisterNativesUGameplayMessageSubsystem()
{
	UClass* Class = UGameplayMessageSubsystem::StaticClass();
	FNativeFunctionRegistrar::RegisterFunctions(Class, 		MakeConstArrayView(UHT_STATICS::Funcs));
}
FClassRegistrationInfo Z_Registration_Info_UClass_UGameplayMessageSubsystem;
UClass* Z_Construct_UClass_UGameplayMessageSubsystem(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UGameplayMessageSubsystem;
		if (!Z_Registration_Info_UClass_UGameplayMessageSubsystem.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("GameplayMessageSubsystem"),
				Z_Registration_Info_UClass_UGameplayMessageSubsystem.InnerSingleton,
				UGameplayMessageSubsystem_StaticRegisterNativesUGameplayMessageSubsystem,
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
		return Z_Registration_Info_UClass_UGameplayMessageSubsystem.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UGameplayMessageSubsystem.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UGameplayMessageSubsystem.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UGameplayMessageSubsystem.OuterSingleton;
}
#undef UHT_STATICS
UGameplayMessageSubsystem::UGameplayMessageSubsystem(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UGameplayMessageSubsystem);
UGameplayMessageSubsystem::~UGameplayMessageSubsystem() {}
// ********** End Class UGameplayMessageSubsystem **************************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Users_Silver_Soul_Documents_Unreal_Projects_TheAwaken_Plugins_GameplayMessageRouter_Source_GameplayMessageRuntime_Public_GameFramework_GameplayMessageSubsystem_h__Script_GameplayMessageRuntime_Statics
struct UHT_STATICS
{
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ Z_Construct_UScriptStruct_FGameplayMessageListenerHandle, Z_Construct_UScriptStruct_FGameplayMessageListenerHandle_Statics::NewStructOps, TEXT("GameplayMessageListenerHandle"),&Z_Registration_Info_UScriptStruct_FGameplayMessageListenerHandle, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FGameplayMessageListenerHandle), 3941547969U) },
		{ Z_Construct_UScriptStruct_FGameplayMessageListenerData, Z_Construct_UScriptStruct_FGameplayMessageListenerData_Statics::NewStructOps, TEXT("GameplayMessageListenerData"),&Z_Registration_Info_UScriptStruct_FGameplayMessageListenerData, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FGameplayMessageListenerData), 3150011098U) },
	};
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UGameplayMessageSubsystem, TEXT("UGameplayMessageSubsystem"), &Z_Registration_Info_UClass_UGameplayMessageSubsystem, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UGameplayMessageSubsystem), 3323618341U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Users_Silver_Soul_Documents_Unreal_Projects_TheAwaken_Plugins_GameplayMessageRouter_Source_GameplayMessageRuntime_Public_GameFramework_GameplayMessageSubsystem_h__Script_GameplayMessageRuntime_3ae8d24fa450c0c1eb27070fb4b75296b2f09d59{
	TEXT("/Script/GameplayMessageRuntime"),
	UHT_STATICS::ClassInfo, UE_ARRAY_COUNT(UHT_STATICS::ClassInfo),
	UHT_STATICS::ScriptStructInfo, UE_ARRAY_COUNT(UHT_STATICS::ScriptStructInfo),
	nullptr, 0,
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
