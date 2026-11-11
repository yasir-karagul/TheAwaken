// Public/GameFrameworkClassTable/AwakenGameMode.h
#pragma once
#include "GameFramework/GameModeBase.h"
#include "AwakenGameMode.generated.h"

class USessionHandshakeSubsystem;

UCLASS()
class THEAWAKEN_API AAwakenGameMode : public AGameModeBase{
    GENERATED_BODY()
public:
    AAwakenGameMode();

    // The engine door (Session Handshake guide, step 10). The three overrides must stay together:
    // without ChangeName a client can rename itself, without PostLogin nobody gets an identity, without PreLogin the door is open.
    virtual void PreLogin(const FString& options, const FString& address, const FUniqueNetIdRepl& uniqueId, FString& errorMessage) override;
    virtual void PostLogin(APlayerController* newPlayer) override;
    virtual void ChangeName(AController* controller, const FString& newName, bool nameChange) override;
    virtual void Logout(AController* exiting) override;

    virtual void StartPlay() override; // TEMPORARY, see "Until B06 exists"
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* newPlayer) override;

    // Called by Doc B06 when World restore step 13 is reached, also for a new game with no restore.
    void onWorldRestoreFinished();

private:
    void beginPlayerRestore(APlayerController* player);
    USessionHandshakeSubsystem* getHandshake() const;

    bool worldRestored = false;
    TArray<TWeakObjectPtr<APlayerController>> pendingPlayers;
};
