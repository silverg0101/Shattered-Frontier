#pragma once

#include "CoreMinimal.h"
#include "Construction/SFConstructionSaveCodec.h"

// Fixed-root local development save publication. This class exposes no path,
// world, actor, or authority controls to callers.
class SHATTEREDFRONTIER_API FSFConstructionSaveStore
{
public:
    FSFConstructionSaveStore();

    bool Publish(const FSFConstructionSandboxSnapshot& Snapshot, uint64& OutGeneration, FString& Reason);
    bool ReadLatest(FSFConstructionSandboxSnapshot& OutSnapshot, uint64& OutGeneration, FString& Reason);

private:
    friend class FSFConstructionSaveStoreTest;

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    enum class ETestFailure : uint8 { None, AfterFlushBeforeRename, BeforeRetention };
    bool SetTestRoot(const FString& CandidateRoot);
    void SetTestFailure(ETestFailure Failure) { TestFailure = Failure; }
    void SetBeforeRename(TFunction<void(const FString&)> Callback) { BeforeRename = MoveTemp(Callback); }
    ETestFailure TestFailure = ETestFailure::None;
    TFunction<void(const FString&)> BeforeRename;
#endif

    FString Root;
    int32 RetainGenerations = 3;
    int32 DirectoryEntryLimit = 128;
    bool bConfigurationValid = false;
    FString ConfigurationError;
};
