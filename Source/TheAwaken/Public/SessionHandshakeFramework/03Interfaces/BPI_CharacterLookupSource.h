#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SessionHandshakeFramework/00Enums/E_SessionHandshakeFramework.h"
#include "BPI_CharacterLookupSource.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UBPI_CharacterLookupSource : public UInterface{
    GENERATED_BODY()
};

class THEAWAKEN_API IBPI_CharacterLookupSource{
    GENERATED_BODY()

public:
    /** Host only. Query: never changes anything. lookupIds is the primary id first, then the linked ids; a match on any one counts
     *  (Doc R13 3.10). A held state inside its 15 minutes counts as a character; a file or held state of a different WorldID counts
     *  as no character (Doc R13 3.10, Doc B20 3.4 step 8). Return NewPlayer when there is nothing to restore. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Handshake")
    E_ReturningLookup lookupCharacter(const TArray<FString>& lookupIds) const;
};
