#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AwakenHandshakeSettings.generated.h"

class UPlatformAuthProvider;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Awaken Session Handshake"))
class THEAWAKEN_API UAwakenHandshakeSettings : public UDeveloperSettings{
    GENERATED_BODY()

public:
    UAwakenHandshakeSettings();

    /** The Steam or Epic provider of step 11. Empty means the Null provider, which is refused in a Shipping build. */
    UPROPERTY(Config, EditAnywhere, Category = "Platform")
    TSoftClassPtr<UPlatformAuthProvider> providerClass;

    /** Editor and development builds only: allow the Null provider, which lets every peer pass. */
    UPROPERTY(Config, EditAnywhere, Category = "Platform")
    bool allowNullProvider = true;

    /** Doc B05 3.2: a check that does not answer in this time rejects the join with Timeout. */
    UPROPERTY(Config, EditAnywhere, Category = "Timing", meta = (ClampMin = "1", Units = "s"))
    float ownershipTimeoutSeconds = 10.0f;

    /** A slow name lookup never refuses a join; a placeholder name is used instead. */
    UPROPERTY(Config, EditAnywhere, Category = "Timing", meta = (ClampMin = "0.5", Units = "s"))
    float displayNameTimeoutSeconds = 3.0f;

    /** How long a passed check waits for the engine connection: a second, short window AFTER the checks. It is not B20's 30 s window (3.5),
     *  which starts at the password step and covers the checks. B20 calls releasePeer when its own window ends. */
    UPROPERTY(Config, EditAnywhere, Category = "Timing", meta = (ClampMin = "1", Units = "s"))
    float admissionLifetimeSeconds = 15.0f;

    /** Doc02 19.6: the host validates what a client sends. A longer token is ignored, so the ownership check fails. */
    UPROPERTY(Config, EditAnywhere, Category = "Limits", meta = (ClampMin = "256"))
    int32 maxAuthTokenLength = 8192;

    /** The same rule for the store id (the Steam id) that a claim carries. */
    UPROPERTY(Config, EditAnywhere, Category = "Limits", meta = (ClampMin = "16"))
    int32 maxExternalIdLength = 64;

    /** Under Saved/. PROPOSED: Doc B05 3.5 leaves the location open. A sibling of the slot root (Saved/SaveGames/Awaken), never inside it:
     *  the Save framework treats every subfolder of its slot root as a slot. It must survive as long as the save slots do. */
    UPROPERTY(Config, EditAnywhere, Category = "Identity")
    FString identityFolder = TEXT("SaveGames/AwakenIdentity");

    UPROPERTY(Config, EditAnywhere, Category = "Identity")
    FString identityFileName = TEXT("identity.json");

    /** Marks a local id so it can never be confused with a platform id (Doc B05 3.5). No characters a file name cannot hold. */
    UPROPERTY(Config, EditAnywhere, Category = "Identity")
    FString localIdPrefix = TEXT("local-");

    /** Used when the platform name cannot be read. Followed by the last four characters of the id. */
    UPROPERTY(Config, EditAnywhere, Category = "Identity")
    FString placeholderNamePrefix = TEXT("Player");

    /** Relative to the project directory. Written by the build step and staged with the game. It sits under Content/ because the staging setting takes paths relative to Content (step 8, part 2). */
    UPROPERTY(Config, EditAnywhere, Category = "Build")
    FString buildInfoRelativePath = TEXT("Content/Awaken/BuildInfo/BuildInfo.json");

    /** Used when the file above is missing, in the editor only. */
    UPROPERTY(Config, EditAnywhere, Category = "Build", meta = (ClampMin = "1"))
    int32 editorBuildNumber = 1;

    UPROPERTY(Config, EditAnywhere, Category = "Build")
    FString editorContentHash = TEXT("editor");

    static const UAwakenHandshakeSettings* get();
};