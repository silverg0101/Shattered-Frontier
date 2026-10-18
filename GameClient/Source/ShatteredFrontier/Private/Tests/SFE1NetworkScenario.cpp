#include "Tests/SFE1NetworkScenario.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Characters/SFE1Character.h"
#include "Interaction/SFE1InteractionTarget.h"
#include "Interaction/SFE1PlayerController.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace
{
struct FScenario
{
    TWeakObjectPtr<UWorld> World;
    FString Role;
    double Started = FPlatformTime::Seconds();
    double NextSample = 0.0;
    double Arrived = -1.0;
    double Requested = -1.0;
    bool bJumped = false;
    bool bReportedEmulation = false;
    FString CaptureDir;
    double CaptureReady = -1.0;
    bool bArrivalCapture = false;
    bool bProbeCapture = false;
    bool bDepartedCapture = false;
    double NextCaptureAllowed = 0.0;
};

void RequestFrame(const TSharedRef<FScenario>& State, const TCHAR* Phase)
{
    const FString Filename = FPaths::Combine(State->CaptureDir, State->Role + TEXT("-") + Phase + TEXT(".png"));
    FScreenshotRequest::RequestScreenshot(Filename, false, false);
    State->NextCaptureAllowed = FPlatformTime::Seconds() + 0.5;
    UE_LOG(LogTemp, Display, TEXT("E1_SCENARIO_CAPTURE_REQUEST Role=%s Phase=%s"), *State->Role, Phase);
}

void Step(const TSharedRef<FScenario>& State)
{
    UWorld* World = State->World.Get();
    if (!World || World->bIsTearingDown) { return; }
    const double Now = FPlatformTime::Seconds();
    const double Elapsed = Now - State->Started;
#if DO_ENABLE_NET_TEST
    if (!State->bReportedEmulation)
    {
        if (const UNetDriver* Driver = World->GetNetDriver())
        {
            const FPacketSimulationSettings& Settings = Driver->PacketSimulationSettings;
            UE_LOG(LogTemp, Display, TEXT("E1_SCENARIO_EMULATION Role=%s PktLag=%d PktLagVariance=%d PktLoss=%d PktOrder=%d PktDup=%d IncomingLagMin=%d IncomingLagMax=%d"),
                *State->Role, Settings.PktLag, Settings.PktLagVariance, Settings.PktLoss,
                Settings.PktOrder, Settings.PktDup, Settings.PktIncomingLagMin, Settings.PktIncomingLagMax);
            State->bReportedEmulation = true;
        }
    }
#endif
    if (Elapsed >= 30.0)
    {
        UE_LOG(LogTemp, Display, TEXT("E1_SCENARIO_DONE Role=%s"), *State->Role);
        return;
    }
    int32 CharacterCount = 0;
    ASFE1Character* OwnedPawn = nullptr;
    ASFE1PlayerController* LocalController = nullptr;
    for (TActorIterator<ASFE1Character> It(World); It; ++It)
    {
        ASFE1Character* Pawn = *It;
        const APlayerState* PlayerState = Pawn->GetPlayerState();
        const int32 PlayerId = PlayerState ? PlayerState->GetPlayerId() : -1;
        if (PlayerId < 0) { continue; }
        ++CharacterCount;
        if (Pawn->IsLocallyControlled())
        {
            ASFE1PlayerController* Controller = Cast<ASFE1PlayerController>(Pawn->GetController());
            if (Controller && Controller->IsLocalController())
            {
                OwnedPawn = Pawn;
                LocalController = Controller;
            }
        }
        if (Now >= State->NextSample)
        {
            const FVector Position = Pawn->GetActorLocation();
            UE_LOG(LogTemp, Display, TEXT("E1_SCENARIO_SAMPLE Role=%s Pawn=%s PlayerId=%d OwnerLocal=%d Time=%.3f X=%.3f Y=%.3f Z=%.3f Grounded=%d"),
                *State->Role, *Pawn->GetName(), PlayerId, Pawn->IsLocallyControlled() ? 1 : 0,
                Elapsed, Position.X, Position.Y, Position.Z, Pawn->GetCharacterMovement()->IsMovingOnGround() ? 1 : 0);
        }
    }
    if (Now >= State->NextSample) { State->NextSample = Now + 0.2; }
    const bool bClientReady = CharacterCount >= 2 && OwnedPawn && LocalController;
    if (!State->CaptureDir.IsEmpty() && bClientReady)
    {
        if (State->CaptureReady < 0.0) { State->CaptureReady = Now; }
        const double ReadyElapsed = Now - State->CaptureReady;
        if (!State->bArrivalCapture && ReadyElapsed >= 1.0)
        {
            RequestFrame(State, TEXT("Arrival"));
            State->bArrivalCapture = true;
        }
        // Observer phases are timed context captures, not evidence of receiving an intent RPC.
        const bool bProbeTime = State->Role == TEXT("Driver")
            ? State->Requested >= 0.0 && Now - State->Requested >= 0.6 : ReadyElapsed >= 2.3;
        if (State->bArrivalCapture && !State->bProbeCapture && bProbeTime && Now >= State->NextCaptureAllowed)
        {
            RequestFrame(State, TEXT("Probe"));
            State->bProbeCapture = true;
        }
        const bool bDepartedTime = State->Role == TEXT("Driver")
            ? State->Requested >= 0.0 && Now - State->Requested >= 5.5 : ReadyElapsed >= 8.0;
        if (State->bProbeCapture && !State->bDepartedCapture && bDepartedTime && Now >= State->NextCaptureAllowed
            && OwnedPawn->GetCharacterMovement()->IsMovingOnGround())
        {
            RequestFrame(State, TEXT("Departed"));
            State->bDepartedCapture = true;
        }
    }
    const bool bCaptureWarm = State->CaptureDir.IsEmpty() || State->bArrivalCapture;
    if (State->Role == TEXT("Driver") && bClientReady && bCaptureWarm)
    {
        TActorIterator<ASFE1InteractionTarget> ProbeIterator(World);
        ASFE1InteractionTarget* Probe = ProbeIterator ? *ProbeIterator : nullptr;
        if (Probe && State->Requested < 0.0)
        {
            FVector Direction = Probe->GetInteractionAnchor() - OwnedPawn->GetActorLocation();
            Direction.Z = 0.0;
            if (FVector::Dist(Probe->GetInteractionAnchor(), OwnedPawn->GetActorLocation()) > 200.0)
            {
                OwnedPawn->AddMovementInput(Direction.GetSafeNormal(), 1.0f);
            }
            else if (State->Arrived < 0.0) { State->Arrived = Now; }
            else if (Now - State->Arrived >= 0.3)
            {
                UE_LOG(LogTemp, Display, TEXT("E1_SCENARIO_DRIVER Pawn=%s PlayerId=%d Action=Interact"), *OwnedPawn->GetName(), OwnedPawn->GetPlayerState()->GetPlayerId());
                LocalController->InteractWithSelectedTarget();
                State->Requested = Now;
            }
        }
        if (State->Requested >= 0.0 && Now - State->Requested >= 0.8 && Now - State->Requested < 3.0)
        {
            OwnedPawn->AddMovementInput(FVector(0.0, -1.0, 0.0), 1.0f);
            if (!State->bJumped && Now - State->Requested > 1.2)
            {
                OwnedPawn->Jump();
                State->bJumped = true;
            }
        }
        if (State->bJumped && Now - State->Requested > 1.5) { OwnedPawn->StopJumping(); }
    }
    World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([State]() { Step(State); }));
}
}

