#include "SessionHandshakeFramework/AwakenHandshakeSettings.h"

UAwakenHandshakeSettings::UAwakenHandshakeSettings(){
    CategoryName = TEXT("Awaken");
    SectionName = TEXT("Session Handshake");
}

const UAwakenHandshakeSettings* UAwakenHandshakeSettings::get(){
    return GetDefault<UAwakenHandshakeSettings>();
}
