#include "SFConstructionSaveRestartScenario.h"
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "Characters/SFE1Character.h"
#include "Construction/SFConstructionGeometry.h"
#include "Construction/SFConstructionSandbox.h"
#include "Construction/SFConstructionSandboxController.h"
#include "Construction/SFConstructionSandboxDoorProjection.h"
#include "Construction/SFConstructionSandboxState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Hash/Blake3.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Tickable.h"

namespace
{
FString AbsolutePath(const FString& Path)
{
    FString Result = FPaths::ConvertRelativePathToFull(Path);
    FPaths::NormalizeFilename(Result);
    FPaths::CollapseRelativeDirectories(Result);
    return Result;
}
// Admission is fixed to the runner's copied project. It never redirects saves.
bool IsOwnedFixture()
{
    FString Directory = AbsolutePath(FPaths::ProjectDir());
    Directory.RemoveFromEnd(TEXT("/"));
    const FString MarkerPath = Directory / TEXT(".construction-restart-fixture");
    const int64 Size = IFileManager::Get().FileSize(*MarkerPath);
    FString Marker;
    if (Size < 1 || Size > 128 || !FFileHelper::LoadFileToString(Marker,*MarkerPath)) { return false; }
    TArray<FString> Lines;
    Marker.ParseIntoArray(Lines,TEXT("\n"),false);
    FGuid Id;
    if (Lines.Num() != 3 || Lines[0] != TEXT("SF_CONSTRUCTION_RESTART_FIXTURE_V1") || !Lines[2].IsEmpty()
        || Lines[1].Len() != 32 || !FGuid::ParseExact(Lines[1],EGuidFormats::Digits,Id) || !Id.IsValid()
        || FPaths::GetCleanFilename(Directory) != TEXT("Project-")+Lines[1]) { return false; }
    return AbsolutePath(FModuleManager::Get().GetModuleFilename(TEXT("ShatteredFrontier"))).Equals(
        Directory / TEXT("Binaries/Win64/UnrealEditor-ShatteredFrontier.dll"),ESearchCase::IgnoreCase);
}

class FSaveRestartScenario final : public FTickableGameObject
{
public:
    FSaveRestartScenario(UWorld* InWorld,bool bInWriter) : World(InWorld), bWriter(bInWriter), Started(FPlatformTime::Seconds()) {}
    virtual UWorld* GetTickableGameObjectWorld() const override { return World.Get(); }
    virtual bool IsTickable() const override { return !bFinished && World.IsValid(); }
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FSaveRestartScenario,STATGROUP_Tickables); }
    virtual void Tick(float DeltaSeconds) override
    {
        if (bFinished || !World.IsValid()) { return; }
        if (FPlatformTime::Seconds()-Started > 45) { Fail(TEXT("Bounded restart input timeout")); return; }
        if (World->GetNetMode() != NM_Standalone || !World->GetAuthGameMode<ASFConstructionSandboxGameMode>())
        { Fail(TEXT("Standalone sandbox lost")); return; }
        if (!Controller.IsValid() || !Character.IsValid() || !State.IsValid())
        {
            Controller = Cast<ASFConstructionSandboxController>(World->GetFirstPlayerController());
            if (!Controller.IsValid() || !Controller->IsLocalController() || !Controller->PlayerInput) { return; }
            Character = Cast<ASFE1Character>(Controller->GetPawn());
            for (TActorIterator<ASFConstructionSandboxState> It(World.Get()); It; ++It) { State = *It; break; }
            if (!Character.IsValid() || !State.IsValid()) { return; }
        }
        if (Controller->GetPawn() != Character.Get()) { Fail(TEXT("Owning pawn changed")); return; }
        if (bAiming)
        {
            FVector Camera; FRotator View; Controller->GetPlayerViewPoint(Camera,View);
            Controller->SetControlRotation((AimTarget-Camera).Rotation());
        }
        if (GFrameCounter < NextFrame) { return; }
        if (!ReleaseKeys.IsEmpty())
        {
            for (const FKey& Key : ReleaseKeys) { Input(Key,IE_Released); }
            ReleaseKeys.Reset(); NextFrame = GFrameCounter+5; return;
        }
        if (Step == 0)
        {
            if (!Check(IsOwnedFixture(),TEXT("Copied-project marker/module admission failed"))
                || !Check(State->GetRevision() == 0 && State->GetMemberCount() == 0 && State->GetDoorCount() == 0,TEXT("Fresh state is not empty"))) { return; }
            UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_RESTART_CONTEXT Role=%s Project=\"%s\" Module=\"%s\" Saved=\"%s\""),Role(),
                *AbsolutePath(FPaths::GetProjectFilePath()),*AbsolutePath(FModuleManager::Get().GetModuleFilename(TEXT("ShatteredFrontier"))),*AbsolutePath(FPaths::ProjectSavedDir()));
            Character->GetCharacterMovement()->DisableMovement();
            Mark(TEXT("FRESH_EMPTY"));
            if (!bWriter) { Setup(FVector(-600,-600,110),FVector(0,0,0)); Press(EKeys::F9); return; }
            Setup(FVector(425,225,110),FVector(425,425,0)); Press(EKeys::One); return;
        }
        if (!bWriter)
        {
            if (!Check(Controller->GetLastLoadedGeneration() == 1 && Controller->GetLastLoadedSourceRevision() == 5
                && State->GetRevision() == 1,TEXT("F9 generation/source/live revision mismatch"))) { return; }
            Finish(); return;
        }
        switch (Step)
        {
        case 1: Press(EKeys::LeftMouseButton); break;
        case 2:
            if (!Check(State->GetMemberCount() == 1 && State->GetRevision() == 1,TEXT("Cube key placement failed"))) { return; }
            Mark(TEXT("CUBE_PLACED")); Setup(FVector(525,225,110),FVector(525,425,0)); Press(EKeys::Two); break;
        case 3: Press(EKeys::LeftMouseButton); break;
        case 4:
            if (!Check(State->GetMemberCount() == 2 && State->GetRevision() == 2,TEXT("Slab key placement failed"))) { return; }
            Mark(TEXT("SLAB_PLACED")); Setup(FVector(625,225,110),FVector(625,425,0)); Press(EKeys::Three); break;
        case 5: Press(EKeys::LeftMouseButton); break;
        case 6:
            if (!Check(State->GetMemberCount() == 3 && State->GetRevision() == 3,TEXT("Slope key placement failed"))) { return; }
            Mark(TEXT("SLOPE_PLACED")); Setup(FVector(225,625,110),FVector(425,625,0)); Press(EKeys::Four); break;
        case 7: Press(EKeys::LeftMouseButton); break;
        case 8:
            if (!Check(DoorMatches(false) && State->GetRevision() == 4,TEXT("Door key placement failed"))) { return; }
            Aim(FVector(402.5,625,100)); SettlingAt = FPlatformTime::Seconds(); Advance(); break;
        case 9:
            if (!ViewTargetsDoor())
            {
                ConsecutiveAim = 0;
                if (FPlatformTime::Seconds()-SettlingAt > 3) { Fail(TEXT("Actual view trace could not acquire owned door")); }
                return;
            }
            if (++ConsecutiveAim < 3) { return; }
            Press(EKeys::E); break;
        case 10:
            if (!Check(DoorMatches(true) && State->GetRevision() == 5,TEXT("E failed to open door"))
                || !Check(IsOwnedFixture(),TEXT("Copied-project admission lost before F5"))
                || !Check(ValidateAssembly(),TEXT("Writer canonical assembly or physics mismatch before F5"))) { return; }
            Mark(TEXT("OPEN_ASSEMBLY_VERIFIED")); Press(EKeys::F5); break;
        case 11:
            if (!Check(Controller->GetLastPublishedGeneration() == 1,TEXT("F5 did not publish first generation"))) { return; }
            Finish(); break;
        }
    }
