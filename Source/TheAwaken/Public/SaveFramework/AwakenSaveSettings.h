#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AwakenSaveSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Awaken Save"))
class THEAWAKEN_API UAwakenSaveSettings : public UDeveloperSettings{
	GENERATED_BODY()
public:
	UAwakenSaveSettings(){
		CategoryName = TEXT("Awaken");
		SectionName = TEXT("Save");
	}

	/** Folder under Saved/SaveGames/ that holds every slot. */
	UPROPERTY(Config, EditAnywhere, Category = "Files") FString saveRootFolder = TEXT("Awaken");

	/** Doc R13 3.11: keeping the previous set as a backup is recommended, not required. */
	UPROPERTY(Config, EditAnywhere, Category = "Files") bool keepBackupSet = true;

	/** Write files on a worker thread. Turn off in automation tests so the write finishes inside the test. */
	UPROPERTY(Config, EditAnywhere, Category = "Files") bool writeAsync = true;

	/** Oldest schema this build can still migrate from (Doc R13 3.11: back to the last public release). */
	UPROPERTY(Config, EditAnywhere, Category = "Files", meta = (ClampMin = "1")) int32 minSupportedSchemaVersion = 1;

	/** PROPOSED placeholder: Doc B18 3.6 says hold durations are data and gives no value for Save. */
	UPROPERTY(Config, EditAnywhere, Category = "Save Point", meta = (ClampMin = "0.1", Units = "s")) float saveHoldSeconds = 2.0f;

	/** Doc B20 3.8: the host keeps a dropped player's live state for 15 minutes, on real time. */
	UPROPERTY(Config, EditAnywhere, Category = "Away", meta = (ClampMin = "1", Units = "s")) float awayHoldSeconds = 900.0f;

	static const UAwakenSaveSettings* get(){
		return GetDefault<UAwakenSaveSettings>();
	}
};