void StartE1NetworkScenario(UWorld* World)
{
    FString Role;
    if (!World || !FParse::Value(FCommandLine::Get(), TEXT("E1ProbeRole="), Role)) { return; }
    if (Role != TEXT("Server") && Role != TEXT("Driver") && Role != TEXT("Observer")) { return; }
    if ((Role == TEXT("Server")) != (World->GetNetMode() == NM_DedicatedServer)) { return; }
    if (Role != TEXT("Server") && World->GetNetMode() != NM_Client) { return; }
    static TArray<TWeakObjectPtr<UWorld>> StartedWorlds;
    StartedWorlds.RemoveAll([](const TWeakObjectPtr<UWorld>& Item) { return !Item.IsValid(); });
    if (StartedWorlds.Contains(World)) { return; }
    StartedWorlds.Add(World);
    TSharedRef<FScenario> State = MakeShared<FScenario>();
    State->World = World;
    State->Role = Role;
    FString CaptureDir;
    if (Role != TEXT("Server") && FParse::Value(FCommandLine::Get(), TEXT("E1CaptureDir="), CaptureDir))
    {
        if (FPaths::IsRelative(CaptureDir) || !FPaths::DirectoryExists(CaptureDir))
        {
            UE_LOG(LogTemp, Error, TEXT("E1 capture requires an existing absolute local output directory"));
            return;
        }
        State->CaptureDir = CaptureDir;
    }
    UE_LOG(LogTemp, Display, TEXT("E1_SCENARIO_START Role=%s DurationSeconds=30"), *Role);
    World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([State]() { Step(State); }));
}
#endif
