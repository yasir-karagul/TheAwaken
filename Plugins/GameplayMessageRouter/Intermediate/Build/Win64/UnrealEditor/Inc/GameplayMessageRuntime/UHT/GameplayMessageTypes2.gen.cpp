// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "GameFramework/GameplayMessageTypes2.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeGameplayMessageTypes2() {}

// ********** Begin Cross Module References ********************************************************
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_GameplayMessageRuntime(ETypeConstructPhase);
GAMEPLAYMESSAGERUNTIME_API UEnum* Z_Construct_UEnum_GameplayMessageRuntime_EGameplayMessageMatch(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Enum EGameplayMessageMatch *****************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UEnum_GameplayMessageRuntime_EGameplayMessageMatch_Statics
template<> GAMEPLAYMESSAGERUNTIME_NON_ATTRIBUTED_API UEnum* StaticEnum<EGameplayMessageMatch>()
{
	return Z_Construct_UEnum_GameplayMessageRuntime_EGameplayMessageMatch(ETypeConstructPhase::Outer);
}
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "BlueprintType", "true" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Match rule for message listeners\n" },
#endif
		{ "ExactMatch.Comment", "// An exact match will only receive messages with exactly the same channel\n// (e.g., registering for \"A.B\" will match a broadcast of A.B but not A.B.C)\n" },
		{ "ExactMatch.Name", "EGameplayMessageMatch::ExactMatch" },
		{ "ExactMatch.ToolTip", "An exact match will only receive messages with exactly the same channel\n(e.g., registering for \"A.B\" will match a broadcast of A.B but not A.B.C)" },
		{ "ModuleRelativePath", "Public/GameFramework/GameplayMessageTypes2.h" },
		{ "PartialMatch.Comment", "// A partial match will receive any messages rooted in the same channel\n// (e.g., registering for \"A.B\" will match a broadcast of A.B as well as A.B.C)\n" },
		{ "PartialMatch.Name", "EGameplayMessageMatch::PartialMatch" },
		{ "PartialMatch.ToolTip", "A partial match will receive any messages rooted in the same channel\n(e.g., registering for \"A.B\" will match a broadcast of A.B as well as A.B.C)" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Match rule for message listeners" },
#endif
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EGameplayMessageMatch::ExactMatch", (int64)EGameplayMessageMatch::ExactMatch },
		{ "EGameplayMessageMatch::PartialMatch", (int64)EGameplayMessageMatch::PartialMatch },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct UHT_STATICS 
const UECodeGen_Private::FEnumParams UHT_STATICS::EnumParams = {
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_GameplayMessageRuntime,
	nullptr,
	"EGameplayMessageMatch",
	"EGameplayMessageMatch",
	UHT_STATICS::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(UHT_STATICS::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	(uint8)UEnum::EUnderlyingType::uint8,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static FEnumRegistrationInfo ZRIE_EGameplayMessageMatch;
UEnum* Z_Construct_UEnum_GameplayMessageRuntime_EGameplayMessageMatch(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Outer)
	{
		if (!ZRIE_EGameplayMessageMatch.OuterSingleton)
		{
			ZRIE_EGameplayMessageMatch.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_GameplayMessageRuntime_EGameplayMessageMatch, (UObject*)Z_Construct_UPackage__Script_GameplayMessageRuntime(ETypeConstructPhase::Outer), TEXT("EGameplayMessageMatch"));
		}
		return ZRIE_EGameplayMessageMatch.OuterSingleton;
	}
	if (!ZRIE_EGameplayMessageMatch.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(ZRIE_EGameplayMessageMatch.InnerSingleton, UHT_STATICS::EnumParams);
	}
	return ZRIE_EGameplayMessageMatch.InnerSingleton;
}
#undef UHT_STATICS
// ********** End Enum EGameplayMessageMatch *******************************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Unreal_Projects_TheAwaken_Plugins_GameplayMessageRouter_Source_GameplayMessageRuntime_Public_GameFramework_GameplayMessageTypes2_h__Script_GameplayMessageRuntime_Statics
struct UHT_STATICS
{
	static constexpr FEnumRegisterCompiledInInfo EnumInfo[] = {
		{ Z_Construct_UEnum_GameplayMessageRuntime_EGameplayMessageMatch, TEXT("EGameplayMessageMatch"), &ZRIE_EGameplayMessageMatch, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 765969424U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Unreal_Projects_TheAwaken_Plugins_GameplayMessageRouter_Source_GameplayMessageRuntime_Public_GameFramework_GameplayMessageTypes2_h__Script_GameplayMessageRuntime_94a4d62d4fc75c8729adca2de936f6a64e6a8250{
	TEXT("/Script/GameplayMessageRuntime"),
	nullptr, 0,
	nullptr, 0,
	UHT_STATICS::EnumInfo, UE_ARRAY_COUNT(UHT_STATICS::EnumInfo),
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