private:
    const TCHAR* Role() const { return bWriter ? TEXT("Writer") : TEXT("Reader"); }
    void Input(FKey Key,EInputEvent Event)
    { Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key,Event,Event == IE_Pressed ? 1.f : 0.f,1,FInputDeviceId::CreateFromInternalId(0))); }
    void Press(FKey Key) { Input(Key,IE_Pressed); ReleaseKeys.Add(Key); ++Step; NextFrame = GFrameCounter+1; }
    void Advance() { ++Step; NextFrame = GFrameCounter+5; }
    void Aim(FVector Point) { bAiming = true; AimTarget = Point; Controller->SetControlRotation((Point-Character->GetActorLocation()).Rotation()); }
    void Setup(FVector Position,FVector Target)
    {
        Character->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);
        Aim(Target);
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_RESTART_SETUP Role=%s Position=%s"),Role(),*Position.ToString());
    }
    bool DoorMatches(bool bOpen) const
    {
        const auto* Door = State->FindDoor(FIntVector(8,12,0));
        return State->GetDoorCount() == 1 && Door && Door->YawQuarterTurns == 0 && Door->bOpen == bOpen
            && Door->Projection.IsValid() && Door->Projection->IsProjectionIntact();
    }
    bool ViewTargetsDoor() const
    {
        FVector Camera; FRotator View; Controller->GetPlayerViewPoint(Camera,View);
        FHitResult Hit; FIntVector Origin;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(RestartDoorAim),false,Character.Get());
        return World->LineTraceSingleByChannel(Hit,Camera,Camera+View.Vector()*State->GetReachCentimetres(),ECC_Visibility,Query)
            && State->FindDoorOrigin(Hit.GetActor(),Origin) && Origin == FIntVector(8,12,0);
    }
    bool ValidateAssembly()
    {
        FSFConstructionSandboxSnapshot Actual,Expected; FString Reason;
        if (!State->ExportSnapshot(Controller.Get(),State->GetRevision(),Actual,Reason) || !DoorMatches(true)) { return false; }
        for (int32 Shape = 0; Shape < 3; ++Shape)
        {
            FSFConstructionSavedCell Cell;
            Cell.Cell = FIntVector(8+Shape*2,8,0); Cell.Shape = Shape; Cell.Rotation = SFConstructionGeometry::IdentityRotation();
            Expected.Cells.Add(Cell);
        }
        FSFConstructionSavedDoor Door; Door.OriginCell = FIntVector(8,12,0); Door.bOpen = true; Expected.Doors.Add(Door);
        Actual.SourceRevision = 0;
        TArray<uint8> ActualBytes,ExpectedBytes;
        if (!SFConstructionSaveCodec::Encode(Actual,ActualBytes) || !SFConstructionSaveCodec::Encode(Expected,ExpectedBytes)
            || ActualBytes != ExpectedBytes) { return false; }
        ContentDigest = LexToString(FBlake3::HashBuffer(ActualBytes.GetData(),ActualBytes.Num()));
        for (const auto& Cell : Expected.Cells)
        {
            ASFConstructionSandboxPreview* Projection = nullptr;
            for (TActorIterator<ASFConstructionSandboxPreview> It(World.Get()); It; ++It)
            { FIntVector Coordinate; if (State->FindCell(*It,Coordinate) && Coordinate == Cell.Cell) { Projection = *It; break; } }
            const FVector Corner = ASFConstructionSandboxState::CellCorner(Cell.Cell);
            if (!Projection || !Projection->IsProjectionIntact(Cell.Shape,Cell.Rotation,Projection->GetGeneration(),Corner)) { return false; }
            FCollisionQueryParams Query(SCENE_QUERY_STAT(RestartCanonicalSimple),false,Character.Get());
            FHitResult Hit;
            if (!World->LineTraceSingleByChannel(Hit,Corner+FVector(37,-20,12),Corner+FVector(37,70,12),ECC_Visibility,Query)
                || Hit.GetActor() != Projection) { return false; }
            const FVector Empty = Cell.Shape == 0 ? FVector(60,0,25) : FVector(12,0,37);
            if (World->LineTraceSingleByChannel(Hit,Corner+Empty+FVector(0,-20,0),Corner+Empty+FVector(0,70,0),ECC_Visibility,Query)) { return false; }
        }
        const auto* LoadedDoor = State->FindDoor(FIntVector(8,12,0));
        const FTransform Transform = LoadedDoor->Projection->GetActorTransform();
        FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(RestartOpenDoorSimple),false,Character.Get());
        if (World->SweepSingleByChannel(Hit,Transform.TransformPosition(FVector(52.5,-100,100)),
            Transform.TransformPosition(FVector(52.5,150,100)),FQuat::Identity,ECC_Visibility,FCollisionShape::MakeCapsule(34,88),Query)) { return false; }
        return World->LineTraceSingleByChannel(Hit,Transform.TransformPosition(FVector(2.5,-20,100)),
            Transform.TransformPosition(FVector(2.5,50,100)),ECC_Visibility,Query) && Hit.GetActor() == LoadedDoor->Projection.Get();
    }
    void Finish()
    {
        if (!Check(ValidateAssembly(),TEXT("Final canonical content/support/simple physics mismatch"))) { return; }
        bFinished = true;
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_RESTART_SUCCESS Role=%s Generation=1 SourceRevision=5 LiveRevision=%llu Cells=3 Doors=1 ContentDigest=%s SimpleCollision=1"),Role(),State->GetRevision(),*ContentDigest);
        FPlatformMisc::RequestExitWithStatus(false,0);
    }
    void Mark(const TCHAR* Name) const
    { UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_RESTART_STEP Role=%s Name=%s Cells=%d Doors=%d LiveRevision=%llu"),Role(),Name,State->GetMemberCount(),State->GetDoorCount(),State->GetRevision()); }
    bool Check(bool bCondition,const TCHAR* Reason) { if (!bCondition) { Fail(Reason); } return bCondition; }
    void Fail(const TCHAR* Reason)
    {
        bFinished = true;
        if (Controller.IsValid()) { for (const FKey& Key : ReleaseKeys) { Input(Key,IE_Released); } }
        UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_RESTART_FAILED Role=%s Step=%d Reason=%s AuthorityReason=%s"),Role(),Step,Reason,
            Controller.IsValid() ? *Controller->GetLastConstructionResult() : TEXT("unavailable"));
        FPlatformMisc::RequestExitWithStatus(false,1);
    }
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ASFConstructionSandboxController> Controller;
    TWeakObjectPtr<ASFE1Character> Character;
    TWeakObjectPtr<ASFConstructionSandboxState> State;
    bool bWriter, bFinished = false, bAiming = false;
    double Started, SettlingAt = 0;
    uint64 NextFrame = 0;
    int32 Step = 0, ConsecutiveAim = 0;
    FVector AimTarget = FVector::ZeroVector;
    FString ContentDigest;
    TArray<FKey> ReleaseKeys;
};
TUniquePtr<FSaveRestartScenario> RestartScenario;
}

void StartConstructionSaveRestartScenario(UWorld* World)
{
    const bool bWriter = FParse::Param(FCommandLine::Get(),TEXT("ConstructionSaveRestartWriter"));
    const bool bReader = FParse::Param(FCommandLine::Get(),TEXT("ConstructionSaveRestartReader"));
    if ((!bWriter && !bReader) || !World || World->GetNetMode() != NM_Standalone
        || !World->GetAuthGameMode<ASFConstructionSandboxGameMode>() || RestartScenario) { return; }
    if (bWriter == bReader || FParse::Param(FCommandLine::Get(),TEXT("ConstructionInputScenario"))
        || FParse::Param(FCommandLine::Get(),TEXT("ConstructionDoorInputScenario")) || !IsOwnedFixture())
    {
        UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_RESTART_FAILED Reason=ConflictingFlagsOrCopiedProjectAdmission"));
        FPlatformMisc::RequestExitWithStatus(false,1); return;
    }
    RestartScenario = MakeUnique<FSaveRestartScenario>(World,bWriter);
}
#endif
