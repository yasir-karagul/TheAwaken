// Private/LocalizationFramework/Tests/LocalizationTests.cpp
// Dev Tools tests 1 and 2 (tier 1). Tests 3 to 5 need a running game and are run by hand with the commands above.
// Run: Automation RunTests Awaken.Localization
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "LocalizationFramework/LocalizationKeyValidator.h"
#include "LocalizationFramework/LocalizationService.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocalizationKeyOutcomeTest, "Awaken.Localization.KeyOutcome", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLocalizationKeyOutcomeTest::RunTest(const FString& parameters){
    TestTrue(TEXT("an empty key is EmptyKey"), FLocalizationKeyValidator::findOutcome(NAME_None) == E_LocalizationLookupOutcome::EmptyKey);
    TestTrue(TEXT("an unknown root is UnknownRoot"), FLocalizationKeyValidator::findOutcome(FName(TEXT("Zzz.Key"))) == E_LocalizationLookupOutcome::UnknownRoot);
    TestTrue(TEXT("a key with no entry is MissingKey"), FLocalizationKeyValidator::findOutcome(FName(TEXT("UI.Nope.Missing"))) == E_LocalizationLookupOutcome::MissingKey);
    TestTrue(TEXT("a real key is Found"), FLocalizationKeyValidator::findOutcome(FName(TEXT("UI.SlotList.NeverSaved"))) == E_LocalizationLookupOutcome::Found);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocalizationNameIsolationTest, "Awaken.Localization.NameIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLocalizationNameIsolationTest::RunTest(const FString& parameters){
    FString arabicName;
    arabicName.AppendChar(static_cast<TCHAR>(0x0645));
    arabicName.AppendChar(static_cast<TCHAR>(0x062D));
    arabicName.AppendChar(static_cast<TCHAR>(0x0645));
    arabicName.AppendChar(static_cast<TCHAR>(0x062F));

    TArray<FString> names;
    names.Add(TEXT("Ali"));
    names.Add(arabicName);
    names.Add(FString());

    for(const FString& name : names){
        const FString wrapped = ULocalizationService::wrapNameForSentence(FText::AsCultureInvariant(name)).ToString();
        TestEqual(TEXT("two characters longer than the name"), wrapped.Len(), name.Len() + 2);
        TestEqual(TEXT("opens with U+2068"), static_cast<int32>(wrapped[0]), 0x2068);
        TestEqual(TEXT("closes with U+2069"), static_cast<int32>(wrapped[wrapped.Len() - 1]), 0x2069);
        TestEqual(TEXT("the name is unchanged inside"), wrapped.Mid(1, name.Len()), name);
    }
    return true;
}
#endif
