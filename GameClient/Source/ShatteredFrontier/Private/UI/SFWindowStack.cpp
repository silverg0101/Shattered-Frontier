#include "UI/SFWindowStack.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/PlayerController.h"

TArray<FVector2D> SFComputeWindowLayout(const TArray<FSFWindowSlot>& Windows, const FVector2D& Viewport, float Margin, float Gap)
{
    TArray<FVector2D> Out;
    Out.Init(FVector2D::ZeroVector, Windows.Num());
    const int32 Equipment = Windows.IndexOfByPredicate([](const FSFWindowSlot& W) { return W.bEquipment; });
    float Total = 0.f; int32 Count = 0;
    for (int32 I = 0; I < Windows.Num(); ++I) { if (I != Equipment) { Total += Windows[I].Size.X; ++Count; } }
    Total += Gap * FMath::Max(0, Count - 1);
    float X;
    if (Equipment != INDEX_NONE)
    {
        Out[Equipment].X = Viewport.X - Margin - Windows[Equipment].Size.X;
        X = FMath::Max(Margin, Out[Equipment].X - Gap - Total);  // the group ends one gap left of Equipment
    }
    else { X = FMath::Max(Margin, (Viewport.X - Total) * 0.5f); }
    for (int32 I = 0; I < Windows.Num(); ++I)
    {
        if (I != Equipment) { Out[I].X = X; X += Windows[I].Size.X + Gap; }
        Out[I].Y = FMath::Max(0.f, (Viewport.Y - Windows[I].Size.Y) * 0.5f);
    }
    return Out;
}

void USFWindowStack::Register(UUserWidget* Window, bool bEquipment)
{
    if (!Window) { return; }
    Unregister(Window);
    FSFWindowEntry& Entry = Windows.AddDefaulted_GetRef();
    Entry.Widget = Window; Entry.bEquipment = bEquipment;
}

bool USFWindowStack::Unregister(UUserWidget* Window)
{
    return Windows.RemoveAll([Window](const FSFWindowEntry& E) { return E.Widget == Window; }) > 0;
}

bool USFWindowStack::IsOpen(const UUserWidget* Window) const
{
    return Window && Windows.ContainsByPredicate([Window](const FSFWindowEntry& E) { return E.Widget == Window; });
}

TArray<UUserWidget*> USFWindowStack::GetOrder() const
{
    TArray<UUserWidget*> Out;
    for (const FSFWindowEntry& E : Windows) { Out.Add(E.Widget); }
    return Out;
}

void USFWindowStack::Open(UUserWidget* Window, bool bEquipment)
{
    if (!Window) { return; }
    Register(Window, bEquipment);
    Window->SetVisibility(ESlateVisibility::Visible);
    Relayout();
}

void USFWindowStack::Close(UUserWidget* Window)
{
    if (!Window) { return; }
    Unregister(Window);
    Window->SetVisibility(ESlateVisibility::Collapsed);
    Relayout();
}

bool USFWindowStack::CloseTop()
{
    UUserWidget* Window = Top();
    if (!Window) { return false; }
    Close(Window);
    return true;
}

void USFWindowStack::CloseAll()
{
    for (const FSFWindowEntry& E : Windows) { if (E.Widget) { E.Widget->SetVisibility(ESlateVisibility::Collapsed); } }
    Windows.Reset();
}

void USFWindowStack::Relayout()
{
    APlayerController* Controller = Cast<APlayerController>(GetOuter());
    if (!Controller || !Controller->IsLocalController()) { return; }
    int32 Width = 0, Height = 0;
    Controller->GetViewportSize(Width, Height);
    if (Width <= 0 || Height <= 0) { return; }
    const float Scale = FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(Controller));
    TArray<FSFWindowSlot> Slots;
    TArray<UUserWidget*> Placed;
    for (const FSFWindowEntry& E : Windows)
    {
        if (!E.Widget || !E.Widget->IsInViewport()) { continue; }
        E.Widget->ForceLayoutPrepass();
        FSFWindowSlot& Slot = Slots.AddDefaulted_GetRef();
        Slot.Size = E.Widget->GetDesiredSize() * Scale;  // screen pixels
        Slot.bEquipment = E.bEquipment;
        Placed.Add(E.Widget);
    }
    const TArray<FVector2D> Positions = SFComputeWindowLayout(Slots, FVector2D(Width, Height), ScreenMargin * Scale, WindowGap * Scale);
    for (int32 I = 0; I < Placed.Num(); ++I) { Placed[I]->SetPositionInViewport(Positions[I], true); }
}
