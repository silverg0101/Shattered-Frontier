#pragma once

#include "Blueprint/UserWidget.h"
#include "Gathering/SFGatheringPilot.h"
#include "Gathering/SFStationRecipes.h"
#include "SFGatheringHud.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UVerticalBox;

// Short-lived skill-gain toasts, newest last; MaxVisible caps what is drawn.
struct FSFToastQueue
{
    static constexpr int32 MaxVisible = 3;
    struct FToast { FString Text; double ExpiresAt = 0; };
    TArray<FToast> Items;

    void Push(const FString& Text, double Now, double LifetimeSeconds = 3.0);
    // Drops expired toasts and returns the newest MaxVisible texts, oldest first.
    TArray<FString> Visible(double Now);
};

// A refinery recipe as the station panel shows it, read from the pilot's typed refining settings.
struct FSFRecipeView
{
    bool bValid = false;
    FString Id;
    FString Title, InputLabel, OutputLabel, InputItemPath, OutputItemPath;
    int32 InputCount = 0, OutputCount = 0;
    TArray<TPair<FString, int32>> Inputs;  // display name and quantity of every input, for the have / need rows
    float Seconds = 0.f;
};

// Local-only presentation of gathering feedback: a progress ring over the target, skill-gain toasts and the
// sawmill/forge recipe panel. It reads the owner's replicated pilot state and sends nothing to the server.
UCLASS()
class SHATTEREDFRONTIER_API USFGatheringHud : public UUserWidget
{
    GENERATED_BODY()
public:
    void Configure(USFGatheringPilotComponent* InPilot) { Pilot = InPilot; }
    // 0 at the start of a harvest, 1 when it completes; safe for a zero or unknown total.
    static float RingFraction(double RemainingSeconds, double TotalSeconds);
    static FString SkillToastText(const TCHAR* SkillName) { return FString::Printf(TEXT("%s +1"), SkillName); }
    // Station recipe from content/crafting; inputs are joined for the label, the icons show the first input and output.
    static FSFRecipeView RecipeFor(const FSFStationRecipe& Recipe, const FString& StationTitle, float RefineSeconds);
    // "have / need" and whether the need is met; the tick or cross glyph and the "short" wording follow from it.
    static FString NeedText(int32 Have, int32 Need) { return FString::Printf(TEXT("%d / %d"), Have, Need); }
    static bool NeedMet(int32 Have, int32 Need) { return Have >= Need; }

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    void ShowRecipe(const FSFRecipeView& Recipe);
    void UpdateNeeds();

    UPROPERTY(Transient) TObjectPtr<USFGatheringPilotComponent> Pilot;
    UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> ToastFrames;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ToastTexts;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> RingPercent;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> RingLabel;
    UPROPERTY(Transient) TObjectPtr<UBorder> RecipeFrame;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> RecipeTitle;
    UPROPERTY(Transient) TObjectPtr<UImage> RecipeInIcon;
    UPROPERTY(Transient) TObjectPtr<UImage> RecipeOutIcon;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> RecipeInText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> RecipeOutText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> RecipeFooter;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> NeedRows;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> NeedHaveTexts;
    UPROPERTY(Transient) TArray<TObjectPtr<UImage>> NeedGlyphs;
    UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> NeedUnderlines;
    TArray<FString> NeedNames;
    TArray<int32> NeedAmounts;

    FSlateBrush RingBrush;  // kit ring_frame, valid when bHasRingBrush
    bool bHasRingBrush = false;
    FSFToastQueue Toasts;
    uint32 LastToastOperation = 0;
    uint32 RingOperation = 0;
    double RingTotalSeconds = 0;
    float RingProgress = 0.f;
    bool bRingVisible = false;
    FVector2D RingCenter = FVector2D::ZeroVector;
    FString RecipeShownId;
};
