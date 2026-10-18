#include "Construction/SFConstructionSaveStore.h"

#include "Construction/SFConstructionSandboxSettings.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Serialization/Archive.h"

namespace
{
constexpr int32 MaxSaveBytes = 65536;
constexpr uint64 MaxGeneration = MAX_uint64;
const TCHAR GenerationPrefix[] = TEXT("generation-");
const TCHAR SaveExtension[] = TEXT(".sfcs");
const TCHAR AutomationFolder[] = TEXT("Automation");

struct FObservedGeneration
{
    uint64 Number = 0;
};

bool ParseGenerationName(const FString& Name, uint64& OutGeneration)
{
    constexpr int32 PrefixLength = UE_ARRAY_COUNT(GenerationPrefix)-1;
    constexpr int32 NumberLength = 20;
    constexpr int32 ExtensionLength = UE_ARRAY_COUNT(SaveExtension)-1;
    if (Name.Len() != PrefixLength+NumberLength+ExtensionLength
        || !Name.StartsWith(GenerationPrefix,ESearchCase::CaseSensitive)
        || !Name.EndsWith(SaveExtension,ESearchCase::CaseSensitive)) return false;
    uint64 Number=0;
    for (int32 I=PrefixLength; I<PrefixLength+NumberLength; ++I)
    {
        const TCHAR C=Name[I];
        if (C<'0' || C>'9') return false;
        const uint64 Digit=static_cast<uint64>(C-'0');
        if (Number>(MAX_uint64-Digit)/10) return false;
        Number=Number*10+Digit;
    }
    if (Number==0) return false;
    OutGeneration=Number;
    return true;
}

FString GenerationName(uint64 Generation)
{
    return FString::Printf(TEXT("generation-%020llu.sfcs"), static_cast<unsigned long long>(Generation));
}

FString GenerationPath(const FString& Root, uint64 Generation)
{
    return FPaths::Combine(Root,GenerationName(Generation));
}

class FBoundedDirectoryVisitor final : public IPlatformFile::FDirectoryVisitor
{
public:
    explicit FBoundedDirectoryVisitor(int32 InLimit) : Limit(InLimit) {}

    virtual bool Visit(const TCHAR* Filename, bool bIsDirectory) override
    {
        ++EntryCount;
        if (EntryCount>Limit) { bExceeded=true; return false; }
        if (!bIsDirectory)
        {
            uint64 Number=0;
            if (ParseGenerationName(FPaths::GetCleanFilename(Filename),Number)) Generations.Add({Number});
        }
        return true;
    }

    int32 Limit;
    int32 EntryCount=0;
    bool bExceeded=false;
    TArray<FObservedGeneration> Generations;
};

bool ScanRoot(IPlatformFile& Platform, const FString& Root, int32 EntryLimit, bool bAllowMissing,
    TArray<uint64>& OutGenerations, FString& Reason)
{
    if (!Platform.DirectoryExists(*Root))
    {
        if (Platform.FileExists(*Root)) { Reason=TEXT("Save root exists as a file."); return false; }
        if (bAllowMissing) { OutGenerations.Reset(); return true; }
        Reason=TEXT("Save root does not exist.");
        return false;
    }

    FBoundedDirectoryVisitor Visitor(EntryLimit);
    const bool bIterated=Platform.IterateDirectory(*Root,Visitor);
    if (Visitor.bExceeded) { Reason=TEXT("Save directory entry limit exceeded."); return false; }
    if (!bIterated) { Reason=TEXT("Save directory enumeration failed."); return false; }

    TArray<uint64> Candidate;
    Candidate.Reserve(Visitor.Generations.Num());
    for (const FObservedGeneration& Entry : Visitor.Generations) Candidate.Add(Entry.Number);
    Candidate.Sort();
    for (int32 I=Candidate.Num()-1; I>0; --I) if (Candidate[I]==Candidate[I-1]) Candidate.RemoveAt(I,1,EAllowShrinking::No);
    OutGenerations=MoveTemp(Candidate);
    return true;
}

bool ReadBoundedFile(const FString& Path, TArray<uint8>& Out, FString& Reason)
{
    IFileManager& FileManager=IFileManager::Get();
    const int64 Size=FileManager.FileSize(*Path);
    if (Size<0) { Reason=TEXT("Save file is missing or unreadable."); return false; }
    if (Size>MaxSaveBytes) { Reason=TEXT("Save file exceeds the byte limit."); return false; }

    TUniquePtr<FArchive> Reader(FileManager.CreateFileReader(*Path, FILEREAD_Silent));
    if (!Reader) { Reason=TEXT("Save file could not be opened."); return false; }
    const int64 OpenSize=Reader->TotalSize();
    if (OpenSize<0 || OpenSize>MaxSaveBytes || OpenSize!=Size)
    { Reason=TEXT("Save file size changed during read."); return false; }

    TArray<uint8> Candidate;
    Candidate.SetNumUninitialized(static_cast<int32>(OpenSize));
    if (OpenSize>0) Reader->Serialize(Candidate.GetData(),OpenSize);
    const bool bReadOk=!Reader->IsError() && Reader->Tell()==OpenSize;
    Reader.Reset();
    if (!bReadOk) { Reason=TEXT("Save file read was incomplete."); return false; }
    Out=MoveTemp(Candidate);
    return true;
}

bool IsGuidLeaf(const FString& Name)
{
    if (Name.Len()!=32) return false;
    for (TCHAR C : Name) if (!((C>='0'&&C<='9')||(C>='a'&&C<='f')||(C>='A'&&C<='F'))) return false;
    return true;
}
}

