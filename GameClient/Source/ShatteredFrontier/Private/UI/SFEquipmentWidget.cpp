#include "UI/SFEquipmentWidget.h"
#include "UI/SFItemTooltip.h"
#include "Characters/SFE1Character.h"

#include "Components/Image.h"
#include "Components/PointLightComponent.h"
#include "Components/ProgressBar.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Exchange/SFExchangeComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "Gathering/SFGatheringPilot.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/SFInventory.h"

// ---- preview studio

const FVector ASFEquipmentPreviewStudio::StudioLocation(0.0, 0.0, -200000.0);

namespace
{
// The studio's own lighting channel: its lights reach only the copies, and the world's sun and lamps (channel 0) never reach them.
void StudioChannel(FLightingChannels& Channels) { Channels.bChannel0 = false; Channels.bChannel1 = true; Channels.bChannel2 = false; }
}

ASFEquipmentPreviewStudio::ASFEquipmentPreviewStudio()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;
    SetCanBeDamaged(false);
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Turntable = CreateDefaultSubobject<USceneComponent>(TEXT("Turntable"));
    Turntable->SetupAttachment(RootComponent);
    Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
    Capture->SetupAttachment(RootComponent);
    Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    Capture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;  // linear colour, alpha = 1 - coverage (M_EquipmentPreview inverts it)
    Capture->bCaptureEveryFrame = true;
    Capture->bCaptureOnMovement = false;
    Capture->bAlwaysPersistRenderingState = false;  // no view state: pre-exposure stays 1 (no eye adaptation drifting the brightness between openings)
    Capture->FOVAngle = 30.f;
    Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
    Capture->PostProcessSettings.AutoExposureMethod = AEM_Manual;
    Capture->PostProcessSettings.bOverride_AutoExposureBias = true;
    Capture->PostProcessSettings.AutoExposureBias = 0.f;
    // Nothing from the world: no sky, sun, fog, clouds, GI or screen-space effects; only the studio's own lights.
    for (const TCHAR* Flag : {TEXT("Atmosphere"), TEXT("Cloud"), TEXT("Fog"), TEXT("VolumetricFog"), TEXT("SkyLighting"), TEXT("DirectionalLights"),
        TEXT("AmbientOcclusion"), TEXT("DistanceFieldAO"), TEXT("ScreenSpaceReflections"), TEXT("GlobalIllumination"), TEXT("LumenGlobalIllumination"),
        TEXT("LumenReflections"), TEXT("ReflectionEnvironment"), TEXT("AmbientCubemap"), TEXT("IndirectLightingCache"), TEXT("LightShafts"), TEXT("VolumetricLightmap"), TEXT("MotionBlur")})
    {
        const int32 Index = FEngineShowFlags::FindIndexByName(Flag);
        if (Index != INDEX_NONE) { Capture->ShowFlags.SetSingleFlag(Index, false); }
    }
    // Key (warm), fill (cool), rim (behind): fixed intensities, so the preview is lit the same every time.
    auto Light = [this](const TCHAR* Name, float Candelas, const FLinearColor& Color, bool bShadows)
    {
        UPointLightComponent* Lamp = CreateDefaultSubobject<UPointLightComponent>(Name);
        Lamp->SetupAttachment(RootComponent);
        Lamp->IntensityUnits = ELightUnits::Candelas;
        Lamp->Intensity = Candelas;
        Lamp->LightColor = Color.ToFColor(true);
        Lamp->AttenuationRadius = 2000.f;
        Lamp->CastShadows = bShadows;
        StudioChannel(Lamp->LightingChannels);
        Lights.Add(Lamp);
    };
    Light(TEXT("Key"), 30.f, FLinearColor(1.f, 0.93f, 0.82f), false);  // ponytail: no studio shadows; a scene capture's virtual shadow pages came out different per opening, so the key light cast none
    Light(TEXT("Fill"), 10.f, FLinearColor(0.78f, 0.86f, 1.f), false);
    Light(TEXT("Rim"), 22.f, FLinearColor(1.f, 0.95f, 0.85f), false);
}

