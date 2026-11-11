#include "TimeFramework/DA_TimeSettings.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

float UDA_TimeSettings::gameSecondsPerRealSecond() const{
    return 86400.0f / static_cast<float>(FMath::Max(1, realSecondsPerGameDay));
}

FPrimaryAssetId UDA_TimeSettings::GetPrimaryAssetId() const{
    return FPrimaryAssetId(FPrimaryAssetType("TimeSettings"), GetFName());
}

#if WITH_EDITOR
EDataValidationResult UDA_TimeSettings::IsDataValid(FDataValidationContext& context) const{
    EDataValidationResult result = Super::IsDataValid(context);
    auto fail = [&context, &result](const TCHAR* text){
        context.AddError(FText::FromString(text));
        result = EDataValidationResult::Invalid;
    };
    if(realSecondsPerGameDay <= 0 || daysPerSeason <= 0 || snowyMinDays <= 0){
        fail(TEXT("Day length, days per season and the Snowy minimum must be above 0"));
    }
    if(snowyMinDays > daysPerSeason){
        fail(TEXT("snowyMinDays is larger than daysPerSeason"));
    }
    if(awayIdleSeconds < 0.0f){
        fail(TEXT("awayIdleSeconds must be 0 (idle detection off) or more (R12-IG4)"));
    }
    const bool phasesRise = dawnStartHour < dayStartHour && dayStartHour < duskStartHour && duskStartHour < nightStartHour && nightStartHour < 24.0f;
    if(!phasesRise || FMath::RoundToInt(dawnStartHour) != wakeHour){
        fail(TEXT("Phase hours must rise inside one day and Dawn must equal the wake hour"));
    }
    if(sleepWindowOpenHour <= sleepLastStartHour + 12 || sleepLastStartHour > wakeHour - 1){
        fail(TEXT("Sleep window: open hour too early, or less than one hour left before waking"));
    }
    if(skipFadeOutSeconds <= 0.0f || skipFadeOutSeconds * gameSecondsPerRealSecond() >= 3600.0f){
        fail(TEXT("skipFadeOutSeconds must be above 0 and shorter than one game hour"));
    }
    return result;
}
#endif