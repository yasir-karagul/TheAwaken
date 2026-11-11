#include "ProgressionFramework/ProgressionComponent.h"
#include "ProgressionFramework/02Tags/T_ProgressionFramework.h"
#include "TimeFramework/TimeSubsystem.h"   // readDaysPassed (Step 5.2). Time is built; this is a soft dependency (Doc00 §7)
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTagsManager.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY(LogAwakenProgression);

namespace{
    // Finds the row whose partTag equals the given tag. Used by the grant and by the checker.
    const FS_StoryChainRow* findRowIn(const TArray<FS_StoryChainRow*>& rows, const FGameplayTag& partTag){
        for(const FS_StoryChainRow* row : rows){
            if(row != nullptr && row->partTag.MatchesTagExact(partTag)){
                return row;
            }
        }
        return nullptr;
    }
}

UProgressionComponent::UProgressionComponent(){
    PrimaryComponentTick.bCanEverTick = false;   // Doc02 section 5: nothing here polls
    SetIsReplicatedByDefault(true);
}

void UProgressionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    // The parameter must be spelled OutLifetimeProps: DOREPLIFETIME_CONDITION reads that exact name (Doc02 section 20.5).
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Every client gets the set (Doc R06 8). COND_None means "no condition: send to everyone".
    DOREPLIFETIME_CONDITION(UProgressionComponent, unlockedTags, COND_None);
    DOREPLIFETIME_CONDITION(UProgressionComponent, isSeeded, COND_None);
}

void UProgressionComponent::BeginPlay(){
    Super::BeginPlay();

    // Only the host evaluates grants, so only the host needs the tables.
    if(isServer()){
        ensureAssetsLoaded();
        validateStoryChain();
    }
}

bool UProgressionComponent::isServer() const{
    // Offline, the local machine has authority (Doc00 5.1). In co-op only the host does.
    const AActor* owner = GetOwner();
    return owner != nullptr && owner->HasAuthority();
}

const TCHAR* UProgressionComponent::roleName() const{
    // Doc B22 3.6: every log line names the role of this machine, so two machines in one editor session can be told apart.
    const ENetMode netMode = GetNetMode();
    if(netMode == NM_Standalone){
        return TEXT("Offline");
    }
    if(netMode == NM_Client){
        return TEXT("Client");
    }
    return TEXT("Host");
}

bool UProgressionComponent::isReady() const{
    return isSeeded;
}

UProgressionComponent* UProgressionComponent::findProgression(const UObject* worldContextObject){
    if(worldContextObject == nullptr || GEngine == nullptr){
        return nullptr;
    }
    const UWorld* world = GEngine->GetWorldFromContextObject(worldContextObject, EGetWorldErrorMode::ReturnNull);
    if(world == nullptr){
        return nullptr;
    }
    const AGameStateBase* gameState = world->GetGameState();
    if(gameState == nullptr){
        return nullptr;
    }
    return gameState->FindComponentByClass<UProgressionComponent>();
}

void UProgressionComponent::ensureAssetsLoaded(){
    // Both assets are tiny (a table of a few dozen rows, a list of tags), so a synchronous load once is fine.
    if(loadedStoryChain == nullptr && !storyChainTable.IsNull()){
        loadedStoryChain = storyChainTable.LoadSynchronous();
    }
    if(loadedStartingUnlocks == nullptr && !startingUnlocks.IsNull()){
        loadedStartingUnlocks = startingUnlocks.LoadSynchronous();
    }
}