void ASFEquipmentPreviewStudio::CopyFrom(const APawn* Pawn)
{
    UnbindFace();
    for (UPrimitiveComponent* Old : Copies) { if (Old) { Old->DestroyComponent(); } }
    Copies.Reset();
    if (Figure) { Figure->Destroy(); Figure = nullptr; }
    if (!Target)
    {
        Target = LoadObject<UTextureRenderTarget2D>(nullptr, TargetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
        if (!Target)
        {
            Target = NewObject<UTextureRenderTarget2D>(this);
            Target->RenderTargetFormat = RTF_RGBA16f;
            Target->ClearColor = FLinearColor::Transparent;
            Target->InitAutoFormat(500, 700);
        }
        Capture->TextureTarget = Target;
        Capture->ShowOnlyActors.Reset();
        Capture->ShowOnlyActors.Add(this);
    }
    const ACharacter* Character = Cast<ACharacter>(Pawn);
    USkeletalMeshComponent* Source = Character ? Character->GetMesh() : Pawn ? Pawn->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
    if (!Source || !Source->GetSkeletalMeshAsset()) { return; }

    // One copy per mesh: same asset, same materials, same socket and offset; skeletal copies follow the original's live pose.
    auto Copy = [this](UMeshComponent* From, USceneComponent* Parent, FName Socket, const FTransform& Relative) -> UMeshComponent*
    {
        UMeshComponent* To = nullptr;
        if (USkeletalMeshComponent* Skeletal = Cast<USkeletalMeshComponent>(From))
        {
            USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(this);
            Mesh->SetSkeletalMeshAsset(Skeletal->GetSkeletalMeshAsset());
            Mesh->SetLeaderPoseComponent(Skeletal);
            To = Mesh;
        }
        else if (UStaticMeshComponent* Static = Cast<UStaticMeshComponent>(From))
        {
            UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
            Mesh->SetStaticMesh(Static->GetStaticMesh());
            To = Mesh;
        }
        if (!To) { return nullptr; }
        To->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        To->SetupAttachment(Parent, Socket);
        To->SetRelativeTransform(Relative);
        StudioChannel(To->LightingChannels);
        To->RegisterComponent();
        for (int32 Index = 0; Index < From->GetNumMaterials(); ++Index) { To->SetMaterial(Index, From->GetMaterial(Index)); }
        Copies.Add(To);
        return To;
    };
    FTransform BodyTransform = Source->GetRelativeTransform();
    BodyTransform.SetLocation(FVector::ZeroVector);  // feet on the turntable
    UMeshComponent* Body = Copy(Source, Turntable, NAME_None, BodyTransform);
    if (!Body) { return; }
    for (USceneComponent* Child : Source->GetAttachChildren())  // what is attached to the body: equipped-item visuals, modular parts
    {
        UMeshComponent* Part = Cast<UMeshComponent>(Child);
        if (Part && Part->IsVisible() && !Part->bHiddenInGame) { Copy(Part, Body, Part->GetAttachSocketName(), Part->GetRelativeTransform()); }
    }

    // A player MetaHuman: its own copy stands on the turntable, posed from the same mannequin; the mannequin copy stays for framing.
    if (const ASFE1Character* Player = Cast<ASFE1Character>(Pawn))
    {
        FaceSource = Player;
        FaceHandle = Player->OnFaceChanged.AddWeakLambda(this, [this, Player] { CopyFrom(Player); });  // a new face: rebuild the copy
    }
    if (const ASFE1Character* Player = Cast<ASFE1Character>(Pawn); Player && Player->GetMetaHuman())
    {
        Figure = Player->SpawnPosedMetaHuman(Body, FTransform::Identity);  // on the mannequin copy, as in the world
        if (Figure)
        {
            Body->SetVisibility(false, false);
            Capture->ShowOnlyActors.Add(Figure);
            for (UPrimitiveComponent* Part : TInlineComponentArray<UPrimitiveComponent*>(Figure)) { StudioChannel(Part->LightingChannels); Part->MarkRenderStateDirty(); }
        }
    }

    // Frame the full figure: the camera looks at the front (+X) from a distance that fits the height in the 5:7 target, lights around it.
    const FBox Box = Body->Bounds.GetBox();
    const float Height = FMath::Max(100.f, float(Box.Max.Z - Box.Min.Z));
    const FVector Centre(GetActorLocation().X, GetActorLocation().Y, Box.GetCenter().Z);
    const float Aspect = Target->SizeX > 0 && Target->SizeY > 0 ? float(Target->SizeX) / float(Target->SizeY) : 5.f / 7.f;
    const float TanHalfVertical = FMath::Tan(FMath::DegreesToRadians(Capture->FOVAngle * 0.5f)) / Aspect;
    const float Distance = Height * 0.56f / TanHalfVertical;
    Capture->SetWorldLocationAndRotation(Centre + FVector(Distance, 0.f, 0.f), FRotator(0.f, 180.f, 0.f));
    if (Lights.Num() == 3)
    {
        Lights[0]->SetWorldLocation(Centre + FVector(Distance * 0.6f, -Height * 0.8f, Height * 0.6f));
        Lights[1]->SetWorldLocation(Centre + FVector(Distance * 0.7f, Height * 0.9f, Height * 0.1f));
        Lights[2]->SetWorldLocation(Centre + FVector(-Height * 0.9f, Height * 0.4f, Height * 0.7f));
    }
}

void ASFEquipmentPreviewStudio::UnbindFace()
{
    if (const ASFE1Character* Player = FaceSource.Get()) { Player->OnFaceChanged.Remove(FaceHandle); }
    FaceSource.Reset();
    FaceHandle.Reset();
}

void ASFEquipmentPreviewStudio::EndPlay(const EEndPlayReason::Type Reason)
{
    UnbindFace();
    if (Figure) { Figure->Destroy(); Figure = nullptr; }
    Super::EndPlay(Reason);
}

void ASFEquipmentPreviewStudio::AddYaw(float Degrees) { Turntable->AddLocalRotation(FRotator(0.f, Degrees, 0.f)); }

// ---- window

FString USFEquipmentWidget::TabLabel(ESFEquipmentTab Tab)
{
    switch (Tab)
    {
    case ESFEquipmentTab::Weapons: return TEXT("Weapons");
    case ESFEquipmentTab::Armor: return TEXT("Armor");
    case ESFEquipmentTab::Accessories: return TEXT("Accessories");
    default: return TEXT("All");
    }
}

bool USFEquipmentWidget::InTab(FName SlotName, ESFEquipmentTab Tab)
{
    if (SlotName.IsNone()) { return false; }
    const bool bWeapon = SlotName == TEXT("MainHand") || SlotName == TEXT("OffHand");
    const bool bAccessory = SlotName == TEXT("Neck") || SlotName == TEXT("Ring");
    switch (Tab)
    {
    case ESFEquipmentTab::Weapons: return bWeapon;
    case ESFEquipmentTab::Accessories: return bAccessory;
    case ESFEquipmentTab::Armor: return !bWeapon && !bAccessory;
    default: return true;
    }
}

ESFEquipDrop USFEquipmentWidget::ResolveDrop(const FSoftObjectPath& Item, FName ItemSlot, FName FromEquipSlot, FName TargetSlot, bool bOnGrid)
{
    if (!TargetSlot.IsNone()) { return !Item.IsNull() && ItemSlot == TargetSlot ? ESFEquipDrop::Equip : ESFEquipDrop::None; }
    // Released anywhere else in the window: a doll item goes back to the bag, a bag item off the grid is worn in its own slot.
    if (!FromEquipSlot.IsNone()) { return ESFEquipDrop::Unequip; }
    return !bOnGrid && !Item.IsNull() && !ItemSlot.IsNone() ? ESFEquipDrop::Equip : ESFEquipDrop::None;
}

void USFEquipmentWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    if (!SlotClass) { SlotClass = LoadClass<USFSlotWidget>(nullptr, TEXT("/Game/Frontier/UI/Widgets/Common/WBP_ItemSlot.WBP_ItemSlot_C")); }
    for (const FName SlotName : USFGatheringPilotComponent::EquipmentSlots())
    {
        if (USFSlotWidget* Doll = Cast<USFSlotWidget>(GetWidgetFromName(FName(*(SlotName.ToString() + TEXT("Slot"))))))
        {
            DollSlots.Add(SlotName, Doll);
            Doll->OnDoubleClicked.AddDynamic(this, &USFEquipmentWidget::HandleDollDoubleClicked);
            Doll->OnClicked.AddDynamic(this, &USFEquipmentWidget::HandleDollClicked);
            Doll->SetIcon(nullptr);
        }
    }
    if (TabBar)
    {
        TArray<FSFTabSpec> Specs;
        for (int32 Index = 0; Index <= static_cast<int32>(ESFEquipmentTab::Accessories); ++Index) { Specs.Add(FSFTabSpec(FText::FromString(TabLabel(static_cast<ESFEquipmentTab>(Index))))); }
        TabBar->SetTabs(Specs);
        TabBar->OnTabSelected.AddDynamic(this, &USFEquipmentWidget::HandleTab);
    }
    if (Tooltip) { Tooltip->SetVisibility(ESlateVisibility::Collapsed); }
    OnVisibilityChanged.AddDynamic(this, &USFEquipmentWidget::HandleVisibility);
    SelectTab(ESFEquipmentTab::All);
}

