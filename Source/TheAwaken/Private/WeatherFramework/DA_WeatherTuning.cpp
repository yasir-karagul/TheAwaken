#include "WeatherFramework/DA_WeatherTuning.h"

UDA_WeatherTuning::UDA_WeatherTuning(){
    coldRateBySeason.Add(E_WeatherSeasonKey::Spring, 0.0f);
    coldRateBySeason.Add(E_WeatherSeasonKey::Summer, 0.0f);
    coldRateBySeason.Add(E_WeatherSeasonKey::Fall, 0.0f);
    coldRateBySeason.Add(E_WeatherSeasonKey::Winter, 0.0f);
    coldRateBySeason.Add(E_WeatherSeasonKey::Snowy, 0.0f);
}

FPrimaryAssetId UDA_WeatherTuning::GetPrimaryAssetId() const{
    return FPrimaryAssetId(FPrimaryAssetType("WeatherTuning"), GetFName());
}

float UDA_WeatherTuning::getColdBaseRate(E_WeatherSeasonKey key) const{
    const float* rate = coldRateBySeason.Find(key);
    if(rate == nullptr){
        return 0.0f;
    }
    return *rate;
}