FS_ProgressionResult UProgressionComponent::grantUnlock(const FGameplayTag& tag){
    FS_ProgressionResult result;

    // 1. Validate everything first. Nothing changes until every check has passed.
    if(!isServer()){
        UE_LOG(LogAwakenProgression, Warning, TEXT("[%s] grantUnlock('%s') refused (Progression.FailReason.NotAuthority): only the host grants."), roleName(), *tag.ToString());
        result.failReason = T_Progression_FailReason_NotAuthority.GetTag();
        return result;
    }
    if(!isSeeded){
        UE_LOG(LogAwakenProgression, Warning, TEXT("[%s] grantUnlock('%s') refused (Progression.FailReason.NotReady): the unlocked set is not seeded yet."), roleName(), *tag.ToString());
        result.failReason = T_Progression_FailReason_NotReady.GetTag();
        return result;
    }
    if(!isGrantableTag(tag)){
        UE_LOG(LogAwakenProgression, Warning, TEXT("[%s] grantUnlock('%s') refused (Progression.FailReason.InvalidTag): it is empty, is one of the five roots, or is not below Progression.Blueprint, Recipe, Ability, Discovery or Story."), roleName(), *tag.ToString());
        result.failReason = T_Progression_FailReason_InvalidTag.GetTag();
        return result;
    }
    if(unlockedTags.HasTagExact(tag)){
        // Benign: an unlock is granted once (Doc R06, Index A87). Verbose only, because callers may repeat it (Doc B22 3.6).
        UE_LOG(LogAwakenProgression, Verbose, TEXT("[%s] grantUnlock('%s') refused (Progression.FailReason.AlreadyUnlocked)."), roleName(), *tag.ToString());
        result.failReason = T_Progression_FailReason_AlreadyUnlocked.GetTag();
        return result;
    }

    bool grantsComplete = false;
    if(tag.MatchesTag(T_Progression_Story.GetTag())){
        ensureAssetsLoaded();
        const FGameplayTag storyRefusal = checkStoryGrant(tag, grantsComplete);
        if(storyRefusal.IsValid()){
            result.failReason = storyRefusal;
            return result;
        }
    }

    // 2. Change state last. Add both tags first, so a listener that reacts to the part already sees Story.Complete.
    unlockedTags.AddTag(tag);
    UE_LOG(LogAwakenProgression, Log, TEXT("[%s] Unlocked '%s'."), roleName(), *tag.ToString());
    const bool addsComplete = grantsComplete && !unlockedTags.HasTagExact(T_Progression_Story_Complete.GetTag());
    if(addsComplete){
        unlockedTags.AddTag(T_Progression_Story_Complete.GetTag());
        UE_LOG(LogAwakenProgression, Log, TEXT("[%s] Unlocked '%s' (the last story part is done)."), roleName(), *T_Progression_Story_Complete.GetTag().ToString());
    }

    // 3. Tell this machine's listeners (Step 7). Clients are told by their own replication callback.
    announce(tag);
    if(addsComplete){
        announce(T_Progression_Story_Complete.GetTag());
    }

    result.success = true;
    return result;
}

bool UProgressionComponent::isGrantableTag(const FGameplayTag& tag) const{
    if(!tag.IsValid()){
        return false;
    }
    const FGameplayTag roots[] = {
        T_Progression_Blueprint.GetTag(),
        T_Progression_Recipe.GetTag(),
        T_Progression_Ability.GetTag(),
        T_Progression_Discovery.GetTag(),
        T_Progression_Story.GetTag()
    };
    for(const FGameplayTag& root : roots){
        // MatchesTag is true for the root itself and for anything below it. A root is not an unlock, so exclude it.
        if(tag.MatchesTag(root) && !tag.MatchesTagExact(root)){
            return true;
        }
    }
    return false;
}

const FS_StoryChainRow* UProgressionComponent::findStoryRow(const FGameplayTag& tag) const{
    if(loadedStoryChain == nullptr){
        return nullptr;
    }
    TArray<FS_StoryChainRow*> rows;
    loadedStoryChain->GetAllRows<FS_StoryChainRow>(TEXT("UProgressionComponent::findStoryRow"), rows);
    return findRowIn(rows, tag);
}

