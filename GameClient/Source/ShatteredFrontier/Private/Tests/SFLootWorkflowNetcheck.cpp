#include "Tests/SFLootWorkflowNetcheck.h"
#if WITH_EDITOR
#include "Inventory/SFLootComponent.h"
#include "Inventory/SFLootSourceActor.h"
#include "Inventory/SFInventory.h"
#include "Framework/SFPlayerController.h"
#include "Framework/SFGameMode.h"
#include "Characters/SFE1Character.h"
#include "Combat/SFCombatPilot.h"
#include "Interaction/SFActionTargetComponent.h"
#include "Services/SFBackpackComponent.h"
#include "UI/SFInventoryWidget.h"
#include "UI/SFWindowStack.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Input/Events.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UObject/StrongObjectPtr.h"
#include "InputKeyEventArgs.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"
#include "GenericPlatform/GenericWindow.h"
#include "Framework/SFE1Settings.h"

namespace
{
struct FWorkflowState
{
    int32 Stage = 0;
    double NextAt = 0;
    TWeakObjectPtr<ASFLootSourceActor> Source;
    FString Item, Instance;
    int32 Quantity = 0, Before = 0;
    bool bSeenCorpse = false;
    bool bGroundSeen = false, bObserverReported = false, bGroundBarrierWritten = false;
    double BarrierAt = -1;
    double NextDiagnosticAt = 0;
    double ClickDeadline = 0;
    double CursorProbeUntil = 0;
    double CursorReadyDeadline = 0;
    int32 StableProjectionSamples = 0, ExactCursorSamples = 0;
    FVector2D PreviousProjection = FVector2D::ZeroVector;
    FVector PreviousView = FVector::ZeroVector;
    FRotator PreviousViewRotation = FRotator::ZeroRotator;
    FVector2D InitialClickScreen = FVector2D::ZeroVector;
    FVector InitialClickView = FVector::ZeroVector;
    FRotator InitialClickRotation = FRotator::ZeroRotator;
    bool bCapturedCorpse = false;
    TWeakObjectPtr<ACameraActor> GroundCamera;
    TWeakObjectPtr<AActor> OriginalView;
    TMap<TWeakObjectPtr<UMeshComponent>, bool> CaptureVisibility;
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    uint64 GroundInputBefore = 0;
#endif
    TStrongObjectPtr<USFItemDragOperation> PendingDrag;
};
TMap<TWeakObjectPtr<USFLootComponent>, FWorkflowState> States;
void HideOwnCaptureMeshes(AActor* Pawn, FWorkflowState& State)
{
    TArray<AActor*> Attached; Pawn->GetAttachedActors(Attached, true, true); Attached.Add(Pawn);
    for (AActor* Actor : Attached)
    {
        if (!Actor) continue;
        TArray<UMeshComponent*> Meshes; Actor->GetComponents(Meshes);
        for (UMeshComponent* Mesh : Meshes)
        {
            State.CaptureVisibility.Add(Mesh, Mesh->GetVisibleFlag());
            Mesh->SetVisibility(false, false); // Local render flag only; no propagation/collision/pose change.
        }
    }
    UE_LOG(LogTemp, Display, TEXT("LOOT_CAPTURE_OWN_MESH HiddenComponents=%d"), State.CaptureVisibility.Num());
}
bool RestoreOwnCaptureMeshes(FWorkflowState& State)
{
    if (State.CaptureVisibility.IsEmpty()) return true;
    bool bExact = true;
    for (const auto& Saved : State.CaptureVisibility)
        if (auto* Mesh = Saved.Key.Get()) { Mesh->SetVisibility(Saved.Value, false); bExact &= Mesh->GetVisibleFlag() == Saved.Value; }
    UE_LOG(LogTemp, Display, TEXT("LOOT_CAPTURE_OWN_MESH RestoredComponents=%d ExactFlags=%d"), State.CaptureVisibility.Num(), bExact);
    State.CaptureVisibility.Reset();
    return bExact;
}
void CaptureRenderDiagnostic(UStaticMeshComponent* Mesh, bool bSelected)
{
    const auto* Model = Mesh->GetStaticMesh().Get(); const auto* Overlay = Mesh->GetOverlayMaterial();
    UE_LOG(LogTemp, Display, TEXT("LOOT_CAPTURE_RENDER Selected=%d Model=%s Overlay=%s OverlayBlend=%d NaniteEnabled=%d Visible=%d"),
        bSelected, *GetPathNameSafe(Model), *GetPathNameSafe(Overlay), Overlay ? int32(Overlay->GetBlendMode()) : -1,
        Model ? Model->GetNaniteSettings().bEnabled : false, Mesh->GetVisibleFlag());
    for (int32 I = 0; I < Mesh->GetNumMaterials(); ++I)
    {
        const auto* Material = Mesh->GetMaterial(I);
        UE_LOG(LogTemp, Display, TEXT("LOOT_CAPTURE_MATERIAL Slot=%d Path=%s Blend=%d"), I, *GetPathNameSafe(Material), Material ? int32(Material->GetBlendMode()) : -1);
    }
}
int32 CountItem(ASFPlayerController* Player, const FString& Id)
{
    auto* Bag = Player->FindComponentByClass<USFInventoryComponent>();
    int32 Total = 0;
    for (int32 I = 0; Bag && I < Bag->NumSlots; ++I)
    {
        TSoftObjectPtr<USFItemDefinition> Item; int32 Quantity = 0;
        if (Bag->GetSlot(I, Item, Quantity))
        { if (const auto* Definition = Item.LoadSynchronous(); Definition && Definition->UniqueId == Id) Total += Quantity; }
    }
    return Total;
}
FPointerEvent Pointer(bool bShift, const FVector2D& Position = FVector2D(1, 1), bool bDown = true)
{
    return FPointerEvent(0, Position, Position, bDown ? TSet<FKey>{EKeys::LeftMouseButton} : TSet<FKey>{},
        EKeys::LeftMouseButton, 0, FModifierKeysState(bShift, false, false, false, false, false, false, false, false));
}
void Capture(ASFPlayerController* Player, const TCHAR* Name)
{
    FString Root; FParse::Value(FCommandLine::Get(), TEXT("LootWorkflowEvidence="), Root);
    if (Root.IsEmpty()) return;
    const FString Path = FPaths::Combine(Root, FString(Name) + TEXT(".png"));
    Player->ConsoleCommand(FString::Printf(TEXT("Shot showui filename=\"%s\" nosuffix"), *Path));
    UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_CAPTURE Requested=%s"), *Path);
}
void Key(ASFPlayerController* Player, FKey Input, EInputEvent Event)
{
    Player->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0), Input,
        Event, Event == IE_Pressed ? 1.f : 0.f, false, FPlatformTime::Cycles64()));
}
void MouseLeft(ASFPlayerController* Player, EInputEvent Event) { Key(Player, EKeys::LeftMouseButton, Event); }
bool CursorOutsideWindows(USFWindowStack* Stack)
{
    if (!FSlateApplication::IsInitialized()) return false;
    const FVector2D Cursor = FSlateApplication::Get().GetCursorPos();
    for (UUserWidget* Window : Stack->GetOrder())
        if (Window && Window->IsVisible() && Window->GetCachedGeometry().IsUnderLocation(Cursor)) return false;
    return true;
}
void CursorDiagnostic(ASFPlayerController* Player, USFActionTargetComponent* Target, const FWorkflowState& State, const FVector& SourceBounds)
{
    if (!State.Source.IsValid()) return;
    FVector Start = FVector::ZeroVector, Direction = FVector::ZeroVector, View; FRotator Rotation;
    const bool bDeproject = Player->DeprojectMousePositionToWorld(Start, Direction);
    Player->GetPlayerViewPoint(View, Rotation);
    float MouseX = 0, MouseY = 0; const bool bMouse = Player->GetMousePosition(MouseX, MouseY);
    FVector2D Projected = FVector2D::ZeroVector;
    const bool bProjected = Player->ProjectWorldLocationToScreen(SourceBounds, Projected);
    int32 Width = 0, Height = 0; Player->GetViewportSize(Width, Height);
    auto* Client = Player->GetWorld()->GetGameViewport(); auto* Viewport = Client ? Client->Viewport : nullptr;
    const auto Window = Client ? Client->GetWindow() : TSharedPtr<SWindow>();
    const auto Native = Window ? Window->GetNativeWindow() : TSharedPtr<FGenericWindow>();
    const FVector2D WindowPosition = Window ? FVector2D(Window->GetPositionInScreen()) : FVector2D::ZeroVector;
    const FVector2D WindowSize = Window ? FVector2D(Window->GetSizeInScreen()) : FVector2D::ZeroVector;
    const FVector2D Slate = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetCursorPos() : FVector2D::ZeroVector;
    const auto& Selection = Target->GetSelection(); const auto& Hit = Selection.Hit;
    const FVector Pawn = Player->GetPawn()->GetActorLocation();
    UE_LOG(LogTemp, Display, TEXT("LOOT_CURSOR_COORD MouseOK=%d Mouse=(%.1f,%.1f) Slate=%s InitialProjected=%s CurrentProjectedOK=%d CurrentProjected=%s Viewport=%dx%d WindowPosition=%s WindowSize=%s Focus=%d Capture=%d WindowActive=%d NativeForeground=%d"),
        bMouse, MouseX, MouseY, *Slate.ToString(), *State.InitialClickScreen.ToString(), bProjected, *Projected.ToString(), Width, Height,
        *WindowPosition.ToString(), *WindowSize.ToString(), Viewport ? Viewport->HasFocus() : -1, Viewport ? Viewport->HasMouseCapture() : -1, Window ? Window->IsActive() : -1, Native ? Native->IsForegroundWindow() : -1);
    UE_LOG(LogTemp, Display, TEXT("LOOT_CURSOR_VIEW DeprojectOK=%d Start=%s Dir=%s View=%s Rotation=%s InitialView=%s InitialRotation=%s SourceRoot=%s SourceBounds=%s"),
        bDeproject, *Start.ToString(), *Direction.ToString(), *View.ToString(), *Rotation.ToString(), *State.InitialClickView.ToString(), *State.InitialClickRotation.ToString(),
        *State.Source->GetActorLocation().ToString(), *SourceBounds.ToString());
    UE_LOG(LogTemp, Display, TEXT("LOOT_CURSOR_HIT Selected=%s Expected=%s Blocking=%d Actor=%s Contact=%s TraceDistance=%.1f ExpectedRootDistance=%.1f HitRootDistance=%.1f ReachCm=%.1f SelectionStart=%s SelectionDir=%s"),
        *GetNameSafe(Target->GetTarget()), *GetNameSafe(State.Source.Get()), Hit.bBlockingHit, *GetNameSafe(Hit.GetActor()), *Hit.ImpactPoint.ToString(), Hit.Distance,
        FVector::Distance(Pawn, State.Source->GetActorLocation()), Hit.GetActor() ? FVector::Distance(Pawn, Hit.GetActor()->GetActorLocation()) : -1,
        GetDefault<USFE1Settings>()->InteractionRangeMetres * 100., *Selection.RayStart.ToString(), *Selection.RayDirection.ToString());
}
}

