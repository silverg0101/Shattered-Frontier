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

FVector2D USFCombatDamageWidget::MeasureVisualSize()
{
    TakeWidget();
    ForceLayoutPrepass();
    FVector2D Padding = FVector2D::ZeroVector;
    for (const UTextBlock* Text : {DamageText.Get(), CriticalNumber.Get(), CriticalText.Get()})
    {
        if (!Text || Text->GetVisibility() == ESlateVisibility::Collapsed) continue;
        const FVector2D Shadow = Text->GetShadowOffset();
        const int32 Outline = Text->GetFont().OutlineSettings.OutlineSize;
        Padding.X = FMath::Max(Padding.X, FMath::Abs(Shadow.X) + Outline);
        Padding.Y = FMath::Max(Padding.Y, FMath::Abs(Shadow.Y) + Outline);
    }
    return GetDesiredSize() + 2 * Padding;
}

float FSFCombatDamageLayout::PopScale(float Age, bool bCritical)
{
    return 1.f + (bCritical ? .3f : .16f) * FMath::Square(FMath::Max(0.f, 1.f - Age / .2f));
}

FVector2D FSFCombatDamageLayout::Position(FVector2D Anchor, float LaneOffset, float Age)
{
    const float T = FMath::Clamp(Age / Lifetime, 0.f, 1.f);
    return Anchor + FVector2D(LaneOffset, -56 * FMath::InterpEaseOut(0.f, 1.f, T, 2.f));
}

FBox2D FSFCombatDamageLayout::Bounds(FVector2D Anchor, FVector2D VisualSize, float LaneOffset, float Age, bool bCritical)
{
    const FVector2D At = Position(Anchor, LaneOffset, Age);
    const float Scale = PopScale(Age, bCritical);
    const double HalfWidth = VisualSize.X * Scale / 2;
    const double ExtraHeight = VisualSize.Y * (Scale - 1) / 2;
    return FBox2D(At - FVector2D(HalfWidth, VisualSize.Y + ExtraHeight), At + FVector2D(HalfWidth, ExtraHeight));
}

FBox2D FSFCombatDamageLayout::Envelope(FVector2D Anchor, FVector2D VisualSize, float LaneOffset, bool bCritical)
{
    FBox2D Box = Bounds(Anchor, VisualSize, LaneOffset, 0, bCritical);
    Box.Min.Y -= 56; // Reserve the whole rise, including the largest initial pop and label/shadow bounds.
    return Box;
}

float FSFCombatDamageLayout::FindLane(FVector2D Anchor, FVector2D VisualSize, float PreferredOffset,
    bool bCritical, const TArray<FBox2D>& Occupied)
{
    const auto Free = [&](float Offset)
    {
        const FBox2D Candidate = Envelope(Anchor, VisualSize, Offset, bCritical);
        for (const FBox2D& Box : Occupied) if (Candidate.Intersect(Box)) return false;
        return true;
    };
    if (Free(PreferredOffset)) return PreferredOffset;
    const double HalfWidth = VisualSize.X * PopScale(0, bCritical) / 2;
    float BestOffset = PreferredOffset;
    float BestDistance = TNumericLimits<float>::Max();
    for (const FBox2D& Box : Occupied)
        for (const double Edge : {Box.Min.X - HalfWidth - 8, Box.Max.X + HalfWidth + 8})
        {
            const float Offset = float(Edge - Anchor.X);
            const float Distance = FMath::Abs(Offset - PreferredOffset);
            if (Distance < BestDistance && Free(Offset)) { BestOffset = Offset; BestDistance = Distance; }
        }
    return BestOffset;
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
    if (Entries.Num() >= FSFCombatDamageLayout::MaxEntries) { Entries[0].Widget->RemoveFromParent(); Entries.RemoveAt(0); }
    USFCombatDamageWidget* Widget = CreateWidget<USFCombatDamageWidget>(PC, Class);
    if (!Widget) return;
    Widget->Present(Damage, bCritical);
    Widget->SetVisibility(ESlateVisibility::Hidden);
    Widget->AddToViewport(12);
    Widget->ForceLayoutPrepass();
    Widget->SetAlignmentInViewport(FVector2D(.5, 1));
    FSFCombatDamageEntry& Entry = Entries.AddDefaulted_GetRef();
    Entry.Widget = Widget; Entry.Receiver = Receiver; Entry.Anchor = HitAnchor;
    Entry.VisualSize = Widget->MeasureVisualSize(); Entry.bCritical = bCritical;
}

void USFCombatFeedbackSubsystem::Tick(float DeltaTime)
{
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC || !PC->IsLocalController()) return;
    const float Dpi = UWidgetLayoutLibrary::GetViewportScale(this);
    const FVector2D View = UWidgetLayoutLibrary::GetViewportSize(this) / Dpi;
    for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
    {
        FSFCombatDamageEntry& Entry = Entries[Index];
        Entry.Age += DeltaTime;
        if (Entry.Age >= FSFCombatDamageLayout::Lifetime) { Entry.Widget->RemoveFromParent(); Entries.RemoveAt(Index); }
    }
    TArray<FBox2D> Occupied;
    for (FSFCombatDamageEntry& Entry : Entries)
    {
        if (Entry.Receiver.IsValid() && !Entry.Receiver->IsActorBeingDestroyed()) Entry.Anchor = HeadAnchor(Entry.Receiver.Get());
        FVector2D Screen;
        bool bVisible = PC->ProjectWorldLocationToScreen(Entry.Anchor, Screen, true);
        if (!bVisible) { Entry.Widget->SetVisibility(ESlateVisibility::Collapsed); continue; }
        Screen /= Dpi;
        Entry.LaneOffset = FSFCombatDamageLayout::FindLane(Screen, Entry.VisualSize, Entry.LaneOffset, Entry.bCritical, Occupied);
        Occupied.Add(FSFCombatDamageLayout::Envelope(Screen, Entry.VisualSize, Entry.LaneOffset, Entry.bCritical));
        const FBox2D Bounds = FSFCombatDamageLayout::Bounds(Screen, Entry.VisualSize, Entry.LaneOffset, Entry.Age, Entry.bCritical);
        bVisible = Bounds.Min.X > 12 && Bounds.Max.X < View.X - 12 && Bounds.Min.Y > 12 && Bounds.Max.Y < View.Y - 12;
        Entry.Widget->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (!bVisible) continue;
        const float T = FMath::Clamp(Entry.Age / FSFCombatDamageLayout::Lifetime, 0.f, 1.f);
        Entry.Widget->SetPositionInViewport(FSFCombatDamageLayout::Position(Screen, Entry.LaneOffset, Entry.Age), false);
        const float Pop = FSFCombatDamageLayout::PopScale(Entry.Age, Entry.bCritical);
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
