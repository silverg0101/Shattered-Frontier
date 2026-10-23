#pragma once

#include "UObject/Object.h"
#include "SFWindowStack.generated.h"

class UUserWidget;

// One window to place: its size in screen pixels and whether it is the Equipment window (anchored right).
struct FSFWindowSlot
{
    FVector2D Size = FVector2D::ZeroVector;
    bool bEquipment = false;
};

// Placement rule (pure, tested): the Equipment window anchors to the right edge and every other open window lines up to its LEFT, in
// opening order, the newest nearest it. With no Equipment the windows line up left to right as one group centred on the screen.
// Returns each window's top-left in screen pixels, in the order given (oldest first).
SHATTEREDFRONTIER_API TArray<FVector2D> SFComputeWindowLayout(const TArray<FSFWindowSlot>& Windows, const FVector2D& Viewport, float Margin, float Gap);

USTRUCT()
struct FSFWindowEntry
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UUserWidget> Widget;
    UPROPERTY() bool bEquipment = false;
};

// The open menu windows of one player (owned by ASFPlayerController). Several can be open at once; Esc closes the newest (CloseTop).
// Register/Unregister are the bookkeeping; Open/Close also show or hide the widget and re-place every window.
UCLASS()
class SHATTEREDFRONTIER_API USFWindowStack : public UObject
{
    GENERATED_BODY()
public:
    static constexpr float ScreenMargin = 24.f;
    static constexpr float WindowGap = 16.f;

    // Bookkeeping only. An already open window moves to the top.
    void Register(UUserWidget* Window, bool bEquipment = false);
    bool Unregister(UUserWidget* Window);
    // Registers, shows (Visible) and re-places the windows.
    void Open(UUserWidget* Window, bool bEquipment = false);
    void Close(UUserWidget* Window);
    // Closes the newest window; false when none is open.
    bool CloseTop();
    void CloseAll();
    bool IsOpen(const UUserWidget* Window) const;
    int32 Num() const { return Windows.Num(); }
    UUserWidget* Top() const { return Windows.IsEmpty() ? nullptr : Windows.Last().Widget.Get(); }
    // Opening order, oldest first.
    TArray<UUserWidget*> GetOrder() const;
    // Re-places every open window from its current desired size (call on open/close and when a window's size may have changed).
    void Relayout();

private:
    UPROPERTY() TArray<FSFWindowEntry> Windows;
};
