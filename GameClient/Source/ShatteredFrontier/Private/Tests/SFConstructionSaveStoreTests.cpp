#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Construction/SFConstructionSaveStore.h"
#include "Construction/SFConstructionSandboxSettings.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Serialization/Archive.h"

namespace
{
FString GenerationPathForTest(const FString& Root, uint64 Generation)
{
    return FPaths::Combine(Root,FString::Printf(TEXT("generation-%020llu.sfcs"),static_cast<unsigned long long>(Generation)));
}

bool WriteBytesForTest(const FString& Path, const TArray<uint8>& Bytes)
{
    IFileHandle* Handle=FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*Path,false,false);
    if (!Handle) return false;
    const bool bWrite=Bytes.IsEmpty() || Handle->Write(Bytes.GetData(),Bytes.Num());
    const bool bFlush=bWrite && Handle->Flush(true);
    delete Handle;
    return bWrite && bFlush;
}

bool WriteTextForTest(const FString& Path, const ANSICHAR* Text)
{
    TArray<uint8> Bytes;
    while (*Text) Bytes.Add(static_cast<uint8>(*Text++));
    return WriteBytesForTest(Path,Bytes);
}

bool ReadBytesForTest(const FString& Path, TArray<uint8>& Out)
{
    IFileManager& Files=IFileManager::Get();
    const int64 Size=Files.FileSize(*Path);
    if (Size<0 || Size>65536) return false;
    TUniquePtr<FArchive> Reader(Files.CreateFileReader(*Path,FILEREAD_Silent));
    if (!Reader || Reader->TotalSize()!=Size) return false;
    TArray<uint8> Candidate;
    Candidate.SetNumUninitialized(static_cast<int32>(Size));
    if (Size>0) Reader->Serialize(Candidate.GetData(),Size);
    const bool bOk=!Reader->IsError() && Reader->Tell()==Size;
    Reader.Reset();
    if (!bOk) return false;
    Out=MoveTemp(Candidate);
    return true;
}

class FTestCleanupVisitor final : public IPlatformFile::FDirectoryVisitor
{
public:
    virtual bool Visit(const TCHAR* Filename,bool bIsDirectory) override
    {
        (bIsDirectory ? Directories : Files).Add(Filename);
        return true;
    }
    TArray<FString> Files;
    TArray<FString> Directories;
};

