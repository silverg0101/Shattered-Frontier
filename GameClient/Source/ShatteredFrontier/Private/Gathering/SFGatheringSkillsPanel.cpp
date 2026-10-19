#include "Gathering/SFGatheringSkillsPanel.h"

#include "Gathering/SFGatheringPilot.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void USFGatheringSkillsPanel::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);

    UBorder* Panel=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("GatheringSkillsPanel"));
    Panel->SetPadding(FMargin(18.f));
    Panel->SetBrushColor(FLinearColor(0.035f,0.045f,0.055f,0.97f));
    UVerticalBox* Contents=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    UScrollBox* Scroll=WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
    Scroll->AddChild(Contents);
    Panel->SetContent(Scroll);
    WidgetTree->RootWidget=Panel;

    auto AddText=[this,Contents](const TCHAR* Text,float Size,const FLinearColor& Color)
    {
        UTextBlock* Label=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Label->SetText(FText::FromString(Text)); Label->SetFontSize(Size); Label->SetColorAndOpacity(FSlateColor(Color)); Label->SetAutoWrapText(true);
        Contents->AddChildToVerticalBox(Label)->SetPadding(FMargin(2.f,3.f)); return Label;
    };
    AddText(TEXT("GATHERING SKILLS"),20.f,FLinearColor(0.9f,0.78f,0.48f,1.f));
    AddText(TEXT("Session skills | M or Esc closes"),12.f,FLinearColor(0.72f,0.76f,0.8f,1.f));

    auto AddSkill=[this,Contents](TObjectPtr<UTextBlock>& Status,TObjectPtr<UButton>* Buttons,const TCHAR* const HandlerNames[3])
    {
        Status=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Status->SetAutoWrapText(true); Status->SetFontSize(13.f);
        Contents->AddChildToVerticalBox(Status)->SetPadding(FMargin(3.f,2.f,3.f,6.f));
        UHorizontalBox* Row=WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        Contents->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f,2.f,0.f,10.f));
        const TCHAR* Labels[3]={TEXT("Train"),TEXT("Hold"),TEXT("Allow decrease")};
        for (int32 I=0;I<3;++I)
        {
            UButton* Button=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
            UTextBlock* Text=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
            Text->SetText(FText::FromString(Labels[I])); Text->SetFontSize(12.f); Text->SetJustification(ETextJustify::Center); Text->SetAutoWrapText(true);
            Button->AddChild(Text); Button->SetBackgroundColor(FLinearColor(0.18f,0.21f,0.25f,1.f));
            FScriptDelegate Click; Click.BindUFunction(this,FName(HandlerNames[I])); Button->OnClicked.Add(Click);
            Row->AddChildToHorizontalBox(Button)->SetPadding(FMargin(3.f)); Buttons[I]=Button;
        }
    };
    const TCHAR* LoggingHandlers[3]={TEXT("TrainLogging"),TEXT("HoldLogging"),TEXT("AllowLoggingDecrease")};
    const TCHAR* MiningHandlers[3]={TEXT("TrainMining"),TEXT("HoldMining"),TEXT("AllowMiningDecrease")};
    AddSkill(LoggingStatus,LoggingButtons,LoggingHandlers);
    AddSkill(MiningStatus,MiningButtons,MiningHandlers);
    AddText(TEXT("Allow decrease only donates points when gathering is at its cap; it never removes points immediately. Only Logging and Mining are playable in this session pilot."),12.f,FLinearColor(0.75f,0.78f,0.82f,1.f));
    Refresh();
}

void USFGatheringSkillsPanel::Configure(USFGatheringPilotComponent* InPilot)
{
    Pilot=InPilot;
    Refresh();
}

void USFGatheringSkillsPanel::NativeTick(const FGeometry& MyGeometry,float InDeltaTime)
{
    Super::NativeTick(MyGeometry,InDeltaTime);
    Refresh();
}

void USFGatheringSkillsPanel::SetLock(ESFGatheringSkill Skill,ESFGatheringSkillLock Lock)
{
    if (Pilot && Pilot->IsEnabled())
    {
        const auto& Locks=Pilot->GetSkillSnapshot().Locks;
        if (Locks.IsValidIndex(int32(Skill))&&Locks[int32(Skill)]!=Lock) { Pilot->RequestSkillLock(Skill,Lock); }
    }
}
void USFGatheringSkillsPanel::TrainLogging() { SetLock(ESFGatheringSkill::Logging,ESFGatheringSkillLock::Up); }
void USFGatheringSkillsPanel::HoldLogging() { SetLock(ESFGatheringSkill::Logging,ESFGatheringSkillLock::Locked); }
void USFGatheringSkillsPanel::AllowLoggingDecrease() { SetLock(ESFGatheringSkill::Logging,ESFGatheringSkillLock::Down); }
void USFGatheringSkillsPanel::TrainMining() { SetLock(ESFGatheringSkill::Mining,ESFGatheringSkillLock::Up); }
void USFGatheringSkillsPanel::HoldMining() { SetLock(ESFGatheringSkill::Mining,ESFGatheringSkillLock::Locked); }
void USFGatheringSkillsPanel::AllowMiningDecrease() { SetLock(ESFGatheringSkill::Mining,ESFGatheringSkillLock::Down); }

void USFGatheringSkillsPanel::Refresh()
{
    if (!Pilot || !Pilot->IsEnabled()) { return; }
    const FSFGatheringSkillSnapshot& Snapshot=Pilot->GetSkillSnapshot();
    auto Update=[this,&Snapshot](ESFGatheringSkill Skill,UTextBlock* Status,TObjectPtr<UButton>* Buttons,const TCHAR* Name)
    {
        const ESFGatheringSkillLock Lock=Snapshot.Locks.IsValidIndex(int32(Skill)) ? Snapshot.Locks[int32(Skill)] : ESFGatheringSkillLock::Up;
        const TCHAR* Description=Lock==ESFGatheringSkillLock::Up ? TEXT("Training: successful gathering can raise this skill.")
            : Lock==ESFGatheringSkillLock::Down ? TEXT("Allow decrease: points can be donated only at the gathering cap.")
            : TEXT("Held: practice will not change this skill.");
        const float Cooldown=FMath::Max(0.f,Pilot->GetSkillLockCooldownRemaining(Skill));
        const bool bPending=Pilot->IsSkillLockChangePending(Skill)||(!Pilot->IsSkillLockReady(Skill)&&Cooldown<=0);
        const FString Availability=bPending ? TEXT("Waiting for server") : Cooldown>0 ? FString::Printf(TEXT("Ready to change in %.1f s"),Cooldown) : TEXT("Ready to change");
        Status->SetText(FText::FromString(FString::Printf(TEXT("%s  %d\n%s\n%s"),Name,Snapshot.Get(Skill),Description,*Availability)));
        const int32 Selected=Lock==ESFGatheringSkillLock::Up ? 0 : Lock==ESFGatheringSkillLock::Locked ? 1 : 2;
        for (int32 I=0;I<3;++I)
        {
            const bool bAvailable=!bPending && Cooldown<=0 && I!=Selected;
            Buttons[I]->SetIsEnabled(I==Selected || bAvailable);
            Buttons[I]->SetBackgroundColor(I==Selected ? FLinearColor(0.42f,0.32f,0.12f,1.f) : FLinearColor(0.18f,0.21f,0.25f,1.f));
        }
    };
    Update(ESFGatheringSkill::Logging,LoggingStatus,LoggingButtons,TEXT("Logging"));
    Update(ESFGatheringSkill::Mining,MiningStatus,MiningButtons,TEXT("Mining"));
}