// Returns an empty tag when the grant is allowed, otherwise the refusal reason (Doc R06 3.1a).
FGameplayTag UProgressionComponent::checkStoryGrant(const FGameplayTag& tag, bool& grantsComplete) const{
    grantsComplete = false;

    // Quests are off on Peaceful (Doc B13, Doc R06 3.3): no story part is granted there, whoever calls. Checked first, so it also answers when the table is missing.
    if(!readQuestsEnabled()){
        UE_LOG(LogAwakenProgression, Warning, TEXT("[%s] Story part '%s' refused (Progression.FailReason.QuestsDisabled): quests are off in this world."), roleName(), *tag.ToString());
        return T_Progression_FailReason_QuestsDisabled.GetTag();
    }

    if(loadedStoryChain == nullptr){
        UE_LOG(LogAwakenProgression, Error, TEXT("[%s] Story part '%s' refused (Progression.FailReason.NotReady): the story chain table is not set on the component or did not load."), roleName(), *tag.ToString());
        return T_Progression_FailReason_NotReady.GetTag();
    }

    const FS_StoryChainRow* row = findStoryRow(tag);
    if(row == nullptr){
        UE_LOG(LogAwakenProgression, Warning, TEXT("[%s] Story tag '%s' refused (Progression.FailReason.UnknownStoryPart): it has no row in the story chain table."), roleName(), *tag.ToString());
        return T_Progression_FailReason_UnknownStoryPart.GetTag();
    }

    // A first part has an empty prerequisite and always passes this check (there is no Part 0).
    if(row->prerequisiteTag.IsValid() && !unlockedTags.HasTagExact(row->prerequisiteTag)){
        UE_LOG(LogAwakenProgression, Log, TEXT("[%s] Story part '%s' refused (Progression.FailReason.PrerequisiteMissing): its prerequisite '%s' is not unlocked."), roleName(), *tag.ToString(), *row->prerequisiteTag.ToString());
        return T_Progression_FailReason_PrerequisiteMissing.GetTag();
    }

    // The minimum days are read from Time at this moment. It is a read, not a new event (XQ-S6-04).
    // The limit is inclusive: a value exactly at the minimum is allowed (Doc02 section 19.9, Index A103, S4-F26).
    if(row->minimumDays > 0 && readDaysPassed() < row->minimumDays){
        UE_LOG(LogAwakenProgression, Log, TEXT("[%s] Story part '%s' refused (Progression.FailReason.TooEarly): it needs day %d and the game is on day %d."), roleName(), *tag.ToString(), row->minimumDays, readDaysPassed());
        return T_Progression_FailReason_TooEarly.GetTag();
    }

    grantsComplete = row->isLastPart;
    return FGameplayTag();
}

int32 UProgressionComponent::readDaysPassed() const{
    // Doc R12 (built). Time is a world subsystem and getDaysPassed() answers its replicated calendar, so this is right on the host and on a client.
    // Time starts at day 0 (R12 §3.3a), and a row's minimumDays is compared inclusive.
    // No Time subsystem (a bare test world) answers 0, so a row with minimumDays above 0 is refused (TooEarly). That fails closed.
    const UTimeSubsystem* time = UTimeSubsystem::get(this);
    return time != nullptr ? static_cast<int32>(time->getDaysPassed()) : 0;
}

