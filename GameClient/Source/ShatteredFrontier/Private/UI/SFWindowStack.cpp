#include "UI/SFWindowStack.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/PlayerController.h"

TArray<FVector2D> SFComputeWindowLayout(const TArray<FVector2D>& Sizes, const FVector2D& Viewport, float Margin, float Gap, float BottomReserve)
{
    TArray<FVector2D> Out;
    float X = Margin;
    for (const FVector2D& Size : Sizes)
    {
        Out.Emplace(X, FMath::Max(Margin, (Viewport.Y - BottomReserve - Size.Y) * 0.5f));
        X += Size.X + Gap;
    }
    return Out;
}

void USFWindowStack::Register(UUserWidget* Window)
{
    if (!Window) { return; }
    Unregister(Window);
    Windows.AddDefaulted_GetRef().Widget = Window;
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

void USFWindowStack::Open(UUserWidget* Window)
{
    if (!Window) { return; }
    Register(Window);
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

void USFWindowStack::Relayout(float DeltaSeconds)
{
    APlayerController* Controller = Cast<APlayerController>(GetOuter());
    if (!Controller || !Controller->IsLocalController()) { return; }
    int32 Width = 0, Height = 0;
    Controller->GetViewportSize(Width, Height);
    if (Width <= 0 || Height <= 0) { return; }
    const float Scale = FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(Controller));
    TArray<FVector2D> Sizes;
    TArray<FSFWindowEntry*> Placed;
    for (FSFWindowEntry& E : Windows)
    {
        if (!E.Widget || !E.Widget->IsInViewport()) { continue; }
        E.Widget->ForceLayoutPrepass();
        Sizes.Add(E.Widget->GetDesiredSize() * Scale);  // screen pixels
        Placed.Add(&E);
    }
    const TArray<FVector2D> Targets = SFComputeWindowLayout(Sizes, FVector2D(Width, Height), ScreenMargin * Scale, WindowGap * Scale, HotbarReserve * Scale);
    for (int32 I = 0; I < Placed.Num(); ++I)
    {
        FSFWindowEntry& E = *Placed[I];
        E.Position = E.bPlaced ? FMath::Vector2DInterpTo(E.Position, Targets[I], DeltaSeconds, SlideSpeed) : Targets[I];
        E.bPlaced = true;
        E.Widget->SetPositionInViewport(E.Position, true);
    }
}
