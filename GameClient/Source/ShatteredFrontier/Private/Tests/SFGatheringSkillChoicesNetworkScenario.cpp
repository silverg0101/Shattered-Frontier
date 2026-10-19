#include "Gathering/SFGatheringPilot.h"
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "Gathering/SFGatheringSkillsPanel.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

void USFGatheringPilotComponent::TickSkillChoicesNetworkScenario()
{
    const auto* PC=Cast<APlayerController>(GetOwner());
    if (!FParse::Param(FCommandLine::Get(),TEXT("GatheringSkillChoicesScenario")) || GetNetMode()!=NM_Client
        || !PC || !PC->IsLocalController() || !bEnabled || !bRoleAssigned || SkillChoiceScenarioStep>=8) { return; }
    const double Now=GetWorld()->GetTimeSeconds();
    if (SkillChoiceScenarioStarted==0) { SkillChoiceScenarioStarted=Now; }
    const auto Fail=[&](const TCHAR* Reason)
    {
        const int32 LoggingIndex=int32(ESFGatheringSkill::Logging),MiningIndex=int32(ESFGatheringSkill::Mining);
        const int32 LoggingLock=Skills.Locks.IsValidIndex(LoggingIndex)?int32(Skills.Locks[LoggingIndex]):-1;
        const int32 MiningLock=Skills.Locks.IsValidIndex(MiningIndex)?int32(Skills.Locks[MiningIndex]):-1;
        const double LoggingReady=SkillLockReadyAt.IsValidIndex(LoggingIndex)?SkillLockReadyAt[LoggingIndex]:-1.0;
        const double MiningReady=SkillLockReadyAt.IsValidIndex(MiningIndex)?SkillLockReadyAt[MiningIndex]:-1.0;
        const auto* GameState=GetWorld()?GetWorld()->GetGameState():nullptr;
        const double SynchronizedTime=GameState?GameState->GetServerWorldTimeSeconds():-1.0;
        const double LocalTime=GetWorld()?GetWorld()->GetTimeSeconds():-1.0;
        UE_LOG(LogTemp,Error,TEXT("GATHERING_SKILL_CHOICES_FAILED Primary=%d Step=%d Reason=%s LoggingLock=%d LoggingReadyAt=%.3f LoggingCooldown=%.3f MiningLock=%d MiningReadyAt=%.3f MiningCooldown=%.3f ServerTime=%.3f LocalTime=%.3f"),
            bPrimaryRole,SkillChoiceScenarioStep,Reason,LoggingLock,LoggingReady,GetSkillLockCooldownRemaining(ESFGatheringSkill::Logging),
            MiningLock,MiningReady,GetSkillLockCooldownRemaining(ESFGatheringSkill::Mining),SynchronizedTime,LocalTime);
        SkillChoiceScenarioStep=5; FPlatformMisc::RequestExitWithStatus(false,1);
    };
    if (Now-SkillChoiceScenarioStarted>85) { Fail(TEXT("ScenarioDeadline")); return; }
    const auto Selected=bPrimaryRole?ESFGatheringSkill::Logging:ESFGatheringSkill::Mining;
    const auto Other=bPrimaryRole?ESFGatheringSkill::Mining:ESFGatheringSkill::Logging;
    const auto SelectedLock=bPrimaryRole?ESFGatheringSkillLock::Locked:ESFGatheringSkillLock::Down;
    const auto OtherLock=bPrimaryRole?ESFGatheringSkillLock::Down:ESFGatheringSkillLock::Locked;
    if (Skills.Values.Num()!=6 || Skills.Locks.Num()!=6 || SkillLockReadyAt.Num()!=6) { Fail(TEXT("SnapshotShape")); return; }
    if (Skills.PracticeCount!=0 || Skills.GainCount!=0) { Fail(TEXT("PreferenceGrantedPractice")); return; }
    for (int32 Value:Skills.Values) { if (Value!=0) { Fail(TEXT("PreferenceChangedSkillValue")); return; } }
    int32 ForeignEndpoints=0;
    for (TActorIterator<AActor> It(GetWorld());It;++It)
    {
        TArray<USFGatheringPilotComponent*> Endpoints; It->GetComponents(Endpoints);
        for (const auto* Endpoint:Endpoints)
        {
            if (Endpoint==this) { continue; } ++ForeignEndpoints;
            const auto& Foreign=Endpoint->GetSkillSnapshot();
            if (Foreign.PracticeCount!=0 || Foreign.GainCount!=0) { Fail(TEXT("ForeignPracticeVisible")); return; }
            for (int32 Value:Foreign.Values) { if (Value!=0) { Fail(TEXT("ForeignValueVisible")); return; } }
            for (const auto Lock:Foreign.Locks)
            { if (Lock!=ESFGatheringSkillLock::Up) { Fail(TEXT("ForeignPreferenceVisible")); return; } }
            for (const double ReadyAt:Endpoint->SkillLockReadyAt)
            { if (ReadyAt!=0) { Fail(TEXT("ForeignCooldownVisible")); return; } }
        }
    }
    if (SkillChoiceScenarioStep==0)
    {
        for (const auto Lock:Skills.Locks)
        { if (Lock!=ESFGatheringSkillLock::Up) { Fail(TEXT("ExpectedDefaultUp")); return; } }
        for (const double ReadyAt:SkillLockReadyAt)
        { if (ReadyAt!=0) { Fail(TEXT("ExpectedDefaultCooldown")); return; } }
        RequestSkillLock(Selected,SelectedLock); SkillChoiceScenarioStep=1; return;
    }
    if (SkillChoiceScenarioStep==1)
    {
        if (Skills.Locks[int32(Selected)]!=SelectedLock || GetSkillLockCooldownRemaining(Selected)<=0) { return; }
        if (GetSkillLockCooldownRemaining(Selected)<2) { Fail(TEXT("RoundTripConsumedReversalWindow")); return; }
        RequestSkillLock(Selected,ESFGatheringSkillLock::Up);
        if (Skills.Locks[int32(Selected)]!=SelectedLock) { Fail(TEXT("ClientPredictedUnauthorizedReversal")); return; }
        // Bypass the local guard deliberately: the server must enforce the same cooldown.
        ServerChangeSkillLock(Selected,ESFGatheringSkillLock::Up);
        // Reliable RPC ordering makes this accepted independent-skill change an acknowledgement
        // that the earlier direct reversal reached the server before it.
        RequestSkillLock(Other,OtherLock); SkillChoiceScenarioStep=2; return;
    }
    if (SkillChoiceScenarioStep==2)
    {
        if (Skills.Locks[int32(Other)]!=OtherLock || GetSkillLockCooldownRemaining(Other)<=0) { return; }
        if (Skills.Locks[int32(Selected)]!=SelectedLock || GetSkillLockCooldownRemaining(Selected)<=0)
        { Fail(TEXT("ServerAcceptedRapidReversal")); return; }
        UE_LOG(LogTemp,Display,TEXT("GATHERING_SKILL_CHOICES_COOLDOWN Primary=%d LocalGuard=1 ServerRejected=1 IndependentSkill=1 Private=1"),bPrimaryRole);
        SkillChoiceScenarioStep=3; return;
    }
    if (SkillChoiceScenarioStep==3)
    {
        if (Skills.Locks[int32(Selected)]!=SelectedLock) { Fail(TEXT("SelectionChangedBeforeCooldown")); return; }
        if (GetSkillLockCooldownRemaining(Selected)>0) { return; }
        const auto* GS=GetWorld()->GetGameState();
        if (!GS || GS->GetServerWorldTimeSeconds()<SkillLockReadyAt[int32(Selected)]+0.25) { return; }
        if (RequestSkillLock(Selected,ESFGatheringSkillLock::Up)) { SkillChoiceScenarioStep=4; } return;
    }
    if (SkillChoiceScenarioStep==4)
    {
        if (Skills.Locks[int32(Selected)]!=ESFGatheringSkillLock::Up || GetSkillLockCooldownRemaining(Selected)<=0) { return; }
        if (Skills.Locks[int32(Other)]!=OtherLock) { Fail(TEXT("IndependentSelectionLost")); return; }
        UE_LOG(LogTemp,Display,TEXT("GATHERING_SKILL_CHOICES_CLIENT_DONE Primary=%d Selected=%d InitialLock=%d FinalLock=0 Cooldown=1 Independent=1 Practice=0 Gains=0 Values=0 Private=1 ForeignEndpoints=%d"),
            bPrimaryRole,int32(Selected),int32(SelectedLock),ForeignEndpoints);
        FString CapturePath;
        if (!FParse::Value(FCommandLine::Get(),TEXT("GatheringSkillsCapture="),CapturePath))
        { SkillChoiceScenarioStep=8; FPlatformMisc::RequestExitWithStatus(false,0); return; }
        if (!FPaths::IsRelative(CapturePath) && CapturePath.EndsWith(TEXT(".png"),ESearchCase::IgnoreCase))
        { SkillChoiceScenarioStep=5; return; }
        Fail(TEXT("InvalidCapturePath")); return;
    }
    if (SkillChoiceScenarioStep==5)
    {
        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PC,Widgets,USFGatheringSkillsPanel::StaticClass(),false);
        for (UUserWidget* Widget:Widgets)
        {
            auto* Panel=Cast<USFGatheringSkillsPanel>(Widget);
            if (Panel && Panel->GetOwningPlayer()==PC)
            {
                Panel->SetVisibility(ESlateVisibility::Visible);
                SkillChoiceCaptureAt=Now;
                SkillChoiceScenarioStep=6;
                return;
            }
        }
        return;
    }
    if (SkillChoiceScenarioStep==6)
    {
        if (Now-SkillChoiceCaptureAt<1.0) { return; }
        FString CapturePath;
        if (!FParse::Value(FCommandLine::Get(),TEXT("GatheringSkillsCapture="),CapturePath)) { Fail(TEXT("CapturePathLost")); return; }
        FScreenshotRequest::RequestScreenshot(CapturePath,true,false);
        SkillChoiceCaptureAt=Now;
        SkillChoiceScenarioStep=7;
        return;
    }
    if (SkillChoiceScenarioStep==7)
    {
        FString CapturePath;
        if (!FParse::Value(FCommandLine::Get(),TEXT("GatheringSkillsCapture="),CapturePath)) { Fail(TEXT("CapturePathLost")); return; }
        if (FScreenshotRequest::IsScreenshotRequested() || IFileManager::Get().FileSize(*CapturePath)<=0) { return; }
        UE_LOG(LogTemp,Display,TEXT("GATHERING_SKILLS_CAPTURE_DONE Path=%s"),*CapturePath);
        SkillChoiceScenarioStep=8;
        FPlatformMisc::RequestExitWithStatus(false,0);
    }
}
#endif