FSFConstructionSaveStore::FSFConstructionSaveStore()
{
    Root=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("ConstructionSandbox")));
    FPaths::NormalizeDirectoryName(Root);
    const USFConstructionSandboxSettings* Settings=GetDefault<USFConstructionSandboxSettings>();
    if (!Settings) { ConfigurationError=TEXT("Construction sandbox settings are unavailable."); return; }
    if (!Settings->Validate(ConfigurationError)) return;
    RetainGenerations=Settings->SaveGenerationsToRetain;
    DirectoryEntryLimit=Settings->SaveDirectoryEntryLimit;
    bConfigurationValid=true;
}

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
bool FSFConstructionSaveStore::SetTestRoot(const FString& CandidateRoot)
{
    FString AutomationRoot=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("ConstructionSandbox"),AutomationFolder));
    FPaths::NormalizeDirectoryName(AutomationRoot);
    AutomationRoot.RemoveFromEnd(TEXT("/"));
    AutomationRoot.RemoveFromEnd(TEXT("\\"));
    FString FullCandidate=FPaths::ConvertRelativePathToFull(CandidateRoot);
    FPaths::NormalizeDirectoryName(FullCandidate);
    FString CandidateParent=FPaths::GetPath(FullCandidate);
    FPaths::NormalizeDirectoryName(CandidateParent);
    CandidateParent.RemoveFromEnd(TEXT("/"));
    CandidateParent.RemoveFromEnd(TEXT("\\"));
    if (!CandidateParent.Equals(AutomationRoot,ESearchCase::IgnoreCase) || !IsGuidLeaf(FPaths::GetCleanFilename(FullCandidate))) return false;
    Root=MoveTemp(FullCandidate);
    return true;
}
#endif

