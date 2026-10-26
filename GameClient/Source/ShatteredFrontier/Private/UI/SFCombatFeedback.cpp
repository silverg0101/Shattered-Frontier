#include "UI/SFCombatFeedback.h"
#include "Framework/SFHudStyle.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"

void USFCombatDamageWidget::ApplyKitStyle()
{
    // Runtime composite font objects cannot be serialized in a WBP. Fill only missing/default
    // font families after load, retaining every size/color/layout chosen in the Designer.
    for (UTextBlock* Text : {DamageText.Get(), CriticalNumber.Get(), CriticalText.Get()})
    {
        if (!Text) continue;
        const FSlateFontInfo Font = Text->GetFont();
        if (!Font.FontObject || Font.FontObject == GetDefault<UTextBlock>()->GetFont().FontObject)
        {
            FSlateFontInfo Family = SFHudStyle::RoleFont(SFHudStyle::NumeralFamily(), 30, true);
            Family.Size = Font.Size;
            Family.OutlineSettings = Font.OutlineSettings;
            Text->SetFont(Family);
        }
    }
}

void USFCombatDamageWidget::Present(int32 Damage, bool bCritical)
{
    if (DamageText) DamageText->SetText(FText::AsNumber(Damage));
    if (CriticalText)
    {
        CriticalText->SetText(FText::FromString(TEXT("CRITICAL!")));
        CriticalText->SetVisibility(bCritical ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    // A second authored numeral supplies the critical tint without replacing Designer styling.
    if (CriticalNumber)
    {
        CriticalNumber->SetText(FText::AsNumber(Damage));
        CriticalNumber->SetVisibility(bCritical ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    if (DamageText) DamageText->SetVisibility(bCritical ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

FVector USFCombatFeedbackSubsystem::HeadAnchor(const AActor* Receiver)
{
    if (!IsValid(Receiver)) return FVector::ZeroVector;
    if (const ACharacter* Character = Cast<ACharacter>(Receiver))
        if (const USkeletalMeshComponent* Mesh = Character->GetMesh(); Mesh && Mesh->DoesSocketExist(TEXT("head")))
            return Mesh->GetSocketLocation(TEXT("head")) + FVector(0, 0, 20);
    FVector Origin, Extent;
    Receiver->GetActorBounds(true, Origin, Extent);
    return Origin + FVector(0, 0, FMath::Max(Extent.Z, 30.0) + 20);
}

void USFCombatFeedbackSubsystem::Receive(AActor* Receiver, FVector HitAnchor, int32 Damage, bool bCritical)
{
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC || !PC->IsLocalController() || Damage <= 0 || HitAnchor.ContainsNaN()) return;
    FVector2D Screen;
    if (!PC->ProjectWorldLocationToScreen(HitAnchor, Screen, true)) return;
    const TSubclassOf<USFCombatDamageWidget> Class = LoadClass<USFCombatDamageWidget>(nullptr, USFCombatDamageWidget::AssetPath);
    if (!Class) return;
    // Bound both widget count and concurrent burst noise; hits remain individual, never fabricated aggregates.
    int32 SameReceiver = 0;
    for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
        if (Entries[Index].Receiver.Get() == Receiver && FVector::DistSquared(Entries[Index].Anchor, HitAnchor) < FMath::Square(300.f))
        {
            if (++SameReceiver >= 4) { Entries[Index].Widget->RemoveFromParent(); Entries.RemoveAt(Index); }
        }
    if (Entries.Num() >= 12) { Entries[0].Widget->RemoveFromParent(); Entries.RemoveAt(0); }
    USFCombatDamageWidget* Widget = CreateWidget<USFCombatDamageWidget>(PC, Class);
    if (!Widget) return;
    Widget->Present(Damage, bCritical);
    Widget->SetVisibility(ESlateVisibility::Hidden);
    Widget->AddToViewport(12);
    Widget->ForceLayoutPrepass();
    Widget->SetAlignmentInViewport(FVector2D(.5, 1));
    FSFCombatDamageEntry& Entry = Entries.AddDefaulted_GetRef();
    Entry.Widget = Widget; Entry.Receiver = Receiver; Entry.Anchor = HitAnchor;
    Entry.Lane = Sequence++ % 5; Entry.bCritical = bCritical;
}

void USFCombatFeedbackSubsystem::Tick(float DeltaTime)
{
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC || !PC->IsLocalController()) return;
    const FVector2D View = UWidgetLayoutLibrary::GetViewportSize(this);
    const float Dpi = UWidgetLayoutLibrary::GetViewportScale(this);
    for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
    {
        FSFCombatDamageEntry& Entry = Entries[Index];
        Entry.Age += DeltaTime;
        if (Entry.Age >= 1.1f) { Entry.Widget->RemoveFromParent(); Entries.RemoveAt(Index); continue; }
        if (Entry.Receiver.IsValid() && !Entry.Receiver->IsActorBeingDestroyed()) Entry.Anchor = HeadAnchor(Entry.Receiver.Get());
        FVector2D Screen;
        bool bVisible = PC->ProjectWorldLocationToScreen(Entry.Anchor, Screen, true);
        const float T = FMath::Clamp(Entry.Age / 1.1f, 0.f, 1.f);
        Screen += FVector2D((Entry.Lane - 2) * 28 * Dpi, (-56 * FMath::InterpEaseOut(0.f, 1.f, T, 2.f) - Entry.Lane * 12) * Dpi);
        const FVector2D Size = Entry.Widget->GetDesiredSize() * Dpi * 1.3f;
        bVisible &= Screen.X > Size.X / 2 + 12 * Dpi && Screen.X < View.X - Size.X / 2 - 12 * Dpi &&
            Screen.Y > Size.Y + 12 * Dpi && Screen.Y < View.Y - 12 * Dpi;
        Entry.Widget->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (!bVisible) continue;
        Entry.Widget->SetPositionInViewport(Screen, true);
        const float Pop = 1.f + (Entry.bCritical ? .3f : .16f) * FMath::Square(FMath::Max(0.f, 1.f - Entry.Age / .2f));
        Entry.Widget->SetRenderScale(FVector2D(Pop));
        Entry.Widget->SetRenderOpacity(1.f - FMath::InterpEaseIn(0.f, 1.f, FMath::Clamp((T - .45f) / .55f, 0.f, 1.f), 2.f));
    }
}

void USFCombatFeedbackSubsystem::Deinitialize()
{
    for (FSFCombatDamageEntry& Entry : Entries) if (Entry.Widget) Entry.Widget->RemoveFromParent();
    Entries.Empty();
    Super::Deinitialize();
}