void USFEquipmentWidget::ApplyKitStyle()
{
    for (UTextBlock* Text : {WeightText.Get(), WeightPercentText.Get(), EmptyText.Get(), TipTitle.Get(), TipRarity.Get(), TipType.Get(), TipQuantity.Get(), TipWeight.Get(), TipDescription.Get()}) { TextStyle.Apply(Text); }
}

void USFEquipmentWidget::Configure(USFGatheringPilotComponent* InPilot) { Pilot = InPilot; }

void USFEquipmentWidget::NativeDestruct()
{
    CloseStudio();
    Super::NativeDestruct();
}

// The studio exists only while the window is shown: opening spawns it, closing (the window stack collapses the window) destroys it.
void USFEquipmentWidget::HandleVisibility(ESlateVisibility InVisibility)
{
    if (InVisibility == ESlateVisibility::Collapsed || InVisibility == ESlateVisibility::Hidden) { CloseStudio(); } else { OpenStudio(); }
}

void USFEquipmentWidget::OpenStudio()
{
    UWorld* World = GetWorld();
    if (Studio || !World || !World->IsGameWorld() || !GetOwningPlayer() || !GetOwningPlayer()->GetPawn()) { return; }
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.ObjectFlags |= RF_Transient;
    Studio = World->SpawnActor<ASFEquipmentPreviewStudio>(ASFEquipmentPreviewStudio::StudioLocation, FRotator::ZeroRotator, Params);
    EquippedSignature.Reset();  // the next tick copies the pawn
}