bool FSFConstructionSaveStore::Publish(const FSFConstructionSandboxSnapshot& Snapshot, uint64& OutGeneration, FString& Reason)
{
    if (!IsInGameThread()) { Reason=TEXT("Save publication requires the game thread."); return false; }
    if (!bConfigurationValid) { Reason=ConfigurationError.IsEmpty() ? TEXT("Save settings are invalid.") : ConfigurationError; return false; }
#if !PLATFORM_WINDOWS
    Reason=TEXT("Immutable save publication is currently supported only on Windows.");
    return false;
#endif

    TArray<uint8> Encoded;
    if (!SFConstructionSaveCodec::Encode(Snapshot,Encoded)) { Reason=TEXT("Snapshot failed save-codec validation."); return false; }

    IPlatformFile& Platform=FPlatformFileManager::Get().GetPlatformFile();
    TArray<uint64> Existing;
    if (!ScanRoot(Platform,Root,DirectoryEntryLimit,true,Existing,Reason)) return false;
    const uint64 Highest=Existing.IsEmpty() ? 0 : Existing.Last();
    if (Highest==MaxGeneration) { Reason=TEXT("Save generation number is exhausted."); return false; }
    const uint64 NextGeneration=Highest+1;
    const FString FinalPath=GenerationPath(Root,NextGeneration);
    if (Platform.FileExists(*FinalPath) || Platform.DirectoryExists(*FinalPath))
    { Reason=TEXT("Next save generation already exists."); return false; }

    if (!Platform.DirectoryExists(*Root) && !Platform.CreateDirectoryTree(*Root))
    { Reason=TEXT("Save root could not be created."); return false; }

    const FString PendingName=FString::Printf(TEXT(".pending-%s.sfcs"),*FGuid::NewGuid().ToString(EGuidFormats::Digits));
    const FString PendingPath=FPaths::Combine(Root,PendingName);
    if (Platform.FileExists(*PendingPath) || Platform.DirectoryExists(*PendingPath))
    { Reason=TEXT("Fresh pending save name already exists."); return false; }

    IFileHandle* Handle=Platform.OpenWrite(*PendingPath,false,false);
    if (!Handle) { Reason=TEXT("Pending save file could not be opened."); return false; }
    const bool bWrote=Handle->Write(Encoded.GetData(),Encoded.Num());
    const bool bFlushed=bWrote && Handle->Flush(true);
    delete Handle;
    if (!bWrote || !bFlushed)
    {
        Reason=TEXT("Pending save write or full flush failed.");
        if (Platform.FileExists(*PendingPath) && !IFileManager::Get().Delete(*PendingPath,true,false,true))
            Reason+=FString::Printf(TEXT(" Pending cleanup failed; inspect: %s"),*PendingPath);
        return false;
    }

    TArray<uint8> VerificationBytes;
    FSFConstructionSandboxSnapshot VerificationSnapshot;
    FString VerificationReason;
    if (!ReadBoundedFile(PendingPath,VerificationBytes,VerificationReason)
        || !SFConstructionSaveCodec::Decode(VerificationBytes,VerificationSnapshot))
    {
        Reason=TEXT("Pending save failed readback validation.");
        if (Platform.FileExists(*PendingPath) && !IFileManager::Get().Delete(*PendingPath,true,false,true))
            Reason+=FString::Printf(TEXT(" Pending cleanup failed; inspect: %s"),*PendingPath);
        return false;
    }

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    if (TestFailure==ETestFailure::AfterFlushBeforeRename)
    {
        Reason=TEXT("Injected crash point after flush; pending save left for recovery testing.");
        return false;
    }
    if (BeforeRename) BeforeRename(FinalPath);
#endif

    // Windows MoveFileW fails when the destination exists; this preserves a
    // competing publisher's generation without replacement or deletion.
    if (!Platform.MoveFile(*FinalPath,*PendingPath))
    {
        Reason=TEXT("Non-replacing publication move failed; previous generations were preserved.");
        if (Platform.FileExists(*PendingPath) && !IFileManager::Get().Delete(*PendingPath,true,false,true))
            Reason+=FString::Printf(TEXT(" Pending cleanup failed; inspect: %s"),*PendingPath);
        return false;
    }

    OutGeneration=NextGeneration;
    Reason.Reset();

    auto DeferRetention=[&](const FString& Why)
    {
        Reason=FString::Printf(TEXT("Published generation %llu; cleanup deferred: %s"),
            static_cast<unsigned long long>(NextGeneration),*Why);
    };
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    if (TestFailure==ETestFailure::BeforeRetention)
    {
        DeferRetention(TEXT("injected retention failure."));
        return true;
    }
#endif

    TArray<uint64> PublishedGenerations;
    FString RetentionScanReason;
    if (!ScanRoot(Platform,Root,DirectoryEntryLimit,false,PublishedGenerations,RetentionScanReason))
    {
        DeferRetention(RetentionScanReason);
        return true;
    }
    const int32 DeleteCount=FMath::Max(0,PublishedGenerations.Num()-RetainGenerations);
    for (int32 I=0; I<DeleteCount; ++I)
    {
        const uint64 OldGeneration=PublishedGenerations[I];
        if (OldGeneration==NextGeneration) continue;
        const FString OldPath=GenerationPath(Root,OldGeneration);
        if (!IFileManager::Get().Delete(*OldPath,true,false,true))
        {
            DeferRetention(FString::Printf(TEXT("could not remove old generation %s."),*OldPath));
            break;
        }
    }
    return true;
}

bool FSFConstructionSaveStore::ReadLatest(FSFConstructionSandboxSnapshot& OutSnapshot, uint64& OutGeneration, FString& Reason)
{
    if (!IsInGameThread()) { Reason=TEXT("Save reading requires the game thread."); return false; }
    if (!bConfigurationValid) { Reason=ConfigurationError.IsEmpty() ? TEXT("Save settings are invalid.") : ConfigurationError; return false; }

    IPlatformFile& Platform=FPlatformFileManager::Get().GetPlatformFile();
    TArray<uint64> Generations;
    if (!ScanRoot(Platform,Root,DirectoryEntryLimit,false,Generations,Reason)) return false;
    if (Generations.IsEmpty()) { Reason=TEXT("No published save generation exists."); return false; }
    const uint64 Latest=Generations.Last();
    TArray<uint8> Bytes;
    if (!ReadBoundedFile(GenerationPath(Root,Latest),Bytes,Reason)) return false;
    FSFConstructionSandboxSnapshot Candidate;
    if (!SFConstructionSaveCodec::Decode(Bytes,Candidate)) { Reason=TEXT("Latest save generation is malformed."); return false; }
    OutSnapshot=MoveTemp(Candidate);
    OutGeneration=Latest;
    Reason.Reset();
    return true;
}
