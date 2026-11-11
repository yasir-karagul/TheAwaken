//UMETA(DisplayName = "...") is optional  we did not use it in the other 2 enums , it is just what will be displayed in 
// the editor, if we did not use it, the enum value will be displayed as it is in the editor, for example : World, Character

#pragma once
#include "CoreMinimal.h"
#include "E_SaveFramework.generated.h"

// Which file a framework's snapshot belongs to. Doc R13 3.4
UENUM(BlueprintType)
enum class E_SaveScope : uint8{
	World UMETA(DisplayName = "World"),
	Character UMETA(DisplayName = "Character"),
};

// result of the bag rule, Doc R13 3.6
UENUM(BlueprintType)
enum class E_SaveBagOutcome : uint8{
	NoConflict,
	SavedStateWinsRemoveWorldBag,
	WorldBagWinsPlayerBagless
};

// where a returning player's state comes from
UENUM(BlueprintType)
enum class E_SaveRestoreSource : uint8{
	None,
	HeldState,
	File,
	FileRefused,
	FileUnsupported
};



/*
regarding : uint8 in  enum class E_SaveFramework : uint8
we use uint8 because it uses 1 byte, for each enum ,  the default is int which uses 4 bytes.for example :
this is normal enum :
enum class E_SaveFramework
{
	World,
	Character
};

there is no need to use uint8 in this case because the default is int which uses 4 bytes, 
but if you want to use less memory you can use uint8 which uses 1 byte.for example :
enum class E_SaveFramework : uint8
{
	World,
	Character
};

Here, you explicitly tell the compiler to use an unsigned 8-bit integer, which occupies 1 byte.
Type	Storage size	Possible values
int	Usually 4 bytes	Typically −2.1 billion to +2.1 billion
uint8	1 byte	0–255


why we used it, since we have only 2 values in the enum, we can use uint8 to save memory, 

the enum has only two choices:
World = 0,
Character = 1
 */