void USFEquipmentWidget::CloseStudio()
{
    if (Studio) { Studio->Destroy(); }
    Studio = nullptr;
    bRotating = false;
}

void USFEquipmentWidget::Collect(TArray<FSFInventoryEntry>& OutBag, TMap<FName, FSFInventoryEntry>& OutEquipped, int32& OutSlots) const
{
    OutSlots = 0;
    const auto* Inventory = Pilot && Pilot->GetOwner() ? Pilot->GetOwner()->FindComponentByClass<USFInventoryComponent>() : nullptr;
    if (!Inventory) { return; }
    OutSlots = Inventory->NumSlots;
    TArray<FSFInventoryEntry> Items;
    for (int32 I = 0; I < FMath::Clamp(Inventory->NumSlots, 0, USFInventoryWidget::MaxSlots); ++I)
    {
        TSoftObjectPtr<USFItemDefinition> Item; int32 Quantity = 0;
        const USFItemDefinition* Definition = Inventory->GetSlot(I, Item, Quantity) && Quantity > 0 ? Item.LoadSynchronous() : nullptr;
        if (Definition) { Items.Add(USFInventoryWidget::MakeEntry(*Definition, Item.GetAssetName(), Quantity, false)); }
    }
    OutBag = USFInventoryWidget::BuildEntries(Items);
    for (const FName SlotName : USFGatheringPilotComponent::EquipmentSlots())
    {
        TSoftObjectPtr<USFItemDefinition> Item; int32 Quantity = 0;
        const USFItemDefinition* Definition = Inventory->GetEquippedItem(SlotName, Item, Quantity) ? Item.LoadSynchronous() : nullptr;
        if (Definition) { OutEquipped.Add(SlotName, USFInventoryWidget::MakeEntry(*Definition, Item.GetAssetName(), Quantity, true)); }
    }
}

void USFEquipmentWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!Pilot || GetVisibility() == ESlateVisibility::Collapsed) { return; }
    TArray<FSFInventoryEntry> Bag; TMap<FName, FSFInventoryEntry> Equipped; int32 Slots = 0;
    Collect(Bag, Equipped, Slots);
    float Total = 0.f, Max = 0.f;
    const bool bWeight = Pilot->GetInventoryWeight(Total, Max);
    Present(Bag, Equipped, Slots, Total, bWeight ? Max : -1.f);
    const USFExchangeComponent* Exchange = GetOwningPlayer() ? GetOwningPlayer()->FindComponentByClass<USFExchangeComponent>() : nullptr;
    if (Exchange && Gold) { Gold->SetAmount(Exchange->GetWallet().Available); }
    if (Gold) { Gold->SetVisibility(Exchange ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
    UpdateTooltip();

    if (!Studio) { OpenStudio(); }  // the pawn may arrive after the window opened
    FString Signature;
    for (const TPair<FName, FSFInventoryEntry>& Pair : Equipped) { Signature += Pair.Key.ToString() + TEXT("=") + Pair.Value.ItemPath.ToString() + TEXT(";"); }
    const APawn* Pawn = GetOwningPlayer() ? GetOwningPlayer()->GetPawn() : nullptr;
    if (Studio && Pawn && (Signature != EquippedSignature || Studio->CopiedMeshCount() == 0))
    {
        Studio->CopyFrom(Pawn);  // the replicated equipment changed: copy the body and its attached item visuals again
        EquippedSignature = Signature;
    }
}

void USFEquipmentWidget::EnsureCells(int32 Count)
{
    Count = FMath::Clamp(Count, 0, USFInventoryWidget::MaxSlots);
    if (!ItemGrid || !SlotClass || Cells.Num() == Count) { return; }
    ItemGrid->ClearChildren();
    Cells.Reset();
    UUniformGridPanel* Uniform = Cast<UUniformGridPanel>(ItemGrid);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        USFSlotWidget* Cell = CreateWidget<USFSlotWidget>(this, SlotClass);
        if (!Cell) { continue; }
        if (Uniform) { Uniform->AddChildToUniformGrid(Cell, Index / GridColumns, Index % GridColumns); } else { ItemGrid->AddChild(Cell); }
        Cell->OnDoubleClicked.AddDynamic(this, &USFEquipmentWidget::HandleCellDoubleClicked);
        Cells.Add(Cell);
    }
}