bool IsOwnedFreshTestRoot(const FString& Root)
{
    FString AutomationRoot=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("ConstructionSandbox"),TEXT("Automation")));
    FPaths::NormalizeDirectoryName(AutomationRoot);
    AutomationRoot.RemoveFromEnd(TEXT("/"));
    AutomationRoot.RemoveFromEnd(TEXT("\\"));
    FString FullRoot=FPaths::ConvertRelativePathToFull(Root);
    FPaths::NormalizeDirectoryName(FullRoot);
    FString Parent=FPaths::GetPath(FullRoot);
    FPaths::NormalizeDirectoryName(Parent);
    Parent.RemoveFromEnd(TEXT("/"));
    Parent.RemoveFromEnd(TEXT("\\"));
    const FString Leaf=FPaths::GetCleanFilename(FullRoot);
    if (!Parent.Equals(AutomationRoot,ESearchCase::IgnoreCase) || Leaf.Len()!=32) return false;
    for (TCHAR C : Leaf) if (!((C>='0'&&C<='9')||(C>='a'&&C<='f')||(C>='A'&&C<='F'))) return false;
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionSaveStoreTest, "ShatteredFrontier.Construction.SaveStore.Pilot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConstructionSaveStoreTest::RunTest(const FString& Parameters)
{
#if !PLATFORM_WINDOWS
    AddWarning(TEXT("Save publication tests require the Windows non-replacing move primitive."));
    return true;
#else
    IPlatformFile& Platform=FPlatformFileManager::Get().GetPlatformFile();
    IFileManager& Files=IFileManager::Get();
    TArray<FString> OwnedRoots;
    auto MakeStore=[&](FSFConstructionSaveStore& Store)
    {
        const FString Root=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("ConstructionSandbox"),TEXT("Automation"),
            FGuid::NewGuid().ToString(EGuidFormats::Digits));
        if (!IsOwnedFreshTestRoot(Root) || Platform.FileExists(*Root) || Platform.DirectoryExists(*Root)
            || !Store.SetTestRoot(Root) || !IsOwnedFreshTestRoot(Store.Root)
            || Platform.FileExists(*Store.Root) || Platform.DirectoryExists(*Store.Root))
        {
            AddError(TEXT("Fresh test root validation failed; no filesystem operation was attempted."));
            return false;
        }
        OwnedRoots.Add(Store.Root);
        return true;
    };
    auto CleanupOwnedRoots=[&]()
    {
        for (const FString& Root : OwnedRoots)
        {
            if (!IsOwnedFreshTestRoot(Root))
            { AddError(FString::Printf(TEXT("Refusing cleanup outside a fresh GUID test child: %s"),*Root)); continue; }
            if (!Platform.DirectoryExists(*Root)) continue;
            FTestCleanupVisitor Visitor;
            if (!Platform.IterateDirectory(*Root,Visitor))
            { AddError(FString::Printf(TEXT("Could not inspect test-owned save root during cleanup: %s"),*Root)); continue; }
            for (const FString& Path : Visitor.Files)
            {
                if (!IsOwnedFreshTestRoot(Root) || !FPaths::GetPath(Path).Equals(Root,ESearchCase::IgnoreCase)
                    || !Files.Delete(*Path,true,false,true))
                    AddError(FString::Printf(TEXT("Could not remove test-owned file; inspect manually: %s"),*Path));
            }
            for (const FString& Path : Visitor.Directories)
            {
                if (!IsOwnedFreshTestRoot(Root) || !FPaths::GetPath(Path).Equals(Root,ESearchCase::IgnoreCase)
                    || !Files.DeleteDirectory(*Path,false,false))
                    AddError(FString::Printf(TEXT("Could not remove flat test-owned directory; inspect manually: %s"),*Path));
            }
            if (!Files.DeleteDirectory(*Root,false,false))
                AddError(FString::Printf(TEXT("Could not remove flat test-owned save root; inspect manually: %s"),*Root));
        }
    };

    FSFConstructionSandboxSnapshot First;
    First.SourceRevision=10;
    First.Cells.Add({FIntVector(1,2,0),0,0});
    FSFConstructionSandboxSnapshot Second;
    Second.SourceRevision=20;
    Second.Cells.Add({FIntVector(-1,0,0),1,7});
    Second.Doors.Add({FIntVector(2,2,0),1,true});

    // Missing reads are pure and leave their outputs untouched.
    {
        FSFConstructionSaveStore Store;
        if (!MakeStore(Store)) { CleanupOwnedRoots(); return false; }
        FSFConstructionSandboxSnapshot Out=First;
        uint64 Generation=777;
        FString Reason;
        TestFalse(TEXT("Read from missing root fails"),Store.ReadLatest(Out,Generation,Reason));
        TestTrue(TEXT("Missing read preserves snapshot"),Out.SourceRevision==First.SourceRevision && Out.Cells.Num()==1);
        TestEqual(TEXT("Missing read preserves generation"),Generation,uint64(777));
        TestFalse(TEXT("Missing read does not create root"),Platform.DirectoryExists(*Store.Root));
        FSFConstructionSandboxSnapshot InvalidDuplicate;
        InvalidDuplicate.Cells={{FIntVector(0,0,0),0,0},{FIntVector(0,0,0),1,1}};
        TestFalse(TEXT("Invalid duplicate snapshot publishes nothing"),Store.Publish(InvalidDuplicate,Generation,Reason));
        TestFalse(TEXT("Invalid snapshot does not create root"),Platform.DirectoryExists(*Store.Root));
    }

    // Initial and sequential publication, unknown-file retention, and normal pruning.
    {
        FSFConstructionSaveStore Store;
        if (!MakeStore(Store)) { CleanupOwnedRoots(); return false; }
        FString Reason;
        uint64 Generation=999;
        TestTrue(TEXT("Retention fixture creates its fresh test root"),Platform.CreateDirectoryTree(*Store.Root));
        TestTrue(TEXT("Unrecognized file can be added before publication"),WriteTextForTest(FPaths::Combine(Store.Root,TEXT("notes.keep")),"leave-me"));
        TestTrue(TEXT("Pending-looking file can be added before publication"),WriteTextForTest(FPaths::Combine(Store.Root,TEXT(".pending-user.sfcs")),"ignore-me"));
        TestTrue(TEXT("Noncanonical generation-like file can be added"),WriteTextForTest(FPaths::Combine(Store.Root,TEXT("generation-1.sfcs")),"not-a-generation"));
        const FString RecognizedDirectory=FPaths::Combine(Store.Root,TEXT("generation-00000000000000000123.sfcs"));
        TestTrue(TEXT("Canonical-looking directory can be created"),Platform.CreateDirectoryTree(*RecognizedDirectory));
        TArray<uint8> NotesBefore,PendingBefore,MalformedBefore;
        TestTrue(TEXT("Unknown bytes captured before publication"),ReadBytesForTest(FPaths::Combine(Store.Root,TEXT("notes.keep")),NotesBefore));
        TestTrue(TEXT("Pending bytes captured before publication"),ReadBytesForTest(FPaths::Combine(Store.Root,TEXT(".pending-user.sfcs")),PendingBefore));
        TestTrue(TEXT("Malformed-name bytes captured before publication"),ReadBytesForTest(FPaths::Combine(Store.Root,TEXT("generation-1.sfcs")),MalformedBefore));
        for (int32 I=0; I<Store.RetainGenerations+2; ++I)
        {
            FSFConstructionSandboxSnapshot Snapshot=I%2 ? Second : First;
            Snapshot.SourceRevision=100+I;
            TestTrue(TEXT("Valid generation publishes"),Store.Publish(Snapshot,Generation,Reason));
            TestEqual(TEXT("Generations increment monotonically"),Generation,static_cast<uint64>(I+1));
        }
        FSFConstructionSandboxSnapshot Latest;
        uint64 LatestGeneration=0;
        TestTrue(TEXT("Latest generation reads"),Store.ReadLatest(Latest,LatestGeneration,Reason));
        TestEqual(TEXT("Latest read selects highest generation"),LatestGeneration,static_cast<uint64>(Store.RetainGenerations+2));
        TestEqual(TEXT("Latest content matches final publication"),Latest.SourceRevision,static_cast<uint64>(99+Store.RetainGenerations+2));
        TestTrue(TEXT("Unknown file remains untouched"),Platform.FileExists(*FPaths::Combine(Store.Root,TEXT("notes.keep"))));
        TestTrue(TEXT("Pending-looking file remains untouched"),Platform.FileExists(*FPaths::Combine(Store.Root,TEXT(".pending-user.sfcs"))));
        TestTrue(TEXT("Noncanonical generation-like file remains untouched"),Platform.FileExists(*FPaths::Combine(Store.Root,TEXT("generation-1.sfcs"))));
        TestTrue(TEXT("Canonical-looking directory remains untouched"),Platform.DirectoryExists(*RecognizedDirectory));
        TArray<uint8> NotesAfter,PendingAfter,MalformedAfter;
        TestTrue(TEXT("Unknown bytes can be reread"),ReadBytesForTest(FPaths::Combine(Store.Root,TEXT("notes.keep")),NotesAfter));
        TestTrue(TEXT("Pending bytes can be reread"),ReadBytesForTest(FPaths::Combine(Store.Root,TEXT(".pending-user.sfcs")),PendingAfter));
        TestTrue(TEXT("Malformed-name bytes can be reread"),ReadBytesForTest(FPaths::Combine(Store.Root,TEXT("generation-1.sfcs")),MalformedAfter));
        TestTrue(TEXT("Publication preserves unknown file bytes"),NotesBefore==NotesAfter);
        TestTrue(TEXT("Publication preserves pending-looking bytes"),PendingBefore==PendingAfter);
        TestTrue(TEXT("Publication preserves malformed-name bytes"),MalformedBefore==MalformedAfter);
        TestFalse(TEXT("Oldest recognized generation pruned"),Platform.FileExists(*GenerationPathForTest(Store.Root,1)));
        TestFalse(TEXT("Next oldest recognized generation pruned"),Platform.FileExists(*GenerationPathForTest(Store.Root,2)));
        TestTrue(TEXT("Newest published generation remains"),Platform.FileExists(*GenerationPathForTest(Store.Root,LatestGeneration)));
        for (uint64 Retained=LatestGeneration-Store.RetainGenerations+1; Retained<=LatestGeneration; ++Retained)
            TestTrue(TEXT("Recent committed generation is retained"),Platform.FileExists(*GenerationPathForTest(Store.Root,Retained)));
    }

    // A simulated crash after durable flush leaves only an ignored pending file.
    {
        FSFConstructionSaveStore Store;
        if (!MakeStore(Store)) { CleanupOwnedRoots(); return false; }
        FString Reason;
        uint64 Generation=0;
        TestTrue(TEXT("Crash fixture base publishes"),Store.Publish(First,Generation,Reason));
        const FString CommittedPath=GenerationPathForTest(Store.Root,Generation);
        TArray<uint8> CommittedBytes;
        TestTrue(TEXT("Committed bytes captured"),ReadBytesForTest(CommittedPath,CommittedBytes));
        Store.SetTestFailure(FSFConstructionSaveStore::ETestFailure::AfterFlushBeforeRename);
        uint64 FailedGeneration=765;
        TestFalse(TEXT("Injected pre-rename crash reports failure"),Store.Publish(Second,FailedGeneration,Reason));
        TestEqual(TEXT("Failed publish preserves generation output"),FailedGeneration,uint64(765));
        FTestCleanupVisitor PendingVisitor;
        TestTrue(TEXT("Crash root can be inspected with bounded direct enumeration"),Platform.IterateDirectory(*Store.Root,PendingVisitor));
        bool bPendingExists=false;
        for (const FString& Path : PendingVisitor.Files) bPendingExists |= FPaths::GetCleanFilename(Path).StartsWith(TEXT(".pending-"));
        TestTrue(TEXT("Crash-before-publication leaves its ignored pending file"),bPendingExists);
        TArray<uint8> AfterFailureBytes;
        TestTrue(TEXT("Prior committed file remains readable"),ReadBytesForTest(CommittedPath,AfterFailureBytes));
        TestTrue(TEXT("Prior committed bytes remain identical"),CommittedBytes==AfterFailureBytes);
        FSFConstructionSandboxSnapshot Latest; uint64 LatestGeneration=0;
        TestTrue(TEXT("Pending crash does not hide prior generation"),Store.ReadLatest(Latest,LatestGeneration,Reason));
        TestEqual(TEXT("Read ignores uncommitted pending file"),LatestGeneration,uint64(1));
    }

    // Competing final creation proves the move cannot replace a winner.
    {
        FSFConstructionSaveStore Store;
        if (!MakeStore(Store)) { CleanupOwnedRoots(); return false; }
        FString Reason;
        uint64 Generation=0;
        TestTrue(TEXT("Collision fixture base publishes"),Store.Publish(First,Generation,Reason));
        const FString PriorPath=GenerationPathForTest(Store.Root,Generation);
        TArray<uint8> PriorBytes; TestTrue(TEXT("Collision fixture captures old save"),ReadBytesForTest(PriorPath,PriorBytes));
        FSFConstructionSandboxSnapshot Competitor=Second; Competitor.SourceRevision=222;
        TArray<uint8> CompetitorBytes;
        TestTrue(TEXT("Competitor snapshot encodes"),SFConstructionSaveCodec::Encode(Competitor,CompetitorBytes));
        Store.SetBeforeRename([&](const FString& FinalPath) { WriteBytesForTest(FinalPath,CompetitorBytes); });
        uint64 FailedGeneration=12345;
        TestFalse(TEXT("Competing final name makes publication fail"),Store.Publish(Second,FailedGeneration,Reason));
        TestEqual(TEXT("Collision preserves generation output"),FailedGeneration,uint64(12345));
        TArray<uint8> PriorAfter; TestTrue(TEXT("Prior save remains after collision"),ReadBytesForTest(PriorPath,PriorAfter));
        TestTrue(TEXT("Collision does not alter prior committed bytes"),PriorBytes==PriorAfter);
        FSFConstructionSandboxSnapshot Latest; uint64 LatestGeneration=0;
        TestTrue(TEXT("Competing valid winner remains readable"),Store.ReadLatest(Latest,LatestGeneration,Reason));
        TestEqual(TEXT("Competing winner generation is retained"),LatestGeneration,uint64(2));
        TestEqual(TEXT("Competing winner content is preserved"),Latest.SourceRevision,uint64(222));
    }

    // Corrupt latest data is a hard failure; the reader never falls back.
    {
        FSFConstructionSaveStore Store;
        if (!MakeStore(Store)) { CleanupOwnedRoots(); return false; }
        FString Reason; uint64 Generation=0;
        TestTrue(TEXT("Corrupt-latest base generation publishes"),Store.Publish(First,Generation,Reason));
        TestTrue(TEXT("Corrupt-latest newer generation publishes"),Store.Publish(Second,Generation,Reason));
        const FString LatestPath=GenerationPathForTest(Store.Root,2);
        TestTrue(TEXT("Corrupt latest fixture overwrites only its owned test file"),WriteTextForTest(LatestPath,"truncated"));
        FSFConstructionSandboxSnapshot Out=First; uint64 OutGeneration=345;
        TestFalse(TEXT("Malformed latest rejects without fallback"),Store.ReadLatest(Out,OutGeneration,Reason));
        TestEqual(TEXT("Corrupt read preserves snapshot revision"),Out.SourceRevision,First.SourceRevision);
        TestEqual(TEXT("Corrupt read preserves generation"),OutGeneration,uint64(345));
        TArray<uint8> Oversized; Oversized.SetNumZeroed(65537);
        TestTrue(TEXT("Oversized latest fixture write completes"),WriteBytesForTest(LatestPath,Oversized));
        TestFalse(TEXT("Oversized latest rejected before allocation"),Store.ReadLatest(Out,OutGeneration,Reason));
        TestEqual(TEXT("Oversized read preserves generation"),OutGeneration,uint64(345));
    }

    // Generation overflow and bounded directory enumeration fail before output mutation.
    {
        FSFConstructionSaveStore Store;
        if (!MakeStore(Store)) { CleanupOwnedRoots(); return false; }
        FString Reason; uint64 Generation=88;
        const FString MaxPath=FPaths::Combine(Store.Root,TEXT("generation-18446744073709551615.sfcs"));
        TestTrue(TEXT("Overflow fixture directory creates"),Platform.CreateDirectoryTree(*Store.Root));
        TestTrue(TEXT("Overflow fixture generation creates"),WriteTextForTest(MaxPath,"opaque"));
        TestFalse(TEXT("Generation overflow rejects publication"),Store.Publish(First,Generation,Reason));
        TestEqual(TEXT("Overflow preserves output generation"),Generation,uint64(88));

        FSFConstructionSaveStore Bounded;
        if (!MakeStore(Bounded)) { CleanupOwnedRoots(); return false; }
        Bounded.DirectoryEntryLimit=32;
        TestTrue(TEXT("Bounded scan fixture directory creates"),Platform.CreateDirectoryTree(*Bounded.Root));
        for (int32 I=0; I<33; ++I)
            TestTrue(TEXT("Unknown scan entry creates"),WriteTextForTest(FPaths::Combine(Bounded.Root,FString::Printf(TEXT("unknown-%02d.tmp"),I)),"x"));
        uint64 BoundedGeneration=89;
        TestFalse(TEXT("Directory scan cap rejects publication"),Bounded.Publish(First,BoundedGeneration,Reason));
        TestEqual(TEXT("Scan-cap failure preserves generation"),BoundedGeneration,uint64(89));
        TestTrue(TEXT("Unknown entries remain untouched"),Platform.FileExists(*FPaths::Combine(Bounded.Root,TEXT("unknown-00.tmp"))));
    }

    // Cleanup failure is reported as success with the published generation intact.
    {
        FSFConstructionSaveStore Store;
        if (!MakeStore(Store)) { CleanupOwnedRoots(); return false; }
        Store.SetTestFailure(FSFConstructionSaveStore::ETestFailure::BeforeRetention);
        FString Reason; uint64 Generation=456;
        TestTrue(TEXT("Retention failure does not undo publication"),Store.Publish(First,Generation,Reason));
        TestEqual(TEXT("Retention-deferred publication returns its generation"),Generation,uint64(1));
        TestTrue(TEXT("Retention-deferred reason is explicit"),Reason.Contains(TEXT("cleanup deferred")));
        TestTrue(TEXT("Published generation survives retention failure"),Platform.FileExists(*GenerationPathForTest(Store.Root,1)));
    }

    // Invalid captured settings fail closed before creating the fixed-root test child.
    {
        USFConstructionSandboxSettings* MutableSettings=const_cast<USFConstructionSandboxSettings*>(GetDefault<USFConstructionSandboxSettings>());
        const int32 SavedLimit=MutableSettings->SaveDirectoryEntryLimit;
        MutableSettings->SaveDirectoryEntryLimit=31;
        FSFConstructionSaveStore InvalidStore;
        if (!MakeStore(InvalidStore)) { CleanupOwnedRoots(); MutableSettings->SaveDirectoryEntryLimit=SavedLimit; return false; }
        MutableSettings->SaveDirectoryEntryLimit=SavedLimit;
        FString Reason; uint64 Generation=789;
        TestFalse(TEXT("Invalid captured settings reject publication"),InvalidStore.Publish(First,Generation,Reason));
        TestEqual(TEXT("Invalid settings preserve generation output"),Generation,uint64(789));
        TestFalse(TEXT("Invalid settings do not create root"),Platform.DirectoryExists(*InvalidStore.Root));
    }

    CleanupOwnedRoots();
    return true;
#endif
}

#endif