void UProgressionComponent::validateStoryChain() const{
    if(loadedStoryChain == nullptr){
        return;   // No table yet is fine while the story content is not written (Doc R06 4.1).
    }

    TArray<FS_StoryChainRow*> rows;
    loadedStoryChain->GetAllRows<FS_StoryChainRow>(TEXT("UProgressionComponent::validateStoryChain"), rows);

    const FGameplayTag storyRoot = T_Progression_Story.GetTag();
    TSet<FGameplayTag> seenParts;
    int32 lastPartCount = 0;

    // Pass 1: each row on its own.
    for(const FS_StoryChainRow* row : rows){
        if(row == nullptr){
            continue;
        }
        const bool partIsUsable = row->partTag.IsValid()
            && row->partTag.MatchesTag(storyRoot)
            && !row->partTag.MatchesTagExact(storyRoot)
            && !row->partTag.MatchesTagExact(T_Progression_Story_Complete.GetTag());
        if(!partIsUsable){
            UE_LOG(LogAwakenProgression, Error, TEXT("[%s] Story chain: a row has an empty or unusable part tag '%s'."), roleName(), *row->partTag.ToString());
            continue;
        }
        if(seenParts.Contains(row->partTag)){
            UE_LOG(LogAwakenProgression, Error, TEXT("[%s] Story chain: '%s' has more than one row."), roleName(), *row->partTag.ToString());
        }
        seenParts.Add(row->partTag);
        if(row->prerequisiteTag.IsValid()){
            if(row->prerequisiteTag.MatchesTagExact(row->partTag)){
                UE_LOG(LogAwakenProgression, Error, TEXT("[%s] Story chain: '%s' lists itself as its own prerequisite."), roleName(), *row->partTag.ToString());
            }else if(findRowIn(rows, row->prerequisiteTag) == nullptr){
                UE_LOG(LogAwakenProgression, Warning, TEXT("[%s] Story chain: '%s' has a prerequisite '%s' that is not a row of the table."), roleName(), *row->partTag.ToString(), *row->prerequisiteTag.ToString());
            }
        }
        if(row->isLastPart){
            lastPartCount++;
        }
    }
    if(lastPartCount > 1){
        UE_LOG(LogAwakenProgression, Warning, TEXT("[%s] Story chain: %d rows are marked as the last part. A story has one."), roleName(), lastPartCount);
    }

    // Pass 2: loops only. Walk back from each row. The row is in a loop when the walk returns to it.
    // A loop is reported once: every row of the loop is remembered, so none of them reports it again.
    // A self-prerequisite was already reported in Pass 1.
    TSet<FGameplayTag> reportedInLoop;
    for(const FS_StoryChainRow* row : rows){
        if(row == nullptr || !row->partTag.IsValid() || reportedInLoop.Contains(row->partTag) || row->prerequisiteTag.MatchesTagExact(row->partTag)){
            continue;
        }
        TArray<FGameplayTag> walked;
        const FS_StoryChainRow* current = row;
        for(int32 steps = 0; steps < rows.Num() && current != nullptr && current->prerequisiteTag.IsValid(); steps++){
            walked.Add(current->partTag);
            current = findRowIn(rows, current->prerequisiteTag);
            if(current != nullptr && current->partTag.MatchesTagExact(row->partTag)){
                UE_LOG(LogAwakenProgression, Warning, TEXT("[%s] Story chain: '%s' is in a prerequisite loop and can never be granted."), roleName(), *row->partTag.ToString());
                for(const FGameplayTag& member : walked){
                    reportedInLoop.Add(member);
                }
                break;
            }
        }
    }
}

bool UProgressionComponent::isUnlocked(const FGameplayTag& tag) const{
    return tag.IsValid() && unlockedTags.HasTagExact(tag);
}

FGameplayTagContainer UProgressionComponent::getAllUnlocked() const{
    return unlockedTags;   // a copy, so a caller cannot change the set
}

void UProgressionComponent::announce(const FGameplayTag& tag) const{
    // Broadcast on this machine only (Doc02 sections 11 and 19.2).
    const UWorld* world = GetWorld();
    UGameInstance* gameInstance = world != nullptr ? world->GetGameInstance() : nullptr;
    UGameplayMessageSubsystem* messageSubsystem = gameInstance != nullptr ? gameInstance->GetSubsystem<UGameplayMessageSubsystem>() : nullptr;
    if(messageSubsystem == nullptr){
        return;
    }
    FS_ProgressionUnlockMessage message;
    message.unlockedTag = tag;
    messageSubsystem->BroadcastMessage(T_Progression_UnlockGranted.GetTag(), message);
}

void UProgressionComponent::OnRep_unlockedTags(){
    handleReplicatedChange();
}

void UProgressionComponent::OnRep_isSeeded(){
    handleReplicatedChange();
}

