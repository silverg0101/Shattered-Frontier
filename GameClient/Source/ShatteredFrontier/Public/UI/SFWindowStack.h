#pragma once

#include "UObject/Object.h"
#include "SFWindowStack.generated.h"

class UUserWidget;

// Placement rule (pure, tested): open windows line up left to right in opening order, the first one at the left margin, each next one a
// gap to the right of the previous. Each is centred vertically in the space above the hotbar (BottomReserve), never above the margin.
// Takes window sizes in screen pixels (oldest first) and returns each window's top-left in the same order.
SHATTEREDFRONTIER_API TArray<FVector2D> SFComputeWindowLayout(const TArray<FVector2D>& Sizes, const FVector2D& Viewport, float Margin, float Gap, float BottomReserve);

USTRUCT()
struct FSFWindowEntry
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UUserWidget> Widget;
    UPROPERTY() FVector2D Position = FVector2D::ZeroVector;  // current top-left in screen pixels (slides toward its slot)
    UPROPERTY() bool bPlaced = false;
};

// The open menu windows of one player (owned by ASFPlayerController). Several can be open at once; Esc closes the newest (CloseTop).
// Register/Unregister are the bookkeeping; Open/Close also show or hide the widget and re-place every window. A new window appears in its
// slot at once; when one closes, the windows to its right slide left (Relayout with the frame time, called from the controller's Tick).
UCLASS()
class SHATTEREDFRONTIER_API USFWindowStack : public UObject
{
    GENERATED_BODY()
public:
    static constexpr float ScreenMargin = 48.f;
    static constexpr float WindowGap = 16.f;
    static constexpr float HotbarReserve = 140.f;  // kept clear at the bottom for the hotbar
    static constexpr float SlideSpeed = 14.f;      // FMath::Vector2DInterpTo speed

    // Bookkeeping only. An already open window moves to the top.
    void Register(UUserWidget* Window);
    bool Unregister(UUserWidget* Window);
    // Registers, shows (Visible) and re-places the windows.
    void Open(UUserWidget* Window);
    void Close(UUserWidget* Window);
    // Closes the newest window; false when none is open.
    bool CloseTop();
    void CloseAll();
    bool IsOpen(const UUserWidget* Window) const;
    int32 Num() const { return Windows.Num(); }
    UUserWidget* Top() const { return Windows.IsEmpty() ? nullptr : Windows.Last().Widget.Get(); }
    // Opening order, oldest first.
    TArray<UUserWidget*> GetOrder() const;
    // Re-places every open window from its current desired size: new windows snap to their slot, placed ones slide by DeltaSeconds.
    void Relayout(float DeltaSeconds = 0.f);

private:
    UPROPERTY() TArray<FSFWindowEntry> Windows;
};