void USFEquipmentWidget::Present(const TArray<FSFInventoryEntry>& Bag, const TMap<FName, FSFInventoryEntry>& Equipped, int32 SlotCount, float TotalKg, float MaxKg)
{
    // The grid: equippable, unequipped bag stacks of the active tab, one cell per shown stack (no empty cells).
    ShownItems = Bag.FilterByPredicate([this](const FSFInventoryEntry& E) { return !E.bEquipped && InTab(E.EquipSlot, ActiveTab); });
    EnsureCells(ShownItems.Num());
    for (int32 Index = 0; Index < Cells.Num(); ++Index)
    {
        USFSlotWidget* Cell = Cells[Index];
        const bool bFilled = ShownItems.IsValidIndex(Index);
        UTexture2D* Icon = bFilled ? ShownItems[Index].Icon.LoadSynchronous() : nullptr;
        Cell->SetIcon(Icon);
        Cell->SetKeyText(bFilled && !Icon ? ShownItems[Index].Name : FText::GetEmpty());
        Cell->SetCount(bFilled ? ShownItems[Index].Count : 0);
        Cell->SetDragPayload(bFilled ? ShownItems[Index].ItemPath : FSoftObjectPath(), bFilled ? ShownItems[Index].EquipSlot : NAME_None, NAME_None);
    }
    // The doll: the equipped item's icon over equip_slot_back, else the slot's ghost.
    EquippedItems = Equipped;
    for (const TPair<FName, TObjectPtr<USFSlotWidget>>& Pair : DollSlots)
    {
        const FSFInventoryEntry* Entry = Equipped.Find(Pair.Key);
        UTexture2D* Icon = Entry ? Entry->Icon.LoadSynchronous() : nullptr;
        Pair.Value->SetIcon(Icon);
        Pair.Value->SetKeyText(Entry && !Icon ? Entry->Name : FText::GetEmpty());
        Pair.Value->SetDragPayload(FSoftObjectPath(), NAME_None, Entry ? Pair.Key : NAME_None);
    }
    if (EmptyText)
    {
        EmptyText->SetText(FText::FromString(TEXT("Nothing to equip here.")));
        EmptyText->SetVisibility(ShownItems.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    if (WeightText && MaxKg >= 0.f)
    {
        WeightText->SetText(FText::FromString(MaxKg > 0.f ? FString::Printf(TEXT("Weight %.1f / %.0f kg"), TotalKg, MaxKg) : FString::Printf(TEXT("Weight %.1f kg"), TotalKg)));
    }
    if (MaxKg >= 0.f)
    {
        const float Fraction = MaxKg > 0.f ? FMath::Clamp(TotalKg / MaxKg, 0.f, 1.f) : 0.f;
        if (WeightBar) { WeightBar->SetPercent(Fraction); }
        if (WeightPercentText) { WeightPercentText->SetText(MaxKg > 0.f ? FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Fraction * 100.f))) : FText::GetEmpty()); }
    }
}

FText USFEquipmentWidget::GetWeightLine() const { return WeightText ? WeightText->GetText() : FText::GetEmpty(); }

void USFEquipmentWidget::SelectTab(ESFEquipmentTab Tab)
{
    ActiveTab = Tab;
    if (TabBar) { TabBar->SelectTab(static_cast<int32>(Tab), false); }
}

void USFEquipmentWidget::HandleTab(int32 Index) { SelectTab(static_cast<ESFEquipmentTab>(FMath::Clamp(Index, 0, static_cast<int32>(ESFEquipmentTab::Accessories)))); }

void USFEquipmentWidget::HandleCellDoubleClicked(USFSelectableWidget* Source)
{
    const int32 Index = Cells.IndexOfByKey(Cast<USFSlotWidget>(Source));
    if (Pilot && ShownItems.IsValidIndex(Index)) { Pilot->RequestEquip(ShownItems[Index].ItemPath, ShownItems[Index].EquipSlot); }
}

void USFEquipmentWidget::HandleDollDoubleClicked(USFSelectableWidget* Source)
{
    for (const TPair<FName, TObjectPtr<USFSlotWidget>>& Pair : DollSlots)
    {
        if (Pair.Value == Source && Pilot && EquippedItems.Contains(Pair.Key)) { Pilot->RequestUnequip(Pair.Key); return; }
    }
}