void UProgressionComponent::handleReplicatedChange(){
    // The host has not finished filling the set. Nothing to announce yet.
    if(!isSeeded){
        return;
    }

    // First sync on this machine: copy silently (R06 7, S6-F04).
    if(!hasSynced){
        knownTags = unlockedTags;
        hasSynced = true;
        UE_LOG(LogAwakenProgression, Log, TEXT("[%s] First sync: %d unlocked, no messages raised."), roleName(), unlockedTags.Num());
        return;
    }

    // Later changes: announce each tag this machine has not announced yet, in the order they were added.
    for(const FGameplayTag& tag : unlockedTags){
        if(!knownTags.HasTagExact(tag)){
            knownTags.AddTag(tag);
            UE_LOG(LogAwakenProgression, Log, TEXT("[%s] Unlocked '%s' (received from the host)."), roleName(), *tag.ToString());
            announce(tag);
        }
    }
}

void UProgressionComponent::finishSeeding(){
    // Replicated. Clients treat their first receipt of the set as silent (Step 7).
    isSeeded = true;
}

int32 UProgressionComponent::addStartingTags(){
    // Adds every tag of DA_StartingUnlocks that the set does not hold yet, silently, and returns how many it added. Used by the seeding of a new world
    // and by the restore of a saved one (open item 5). A story tag, or any tag that is not grantable, is skipped with one log line:
    // a story part only ever comes through the story chain.
    int32 added = 0;
    if(loadedStartingUnlocks == nullptr){
        return added;
    }
    for(const FGameplayTag& tag : loadedStartingUnlocks->startingTags){
        if(!isGrantableTag(tag) || tag.MatchesTag(T_Progression_Story.GetTag())){
            UE_LOG(LogAwakenProgression, Warning, TEXT("[%s] Starting unlock '%s' skipped: it is not a tag below Progression.Blueprint, Recipe, Ability or Discovery. A story part comes only through the story chain."), roleName(), *tag.ToString());
            continue;
        }
        if(!unlockedTags.HasTagExact(tag)){
            unlockedTags.AddTag(tag);
            added++;
        }
    }
    return added;
}

bool UProgressionComponent::seedNewWorld(){
    if(!isServer()){
        return false;
    }
    if(isSeeded){
        UE_LOG(LogAwakenProgression, Warning, TEXT("[%s] seedNewWorld ignored: the unlocked set is already seeded."), roleName());
        return false;
    }
    ensureAssetsLoaded();

    // 1. The starting set: data, silent (R06 4.1, A93, A106).
    if(loadedStartingUnlocks != nullptr){
        addStartingTags();
    }else{
        UE_LOG(LogAwakenProgression, Warning, TEXT("[%s] No DA_StartingUnlocks is assigned. The world starts with nothing unlocked."), roleName());
    }

    // 2. Peaceful: everything a normal game can give, at the start, with no exploration (R06 3.1a, B13).
    if(readUnlockEverything()){
        unlockedTags.AppendTags(getEveryUnlockTag());
    }

    finishSeeding();
    UE_LOG(LogAwakenProgression, Log, TEXT("[%s] Seeded a new world: %d unlocked."), roleName(), unlockedTags.Num());
    return true;
}

FGameplayTagContainer UProgressionComponent::getEveryUnlockTag() const{
    FGameplayTagContainer result;
    const UGameplayTagsManager& tagManager = UGameplayTagsManager::Get();

    // Blueprint, Recipe and Ability only. Never Story (R06 3.1a, Index A111). Discovery is left out on purpose: Peaceful places every helper NPC through the
    // difficulty profile, and a discovery records where it was found (R14 3.6), so a bulk grant would skip that.
    const FGameplayTag roots[] = {
        T_Progression_Blueprint.GetTag(),
        T_Progression_Recipe.GetTag(),
        T_Progression_Ability.GetTag()
    };
    for(const FGameplayTag& root : roots){
        const FGameplayTagContainer children = tagManager.RequestGameplayTagChildren(root);
        for(const FGameplayTag& child : children){
            // Leaves only: a grouping tag such as Progression.Blueprint.Building is not an unlock (decision 11, PROPOSED).
            if(tagManager.RequestGameplayTagChildren(child).IsEmpty()){
                result.AddTag(child);
            }
        }
    }
    return result;
}