int32 FSFLootWorkflowNetcheck::FindAuthorizedRow(const TArray<FSFInventoryEntry>& Rows, const TArray<FSFLootEntry>& Items)
{
    return Rows.IndexOfByPredicate([&Items](const FSFInventoryEntry& Row)
    {
        return Items.ContainsByPredicate([&Row](const FSFLootEntry& Item)
        { return Item.DefinitionId == Row.DefinitionId && Item.InstanceId == Row.InstanceId && Item.Quantity == Row.Count; });
    });
}
bool FSFLootWorkflowNetcheck::HasPersonalSource(UWorld* World)
{
    bool bPersonal = false;
    for (TActorIterator<ASFLootSourceActor> It(World); It; ++It) bPersonal |= !It->IsGround();
    return bPersonal;
}

void FSFLootWorkflowNetcheck::Tick(USFLootComponent& Loot)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("LootWorkflowNetcheck"))) return;
    auto* Player = Cast<ASFPlayerController>(Loot.GetOwner());
    auto* Pawn = Player ? Cast<ASFE1Character>(Player->GetPawn()) : nullptr;
    if (!Pawn || !Player->GetWorld()) return;
    auto& State = States.FindOrAdd(&Loot);
    UWorld* World = Player->GetWorld();
    const double Now = World->GetTimeSeconds();
    if (State.Stage == 99) { RestoreOwnCaptureMeshes(State); return; }
    if (Now < State.NextAt) return;
    State.NextAt = Now + .2;
    if (Player->HasAuthority())
    {
        auto* Identity = Player->FindComponentByClass<USFBackpackComponent>();
        auto* Mode = World->GetAuthGameMode<ASFGameMode>();
        if (!Identity || Identity->GetCharacterId() != TEXT("chr_dev_alpha") || !Mode || Identity->IsBusy() || Now < 12) return;
        ASFPlayerController* Observer = nullptr;
        for (auto It = World->GetPlayerControllerIterator(); It; ++It)
        {
            auto* Other = Cast<ASFPlayerController>(It->Get());
            auto* Pack = Other ? Other->FindComponentByClass<USFBackpackComponent>() : nullptr;
            if (Pack && Pack->GetCharacterId() == TEXT("chr_dev_beta") && Other->GetPawn() && !Pack->IsBusy()) Observer = Other;
        }
        if (!Observer) return;
        // Fixture positioning and a lethal hit use the existing live actor and
        // real RewardKill path. Canonical profile, eligibility and rolls stay intact.
        if (State.Stage == 0 || State.Stage == 2)
        {
            ASFCombatNpc* Victim = nullptr;
            const FString Creature = State.Stage == 0 ? TEXT("npc/boar") : TEXT("npc/wolf");
            for (TActorIterator<ASFCombatNpc> It(World); It; ++It)
                if (It->Health() > 0 && It->GetCreatureId() == Creature && !It->HasSingleDrop() && !Mode->IsSafe(It->GetActorLocation())) { Victim = *It; break; }
            if (!Victim) return;
            // Use the recorded drop XY whose actual collision is covered by
            // FixtureMapGeometry. Its old Z was floating; ground every actor.
            FVector At = RecordedGeometryAnchor(); FHitResult BodyFloor;
            FCollisionQueryParams BodyQuery(SCENE_QUERY_STAT(LootFixtureBodyFloor), false, Victim);
            BodyQuery.AddIgnoredActor(Pawn); BodyQuery.AddIgnoredActor(Observer->GetPawn());
            if (!World->LineTraceSingleByChannel(BodyFloor, FVector(At.X,At.Y,20000), FVector(At.X,At.Y,-20000), ECC_WorldStatic, BodyQuery)) return;
            At.Z = BodyFloor.ImpactPoint.Z + Victim->GetSimpleCollisionHalfHeight() + 4;
            Victim->SetActorLocationAndRotation(At, FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
            const auto GroundPawn = [World, Victim, Player, Observer](ASFE1Character* Character, const FVector& Point)
            {
                if (!Character) return false;
                FCollisionQueryParams Query(SCENE_QUERY_STAT(LootFixtureFloor), false, Player->GetPawn());
                Query.AddIgnoredActor(Victim); Query.AddIgnoredActor(Observer->GetPawn()); FHitResult Floor;
                if (!World->LineTraceSingleByChannel(Floor, FVector(Point.X,Point.Y,20000), FVector(Point.X,Point.Y,-20000), ECC_WorldStatic, Query)) return false;
                const FVector Position = Floor.ImpactPoint + FVector(0,0,Character->GetSimpleCollisionHalfHeight() + 1);
                Character->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
                return Character->GetActorLocation().Equals(Position, 1);
            };
            if (Mode->IsSafe(At + FVector(130, 0, 0)) || !GroundPawn(Pawn, At + FVector(130,0,0)) ||
                !GroundPawn(Cast<ASFE1Character>(Observer->GetPawn()), At + FVector(130,220,0))) return;
            Pawn->GetCharacterMovement()->DisableMovement();
            if (auto* Other = Cast<ASFE1Character>(Observer->GetPawn())) Other->GetCharacterMovement()->DisableMovement();
            Victim->ReceivePilotDamage(Pawn, 1000000);
            UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_DEFEAT Creature=%s Dead=%d"), *Creature, Victim->Health() <= 0);
            ++State.Stage; State.bSeenCorpse = false;
        }
        else if (State.Stage == 1 || State.Stage == 3)
        {
            bool bCorpse = false;
            for (TActorIterator<ASFLootSourceActor> It(World); It; ++It) if (!It->IsGround()) bCorpse = true;
            if (bCorpse) State.bSeenCorpse = true;
            if (State.bSeenCorpse && !bCorpse) { ++State.Stage; State.NextAt = Now + 1; }
        }
        return;
    }
    if (!Player->IsLocalController()) return;
    FString Role; FParse::Value(FCommandLine::Get(), TEXT("LootWorkflowRole="), Role);
    FString GroundBarrier; FParse::Value(FCommandLine::Get(), TEXT("LootWorkflowGroundSeen="), GroundBarrier);
    if (Role == TEXT("Observer"))
    {
        const bool bPersonal = HasPersonalSource(World);
        for (TActorIterator<ASFLootSourceActor> It(World); It; ++It)
            if (It->IsGround() && It->Mesh->GetStaticMesh()) State.bGroundSeen = true;
        if (bPersonal) { UE_LOG(LogTemp, Error, TEXT("LOOT_WORKFLOW_FAIL ObserverSawPersonalCorpse")); State.Stage = 99; return; }
        if (State.bGroundSeen && !State.bGroundBarrierWritten && !GroundBarrier.IsEmpty())
            State.bGroundBarrierWritten = FFileHelper::SaveStringToFile(TEXT("Observer received the meshed ground source."), *GroundBarrier);
        FString Barrier; FParse::Value(FCommandLine::Get(), TEXT("LootWorkflowBarrier="), Barrier);
        if (!Barrier.IsEmpty() && FPaths::FileExists(Barrier) && State.BarrierAt < 0) State.BarrierAt = Now;
        if (State.bGroundSeen && State.BarrierAt >= 0 && Now >= State.BarrierAt + 1 && !State.bObserverReported)
        {
            UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_OBSERVER PersonalSources=0 GroundMesh=1 DriverComplete=1"));
            State.bObserverReported = true; // Continue monitoring through runner teardown.
        }
        return;
    }
    if (Role != TEXT("Driver")) return;
    auto* Stack = Player->GetWindowStack();
    auto* Target = Player->FindComponentByClass<USFActionTargetComponent>();
    if (!Stack || !Target) return;
    const bool bDiagnosticNow = Now >= State.NextDiagnosticAt;
    if (bDiagnosticNow)
    {
        State.NextDiagnosticAt = Now + 10;
        UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_STATE Stage=%d Own=%d External=%d Items=%d Rows=%d Windows=%d"), State.Stage,
            Player->InventoryWidget != nullptr, Loot.ExternalWidget != nullptr, Loot.ExternalItems.Num(),
            Loot.ExternalWidget ? Loot.ExternalWidget->ShownItems.Num() : -1, Stack->Num());
    }
    if (State.Stage == 0 || State.Stage == 2)
    {
        for (TActorIterator<ASFLootSourceActor> It(World); It; ++It)
        {
            if (It->IsGround() || !It->Mesh->GetStaticMesh()) continue;
            if (!It->InteractionBounds->GetUnscaledBoxExtent().Equals(It->HitShape.Extent, .01) ||
                !It->InteractionBounds->GetRelativeLocation().Equals(It->HitShape.Center, .01))
            { if (bDiagnosticNow) UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_GATE Reason=ReplicatedShape Source=%s"), *It->GetActorLocation().ToString()); continue; }
            // An outer cross-section beyond the constructor's 25 cm box must
            // collide in this actual network client, not just at body centre.
            FHitResult EdgeHit; const auto Transform = It->GetActorTransform();
            const float Edge = It->HitShape.Extent.X * .8f;
            if (Edge <= 25 || !World->LineTraceSingleByChannel(EdgeHit,
                Transform.TransformPosition(It->HitShape.Center + FVector(Edge, -200, 0)),
                Transform.TransformPosition(It->HitShape.Center + FVector(Edge, 200, 0)), ECC_Visibility) || EdgeHit.GetActor() != *It)
            {
                if (bDiagnosticNow) UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_GATE Reason=OuterHit Source=%s Extent=%s Blocker=%s"), *It->GetActorLocation().ToString(), *It->HitShape.Extent.ToString(), *GetNameSafe(EdgeHit.GetActor()));
                continue;
            }
            FVector Start; FRotator Rotation; Player->GetPlayerViewPoint(Start, Rotation);
            Target->RefreshFromRay(Start, (It->InteractionBounds->GetComponentLocation() - Start).GetSafeNormal());
            if (Target->GetTarget() != *It || !It->Mesh->GetOverlayMaterial())
            {
                if (bDiagnosticNow) UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_GATE Reason=SelectedTarget Source=%s Pawn=%s Distance=%.1f Selected=%s Blocker=%s Rim=%d"), *It->GetActorLocation().ToString(), *Pawn->GetActorLocation().ToString(), FVector::Distance(Pawn->GetActorLocation(), It->GetActorLocation()), *GetNameSafe(Target->GetTarget()), *GetNameSafe(Target->GetSelection().Hit.GetActor()), It->Mesh->GetOverlayMaterial() != nullptr);
                continue;
            }
            State.Source = *It;
            UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_TARGET Corpse=1 ExactTarget=1 Rim=1 ClientBounds=1"));
            if (State.Stage == 0)
            {
                const float Yaw = (It->GetActorLocation() - Pawn->GetActorLocation()).Rotation().Yaw;
                Player->SetControlRotation(FRotator(-15,Yaw,0)); State.Stage = 16;
                State.CursorReadyDeadline = Now + 8; return;
            }
            Player->DispatchSelectedAction(Target->GetSelection());
            ++State.Stage; return;
        }
    }
    else if (State.Stage == 1 || State.Stage == 3)
    {
        if (State.Stage == 1 && !State.Source.IsValid() && Loot.ExternalItems.IsEmpty())
        { Stack->Close(Loot.ExternalWidget); State.Stage = 2; State.NextAt = Now + 1; return; }
        if (!Loot.ExternalWidget || !Player->InventoryWidget || !Stack->IsOpen(Loot.ExternalWidget) || !Stack->IsOpen(Player->InventoryWidget)) return;
        auto* Window = Loot.ExternalWidget.Get();
        // A real NativeTick populates cells from the RPC result, as in normal play.
        if (Window->ShownItems.IsEmpty()) return;
        if (State.Stage == 1 && !State.bCapturedCorpse) { Capture(Player, TEXT("corpse-and-bag")); State.bCapturedCorpse = true; }
        int32 Index = FindAuthorizedRow(Window->ShownItems, Loot.ExternalItems);
        if (State.Stage == 3)
        {
            Index = Window->ShownItems.IndexOfByPredicate([](const FSFInventoryEntry& Item) { return Item.DefinitionId == TEXT("item/wolf_pelt"); });
            if (Index == INDEX_NONE) return;
        }
        if (Index == INDEX_NONE) return;
        const auto Item = Window->ShownItems[Index];
        auto* Cell = Window->GetCell(Index); if (!Cell) return;
        State.Item = Item.DefinitionId; State.Instance = Item.InstanceId; State.Quantity = Item.Count; State.Before = CountItem(Player, State.Item);
        if (State.Stage == 1)
        {
            Cell->NativeOnMouseButtonDown(Cell->GetCachedGeometry(), Pointer(true));
            UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_SHIFT Windows=2 Item=%s Quantity=%d Instance=%s"), *State.Item, State.Quantity, *State.Instance);
            State.Stage = 10;
        }
        else
        {
            UDragDropOperation* Operation = nullptr;
            Cell->NativeOnDragDetected(Cell->GetCachedGeometry(), Pointer(false), Operation);
            if (!Operation || !Player->InventoryWidget->NativeOnDrop(Player->InventoryWidget->GetCachedGeometry(), FDragDropEvent(Pointer(false), nullptr), Operation))
            { UE_LOG(LogTemp, Error, TEXT("LOOT_WORKFLOW_FAIL NativeDragRejected")); State.Stage = 99; return; }
            UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_DRAG Windows=2 Item=%s Quantity=%d Instance=%s"), *State.Item, State.Quantity, *State.Instance);
            State.Stage = 11;
        }
    }
    else if (State.Stage == 16 && State.Source.IsValid())
    {
        if (Now > State.CursorReadyDeadline)
        { CursorDiagnostic(Player, Target, State, State.Source->InteractionBounds->GetComponentLocation()); UE_LOG(LogTemp, Error, TEXT("LOOT_WORKFLOW_FAIL CursorProjectionDidNotSettle")); State.Stage = 99; Player->ConsoleCommand(TEXT("quit")); return; }
        FVector2D Screen; int32 Width = 0, Height = 0; Player->GetViewportSize(Width, Height);
        if (!Player->ProjectWorldLocationToScreen(State.Source->InteractionBounds->GetComponentLocation(), Screen) ||
            Screen.X < 0 || Screen.Y < 0 || Screen.X >= Width || Screen.Y >= Height)
        { if (bDiagnosticNow) UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_GATE Reason=ClickProjection Point=%s Viewport=%dx%d"), *Screen.ToString(), Width, Height); return; }
        FVector View; FRotator Rotation; Player->GetPlayerViewPoint(View, Rotation);
        // Teleporting the fixture pawn leaves the normal spring-arm camera
        // catching up. Only aim after consecutive stable views/projections.
        const bool bStable = FVector2D::Distance(Screen, State.PreviousProjection) <= 1 &&
            FVector::DistSquared(View, State.PreviousView) <= 1 && Rotation.Equals(State.PreviousViewRotation, .05f);
        State.StableProjectionSamples = bStable ? State.StableProjectionSamples + 1 : 0;
        State.PreviousProjection = Screen; State.PreviousView = View; State.PreviousViewRotation = Rotation;
        if (State.StableProjectionSamples < 2) return;
        UE_LOG(LogTemp, Display, TEXT("LOOT_CURSOR_STABLE Samples=%d Projected=%s View=%s"), State.StableProjectionSamples, *Screen.ToString(), *View.ToString());
        State.InitialClickScreen = Screen; State.InitialClickView = View; State.InitialClickRotation = Rotation;
        Player->SetMouseLocation(FMath::RoundToInt(Screen.X), FMath::RoundToInt(Screen.Y)); State.Stage = 17;
        State.CursorProbeUntil = Now + 1;
    }
    else if (State.Stage == 17 && State.Source.IsValid())
    {
        Target->Refresh();
        const bool bProbe = FParse::Param(FCommandLine::Get(), TEXT("LootWorkflowCursorProbe"));
        if (bDiagnosticNow || bProbe) CursorDiagnostic(Player, Target, State, State.Source->InteractionBounds->GetComponentLocation());
        if (bProbe)
        {
            if (Now >= State.CursorProbeUntil)
            { UE_LOG(LogTemp, Display, TEXT("LOOT_CURSOR_PROBE_DONE ExactTarget=%d InputSent=0"), Target->GetTarget() == State.Source.Get()); State.Stage = 99; }
            return; // Diagnostic run stops before any click, transfer or drop.
        }
        if (Target->GetTarget() != State.Source.Get())
        {
            State.ExactCursorSamples = 0;
            if (Now > State.CursorReadyDeadline)
            { CursorDiagnostic(Player, Target, State, State.Source->InteractionBounds->GetComponentLocation()); UE_LOG(LogTemp, Error, TEXT("LOOT_WORKFLOW_FAIL StableCursorDidNotSelectSource")); State.Stage = 99; Player->ConsoleCommand(TEXT("quit")); return; }
            if (bDiagnosticNow) UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_GATE Reason=CursorTarget Selected=%s Expected=%s"), *GetNameSafe(Target->GetTarget()), *GetNameSafe(State.Source.Get())); return;
        }
        if (++State.ExactCursorSamples < 2) return;
        MouseLeft(Player, IE_Pressed); State.Stage = 18; State.NextAt = Now + .04;
    }
    else if (State.Stage == 18)
    { MouseLeft(Player, IE_Released); State.Stage = 19; State.ClickDeadline = Now + 5; State.NextAt = Now + .04; }
    else if (State.Stage == 19)
    {
        if (Loot.ExternalSource == State.Source.Get() && Loot.ExternalWidget && Player->InventoryWidget &&
            Stack->IsOpen(Loot.ExternalWidget) && Stack->IsOpen(Player->InventoryWidget))
        {
            UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_CLICK Input=NativeMouseLeft PressRelease=1 CursorTarget=1 Windows=2")); State.Stage = 1;
        }
        else if (Now > State.ClickDeadline)
        { UE_LOG(LogTemp, Error, TEXT("LOOT_WORKFLOW_FAIL NativeMouseClickDidNotOpen")); State.Stage = 99; Player->ConsoleCommand(TEXT("quit")); }
    }
    else if (State.Stage == 10 || State.Stage == 11)
    {
        if (CountItem(Player, State.Item) != State.Before + State.Quantity) return;
        if (State.Stage == 10)
        {
            State.Stage = 1; // Drain every real rolled row, including bonus recipes.
            UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_SHIFT_COMMITTED"));
        }
        else
        {
            auto* Own = Player->InventoryWidget.Get();
            const int32 Index = Own->ShownItems.IndexOfByPredicate([&State](const FSFInventoryEntry& Item) { return Item.DefinitionId == State.Item && Item.InstanceId == State.Instance; });
            if (Index == INDEX_NONE || !Own->GetCell(Index)) return;
            UDragDropOperation* Operation = nullptr; auto* Cell = Own->GetCell(Index);
            Cell->NativeOnDragDetected(Cell->GetCachedGeometry(), Pointer(false), Operation);
            if (!Operation) return;
            Stack->Close(Loot.ExternalWidget);
            State.PendingDrag.Reset(Cast<USFItemDragOperation>(Operation)); State.Stage = 14;
        }
    }
    else if (State.Stage == 14 && State.PendingDrag.IsValid())
    {
        auto* Surface = State.PendingDrag->WorldDropTarget.Get(); if (!Surface) return;
        const FGeometry Geometry = Surface->GetCachedGeometry();
        if (Geometry.GetLocalSize().X < 100 || Geometry.GetLocalSize().Y < 100) return;
        for (double X : {.9, .7, .5}) for (double Y : {.4, .6, .2})
        {
            const FVector2D Point = Geometry.LocalToAbsolute(FVector2D(Geometry.GetLocalSize()) * FVector2D(X,Y));
            const auto Release = Pointer(false, Point, false);
            if (Surface->NativeOnDrop(Geometry, FDragDropEvent(Release, nullptr), State.PendingDrag.Get()))
            {
                State.PendingDrag->Drop_Implementation(Release); State.PendingDrag.Reset(); State.Stage = 12;
                UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_DROP_REQUEST WorldRelease=1 Item=%s"), *State.Item); return;
            }
        }
    }
    else if (State.Stage == 12)
    {
        // Keep the actual replicated ground model alive until the observer has
        // received it, then continue monitoring privacy through the end barrier.
        if (GroundBarrier.IsEmpty() || !FPaths::FileExists(GroundBarrier)) return;
        for (TActorIterator<ASFLootSourceActor> It(World); It; ++It)
        {
            if (!It->IsGround() || !It->Mesh->GetStaticMesh() || CountItem(Player, State.Item) != State.Before) continue;
            State.Source = *It;
            // Local evidence camera only: the pawn/source/collision and server reach
            // remain untouched. Look from the side away from the pawn, above the
            // item, so its edge is clear of the legs and bottom interaction prompt.
            State.OriginalView = Player->GetViewTarget();
            State.GroundCamera = World->SpawnActor<ACameraActor>();
            if (!State.GroundCamera.IsValid()) return;
            const FVector Centre = It->Mesh->Bounds.Origin;
            const double Radius = FMath::Max(30.0, It->Mesh->Bounds.BoxExtent.GetMax());
            FVector Away = (Centre - Pawn->GetActorLocation()).GetSafeNormal2D();
            if (Away.IsNearlyZero()) Away = FVector::ForwardVector;
            const FVector Eye = Centre + Away * (Radius * 4 + 100) + FVector(0,0,Radius * 3 + 100);
            State.GroundCamera->SetActorLocationAndRotation(Eye, (Centre - Eye).Rotation());
            State.GroundCamera->GetCameraComponent()->SetFieldOfView(45.f);
            State.GroundCamera->GetCameraComponent()->bConstrainAspectRatio = false;
            Player->SetViewTarget(State.GroundCamera.Get()); State.Stage = 20; return;
        }
    }
    else if (State.Stage == 20 && State.Source.IsValid())
    {
        if (FParse::Param(FCommandLine::Get(), TEXT("LootWorkflowCapturePair")) && State.CaptureVisibility.IsEmpty()) HideOwnCaptureMeshes(Pawn, State);
        int32 Width = 0, Height = 0; Player->GetViewportSize(Width, Height);
        if (Width < 100 || Height < 100) return;
        Player->SetMouseLocation(Width - 32, 32); State.Stage = 21;
    }
    else if (State.Stage == 21 && State.Source.IsValid())
    {
        Target->Refresh();
        if (State.Source->Mesh->GetOverlayMaterial())
        { UE_LOG(LogTemp, Error, TEXT("LOOT_WORKFLOW_FAIL GroundUnselectedRimStillSet")); State.Stage = 99; return; }
        CaptureRenderDiagnostic(State.Source->Mesh, false);
        Capture(Player, TEXT("ground-unselected")); State.Stage = 22;
    }
    else if (State.Stage == 22 && State.Source.IsValid())
    {
        FVector2D Screen; int32 Width = 0, Height = 0; Player->GetViewportSize(Width, Height);
        if (!Player->ProjectWorldLocationToScreen(State.Source->InteractionBounds->GetComponentLocation(), Screen) ||
            Screen.X < 32 || Screen.Y < 32 || Screen.X >= Width - 32 || Screen.Y >= Height * .7)
        { UE_LOG(LogTemp, Error, TEXT("LOOT_WORKFLOW_FAIL GroundProjection Point=%s Viewport=%dx%d"), *Screen.ToString(), Width, Height); State.Stage = 99; return; }
        Player->SetMouseLocation(FMath::RoundToInt(Screen.X), FMath::RoundToInt(Screen.Y)); State.Stage = 23;
    }
    else if (State.Stage == 23 && State.Source.IsValid())
    {
        Target->Refresh();
        if (!CursorOutsideWindows(Stack) || !Stack->IsOpen(Player->InventoryWidget) ||
            Target->GetTarget() != State.Source.Get() || !State.Source->Mesh->GetOverlayMaterial())
        { UE_LOG(LogTemp, Error, TEXT("LOOT_WORKFLOW_FAIL GroundCursor OutsideWindows=%d OwnOpen=%d ExactTarget=%d Rim=%d"), CursorOutsideWindows(Stack), Stack->IsOpen(Player->InventoryWidget), Target->GetTarget() == State.Source.Get(), State.Source->Mesh->GetOverlayMaterial() != nullptr); State.Stage = 99; return; }
        CaptureRenderDiagnostic(State.Source->Mesh, true);
        Capture(Player, TEXT("ground-selected")); State.Stage = 24;
    }
    else if (State.Stage == 24 && State.Source.IsValid())
    {
        if (FParse::Param(FCommandLine::Get(), TEXT("LootWorkflowCapturePair")))
        { State.Stage = 26; State.NextAt = Now + .5; return; } // Let existing Shot requests finish before restoring.
        Target->Refresh();
        if (!CursorOutsideWindows(Stack) || Target->GetTarget() != State.Source.Get())
        { UE_LOG(LogTemp, Error, TEXT("LOOT_WORKFLOW_FAIL GroundCursorChangedBeforeInput")); State.Stage = 99; return; }
        const auto* Mapping = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Frontier/Input/IMC_Player.IMC_Player"));
        if (!Mapping || !Mapping->GetMappings().ContainsByPredicate([](const FEnhancedActionKeyMapping& Entry)
            { return Entry.Key == EKeys::F && Entry.Action && Entry.Action->GetFName() == TEXT("IA_Interact"); }))
        { UE_LOG(LogTemp, Error, TEXT("LOOT_WORKFLOW_FAIL ShippedInteractFMappingMissing")); State.Stage = 99; return; }
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
        State.GroundInputBefore = Player->GetInteractionInputAttemptCount();
#endif
        // tools/Create-PlayerInputAssets.py ships IA_Interact on F. The actual
        // character action must reach the existing controller UI guard/dispatch.
        Key(Player, EKeys::F, IE_Pressed); State.Stage = 25; State.NextAt = Now + .04;
    }
    else if (State.Stage == 25)
    {
        Key(Player, EKeys::F, IE_Released); State.Stage = 13; State.ClickDeadline = Now + 5;
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
        if (Player->GetInteractionInputAttemptCount() != State.GroundInputBefore + 1)
        { UE_LOG(LogTemp, Error, TEXT("LOOT_WORKFLOW_FAIL GroundNativeActionAttempts Before=%llu After=%llu"), State.GroundInputBefore, Player->GetInteractionInputAttemptCount()); State.Stage = 99; return; }
#endif
        UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_GROUND Input=NativeF PressRelease=1 CursorOutsideWindows=1 ControllerAction=1 Mesh=1 Rim=1"));
    }
    else if (State.Stage == 26)
    {
        const bool bRestored = RestoreOwnCaptureMeshes(State);
        if (State.OriginalView.IsValid()) Player->SetViewTarget(State.OriginalView.Get());
        if (State.GroundCamera.IsValid()) State.GroundCamera->Destroy();
        if (!bRestored) { UE_LOG(LogTemp, Error, TEXT("LOOT_WORKFLOW_FAIL CaptureVisibilityRestore")); State.Stage = 99; Player->ConsoleCommand(TEXT("quit")); return; }
        UE_LOG(LogTemp, Display, TEXT("LOOT_CAPTURE_PAIR_DONE VisibilityRestored=1 PickupInputSent=0")); State.Stage = 99;
    }
    else if (State.Stage == 13 && CountItem(Player, State.Item) == State.Before + State.Quantity && !State.Source.IsValid())
    {
        if (Stack->IsOpen(Loot.ExternalWidget)) { UE_LOG(LogTemp, Error, TEXT("LOOT_WORKFLOW_FAIL PickupOpenedWidget")); State.Stage = 99; return; }
        Target->RefreshFromRay(Pawn->GetActorLocation(), FVector::UpVector);
        if (Target->GetTarget()) return;
        if (State.OriginalView.IsValid()) Player->SetViewTarget(State.OriginalView.Get());
        if (State.GroundCamera.IsValid()) State.GroundCamera->Destroy();
        Capture(Player, TEXT("final-own-bag"));
        UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_DONE NativeShift=1 NativeDrag=1 GroundModel=1 PickupWidget=0 TargetCleared=1")); State.Stage = 99;
    }
    else if (State.Stage == 13 && Now > State.ClickDeadline)
    { UE_LOG(LogTemp, Error, TEXT("LOOT_WORKFLOW_FAIL NativeGroundActionDidNotCommit")); State.Stage = 99; Player->ConsoleCommand(TEXT("quit")); }
}
#endif