void USFEquipmentWidget::HandleDollClicked(USFSelectableWidget* Source)
{
    for (const TPair<FName, TObjectPtr<USFSlotWidget>>& Pair : DollSlots) { Pair.Value->SetSelected(Pair.Value == Source); }  // the gold frame marks one slot
}

FName USFEquipmentWidget::DollSlotAt(const FVector2D& ScreenPos) const
{
    for (const TPair<FName, TObjectPtr<USFSlotWidget>>& Pair : DollSlots)
    {
        if (Pair.Value->GetCachedGeometry().IsUnderLocation(ScreenPos)) { return Pair.Key; }
    }
    return NAME_None;
}

bool USFEquipmentWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    const USFItemDragOperation* Drag = Cast<USFItemDragOperation>(InOperation);
    if (!Drag) { return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation); }
    if (Drag->FromHotbar != INDEX_NONE) { return false; }  // off the bar: the hotbar slot clears
    const FVector2D At = InDragDropEvent.GetScreenSpacePosition();
    const FName Target = DollSlotAt(At);
    const UWidget* Grid = GridScroll ? GridScroll.Get() : ItemGrid.Get();
    const bool bOnGrid = Grid && Grid->GetCachedGeometry().IsUnderLocation(At);
    switch (ResolveDrop(Drag->Item, Drag->ItemSlot, Drag->FromEquipSlot, Target, bOnGrid))
    {
    case ESFEquipDrop::Equip: if (Pilot) { Pilot->RequestEquip(Drag->Item, Target.IsNone() ? Drag->ItemSlot : Target); } break;
    case ESFEquipDrop::Unequip: if (Pilot) { Pilot->RequestUnequip(Drag->FromEquipSlot); } break;
    default: break;  // an invalid drop does nothing
    }
    return true;
}

// Drag on the preview turns the character (the studio's turntable); the lights and camera stay put.
FReply USFEquipmentWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && Preview && Preview->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
    {
        bRotating = true;
        LastRotateX = InMouseEvent.GetScreenSpacePosition().X;
        return FReply::Handled().CaptureMouse(TakeWidget());
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USFEquipmentWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (!bRotating) { return Super::NativeOnMouseMove(InGeometry, InMouseEvent); }
    const float X = InMouseEvent.GetScreenSpacePosition().X;
    if (Studio) { Studio->AddYaw(-(X - LastRotateX) * RotateDegreesPerPixel); }
    LastRotateX = X;
    return FReply::Handled();
}

FReply USFEquipmentWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bRotating && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) { bRotating = false; return FReply::Handled().ReleaseMouseCapture(); }
    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

// The tooltip of the hovered grid cell or paper-doll slot, beside the cursor.
void USFEquipmentWidget::UpdateTooltip()
{
    if (!Tooltip || !FSlateApplication::IsInitialized()) { return; }
    const FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();
    const FSFInventoryEntry* Shown = nullptr;
    for (int32 Index = 0; Index < Cells.Num() && !Shown; ++Index)
    {
        if (ShownItems.IsValidIndex(Index) && Cells[Index]->GetCachedGeometry().IsUnderLocation(CursorPos)) { Shown = &ShownItems[Index]; }
    }
    if (!Shown) { const FName SlotName = DollSlotAt(CursorPos); Shown = SlotName.IsNone() ? nullptr : EquippedItems.Find(SlotName); }
    if (bRotating) { Shown = nullptr; }
    FSFItemTooltipParts Parts;
    Parts.Root = Tooltip; Parts.Icon = TipIcon; Parts.Title = TipTitle; Parts.Rarity = TipRarity; Parts.Type = TipType;
    Parts.Quantity = TipQuantity; Parts.Weight = TipWeight; Parts.Description = TipDescription;
    SFItemTooltip::Present(Parts, Shown, CursorPos);
}
