// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "K2Node_AsyncAction_ListenForGameplayMessages.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeK2Node_AsyncAction_ListenForGameplayMessages() {}

// ********** Begin Cross Module References ********************************************************
BLUEPRINTGRAPH_API UClass* Z_Construct_UClass_UK2Node_AsyncAction(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GameplayMessageNodes(ETypeConstructPhase);
GAMEPLAYMESSAGENODES_API UClass* Z_Construct_UClass_UK2Node_AsyncAction_ListenForGameplayMessages(ETypeConstructPhase);
GAMEPLAYMESSAGENODES_API UClass* Z_Construct_UClass_UK2Node_AsyncAction_ListenForGameplayMessages(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class UK2Node_AsyncAction_ListenForGameplayMessages ****************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_UK2Node_AsyncAction_ListenForGameplayMessages_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n * Blueprint node which is spawned to handle the async logic for UAsyncAction_RegisterGameplayMessageReceiver\n */" },
#endif
		{ "IncludePath", "K2Node_AsyncAction_ListenForGameplayMessages.h" },
		{ "ModuleRelativePath", "Public/K2Node_AsyncAction_ListenForGameplayMessages.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Blueprint node which is spawned to handle the async logic for UAsyncAction_RegisterGameplayMessageReceiver" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UK2Node_AsyncAction_ListenForGameplayMessages constinit property declarations 
// ********** End Class UK2Node_AsyncAction_ListenForGameplayMessages constinit property declarations 
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UK2Node_AsyncAction_ListenForGameplayMessages>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UK2Node_AsyncAction,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GameplayMessageNodes,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_UK2Node_AsyncAction_ListenForGameplayMessages,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	nullptr,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	0,
	0,
	0x000000A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
FClassRegistrationInfo Z_Registration_Info_UClass_UK2Node_AsyncAction_ListenForGameplayMessages;
UClass* Z_Construct_UClass_UK2Node_AsyncAction_ListenForGameplayMessages(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = UK2Node_AsyncAction_ListenForGameplayMessages;
		if (!Z_Registration_Info_UClass_UK2Node_AsyncAction_ListenForGameplayMessages.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("K2Node_AsyncAction_ListenForGameplayMessages"),
				Z_Registration_Info_UClass_UK2Node_AsyncAction_ListenForGameplayMessages.InnerSingleton,
				nullptr,
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
		return Z_Registration_Info_UClass_UK2Node_AsyncAction_ListenForGameplayMessages.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_UK2Node_AsyncAction_ListenForGameplayMessages.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UK2Node_AsyncAction_ListenForGameplayMessages.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_UK2Node_AsyncAction_ListenForGameplayMessages.OuterSingleton;
}
#undef UHT_STATICS
UK2Node_AsyncAction_ListenForGameplayMessages::UK2Node_AsyncAction_ListenForGameplayMessages(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UK2Node_AsyncAction_ListenForGameplayMessages);
UK2Node_AsyncAction_ListenForGameplayMessages::~UK2Node_AsyncAction_ListenForGameplayMessages() {}
// ********** End Class UK2Node_AsyncAction_ListenForGameplayMessages ******************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Unreal_Projects_TheAwaken_Plugins_GameplayMessageRouter_Source_GameplayMessageNodes_Public_K2Node_AsyncAction_ListenForGameplayMessages_h__Script_GameplayMessageNodes_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UK2Node_AsyncAction_ListenForGameplayMessages, TEXT("UK2Node_AsyncAction_ListenForGameplayMessages"), &Z_Registration_Info_UClass_UK2Node_AsyncAction_ListenForGameplayMessages, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UK2Node_AsyncAction_ListenForGameplayMessages), 4235510507U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Unreal_Projects_TheAwaken_Plugins_GameplayMessageRouter_Source_GameplayMessageNodes_Public_K2Node_AsyncAction_ListenForGameplayMessages_h__Script_GameplayMessageNodes_63a125af5363b428e3c640287d9b626ba7d8e60e{
	TEXT("/Script/GameplayMessageNodes"),
	UHT_STATICS::ClassInfo, UE_ARRAY_COUNT(UHT_STATICS::ClassInfo),
	nullptr, 0,
	nullptr, 0,
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