bool UProgressionComponent::readUnlockEverything() const{
    // SWAP POINT (Doc B13). When UDifficultySubsystem exists, replace the body with a read of the Peaceful flag.
    // B13 lists GetProfile() and a flag named bUnlockEverything; both become camelCase under Doc02 section 20:
    //     const UDifficultySubsystem* difficulty = GetWorld()->GetGameInstance()->GetSubsystem<UDifficultySubsystem>();
    //     return difficulty != nullptr && difficulty->getProfile() != nullptr && difficulty->getProfile()->unlockEverything;
    // Until then this returns false. To try the Peaceful path before B13 exists, return true here for one test run.
    return false;
}

bool UProgressionComponent::readQuestsEnabled() const{
    // SWAP POINT (Doc B13). When UDifficultySubsystem exists, replace the body with a read of the quests flag. B13 names it bQuestsEnabled; under Doc02 section 20 it becomes questsEnabled:
    //     const UDifficultySubsystem* difficulty = GetWorld()->GetGameInstance()->GetSubsystem<UDifficultySubsystem>();
    //     return difficulty == nullptr || difficulty->getProfile() == nullptr || difficulty->getProfile()->questsEnabled;
    // Until then this returns true, so no story grant is refused. To try the Peaceful refusal before B13 exists, return false here for one test run.
    return true;
}

FS_ProgressionSaveRecord UProgressionComponent::getSnapshotForSave() const{
    // Query: reads, never changes (Doc02 section 4).
    FS_ProgressionSaveRecord record;
    for(const FGameplayTag& tag : unlockedTags){
        record.unlockedTags.Add(tag.GetTagName());
    }
    return record;
}

bool UProgressionComponent::restoreFromSnapshot(const FS_ProgressionSaveRecord& record){
    // Command: host only, once, and silent. It never raises a message and never triggers a save.
    if(!isServer()){
        return false;
    }
    if(isSeeded){
        UE_LOG(LogAwakenProgression, Warning, TEXT("[%s] restoreFromSnapshot ignored: the unlocked set is already seeded."), roleName());
        return false;
    }

    FGameplayTagContainer restored;
    for(const FName& name : record.unlockedTags){
        // The second argument false means: do not log an error engine-side, return an empty tag instead.
        const FGameplayTag tag = FGameplayTag::RequestGameplayTag(name, false);
        if(!tag.IsValid()){
            // Doc R13 3.11 (written for item ids, applied here to tags): an id that no longer exists is skipped with one log line, and the load continues.
            UE_LOG(LogAwakenProgression, Warning, TEXT("[%s] Saved unlock '%s' skipped: the tag no longer exists."), roleName(), *name.ToString());
            continue;
        }
        restored.AddTag(tag);
    }

    unlockedTags = restored;   // replaces the set with what the file says
    // The starting set is the baseline every world has (R06 4.1, A93, A106), so a tag a patch adds to it reaches saves made before the patch (open item 5, approved).
    // Silent like the seeding, and it only adds: nothing is revoked.
    ensureAssetsLoaded();
    const int32 startingAdded = addStartingTags();
    if(readUnlockEverything()){
        // Peaceful only (decision 16, PROPOSED): everything a normal game can give is available from the start (R06 3.3),
        // including a tag added in a later patch. Silent, like the seeding. The difficulty is restored first (B06 3.6 step 2).
        unlockedTags.AppendTags(getEveryUnlockTag());
    }
    finishSeeding();
    UE_LOG(LogAwakenProgression, Log, TEXT("[%s] Restored %d unlocked from the save (%d of them added from the starting set)."), roleName(), unlockedTags.Num(), startingAdded);
    return true;
}
