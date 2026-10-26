#include "Construction/SFConstructionSandboxPalette.h"
#include "Engine/LocalPlayer.h"
#include "Framework/SFPlayerController.h"
#include "Input/Events.h"
#include "Layout/Geometry.h"
#include "UI/SFInventoryWidget.h"
#include "UI/SFHudWidgets.h"
#include "UI/SFWindowStack.h"
#include "GameFramework/WorldSettings.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Editor.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionHotbarRegressionTest,
    "ShatteredFrontier.Hud.BuildHotbar.DragAndAssets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConstructionHotbarRegressionTest::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world"), World)) { return false; }

    const TSubclassOf<USFConstructionSandboxPalette> PaletteClass = LoadClass<USFConstructionSandboxPalette>(nullptr, USFConstructionSandboxPalette::AssetPath);
    const TSubclassOf<USFHudWidget> HudClass = LoadClass<USFHudWidget>(nullptr, USFHudWidget::AssetPath);
    if (!TestNotNull(TEXT("Build catalogue WBP loads"), PaletteClass.Get()) || !TestNotNull(TEXT("HUD WBP loads"), HudClass.Get())) { return false; }
    USFConstructionSandboxPalette* Palette = CreateWidget<USFConstructionSandboxPalette>(World, PaletteClass);
    USFHudWidget* Hud = CreateWidget<USFHudWidget>(World, HudClass);
    if (!TestNotNull(TEXT("Build catalogue constructs"), Palette) || !TestNotNull(TEXT("HUD constructs"), Hud) || !Hud->Hotbar) { return false; }

    static constexpr int32 BuildPieceCount = 4;
    for (int32 Index = 0; Index < BuildPieceCount; ++Index)
    {
        USFSlotWidget* Tile = Palette->GetCell(Index);
        if (!TestNotNull(FString::Printf(TEXT("Catalogue tile %d"), Index), Tile)) { return false; }
        TestTrue(FString::Printf(TEXT("Catalogue tile %d has its icon"), Index), Tile->HasIcon());
        TestTrue(FString::Printf(TEXT("Catalogue tile %d carries a drag payload"), Index), Tile->CanDrag());
    }
    USFSlotWidget* DetailSlot = Cast<USFSlotWidget>(Palette->GetWidgetFromName(TEXT("DetailSlot")));
    if (!TestNotNull(TEXT("Catalogue detail preview slot"), DetailSlot)) { return false; }
    TestTrue(TEXT("Catalogue detail preview has its icon"), DetailSlot->HasIcon());

    Hud->Hotbar->Present(FSFHotbarStock());
    for (int32 Index = 0; Index < BuildPieceCount; ++Index)
    {
        TestTrue(FString::Printf(TEXT("Build hotbar slot %d has its icon"), Index + 1), Hud->Hotbar->GetCell(Index)->HasIcon());
    }

    // Exercise the same shared WBP slot drag callback used by catalogue tiles.
    USFSlotWidget* FoundationTile = Palette->GetCell(0);
    const FGeometry Geometry = FGeometry::MakeRoot(FVector2D(128.f, 128.f), FSlateLayoutTransform());
    UDragDropOperation* RawDrag = nullptr;
    FoundationTile->NativeOnDragDetected(Geometry, FPointerEvent(), RawDrag);
    USFItemDragOperation* CatalogueDrag = Cast<USFItemDragOperation>(RawDrag);
    if (!TestNotNull(TEXT("Catalogue drag uses the shared item payload"), CatalogueDrag)) { return false; }
    TestEqual(TEXT("Catalogue payload is the foundation shortcut"), CatalogueDrag->DefinitionId, FString(TEXT("build/foundation")));
    TestTrue(TEXT("Catalogue drag assigns a hotbar shortcut"), Hud->Hotbar->ApplyDropToSlot(6, *CatalogueDrag));
    TestEqual(TEXT("Catalogue source remains intact and slot 7 gets the shortcut"), Hud->Hotbar->GetLayout()[6], FString(TEXT("build/foundation")));

    // Use the real inventory WBP's Present path to populate the shared slot payload.
    const TSubclassOf<USFInventoryWidget> InventoryClass = LoadClass<USFInventoryWidget>(nullptr, USFInventoryWidget::AssetPath);
    if (!TestNotNull(TEXT("Inventory WBP loads"), InventoryClass.Get())) { return false; }
    USFInventoryWidget* Inventory = CreateWidget<USFInventoryWidget>(World, InventoryClass);
    if (!TestNotNull(TEXT("Inventory WBP constructs"), Inventory)) { return false; }
    FSFInventoryEntry AxeEntry;
    AxeEntry.Name = FText::FromString(TEXT("Iron Axe"));
    AxeEntry.DefinitionId = TEXT("item/iron_axe");
    AxeEntry.Count = 1;
    AxeEntry.Category = ESFInventoryTab::Gear;
    AxeEntry.ItemPath = FSoftObjectPath(TEXT("/Game/Frontier/Gameplay/Pilot/DA_IronAxe.DA_IronAxe"));
    TArray<FSFInventoryEntry> InventoryEntries;
    InventoryEntries.Add(AxeEntry);
    Inventory->Present(InventoryEntries, 1, 1.f, 80.f);
    USFSlotWidget* InventoryCell = Inventory->GetCell(0);
    if (!TestNotNull(TEXT("Inventory item cell"), InventoryCell)) { return false; }
    UDragDropOperation* RawInventoryDrag = nullptr;
    InventoryCell->NativeOnDragDetected(Geometry, FPointerEvent(), RawInventoryDrag);
    USFItemDragOperation* InventoryDrag = Cast<USFItemDragOperation>(RawInventoryDrag);
    if (!TestNotNull(TEXT("Inventory drag uses the shared item payload"), InventoryDrag)) { return false; }
    TestTrue(TEXT("Inventory payload keeps its source identity"), InventoryDrag->bFromInventory && InventoryDrag->Quantity == 1);
    TestTrue(TEXT("Inventory drag assigns a shortcut"), Hud->Hotbar->ApplyDropToSlot(7, *InventoryDrag));
    TestEqual(TEXT("Inventory drag binds the item id"), Hud->Hotbar->GetLayout()[7], FString(TEXT("item/iron_axe")));
    TestEqual(TEXT("Inventory drag preserves source quantity"), InventoryDrag->Quantity, 1);

    // A hotbar source cell creates the shared operation, and dropping it on another cell swaps assignments.
    USFSlotWidget* SlotSource = Hud->Hotbar->GetCell(6);
    SlotSource->SetHotbarDrag(6);
    UDragDropOperation* RawSlotDrag = nullptr;
    SlotSource->NativeOnDragDetected(Geometry, FPointerEvent(), RawSlotDrag);
    USFItemDragOperation* SlotDrag = Cast<USFItemDragOperation>(RawSlotDrag);
    if (!TestNotNull(TEXT("Hotbar drag uses the shared item payload"), SlotDrag)) { return false; }
    TestEqual(TEXT("Hotbar payload identifies its source slot"), SlotDrag->FromHotbar, 6);
    TestTrue(TEXT("Hotbar slot drop is accepted"), Hud->Hotbar->ApplyDropToSlot(7, *SlotDrag));
    TestEqual(TEXT("Hotbar slots swap"), Hud->Hotbar->GetLayout()[6], FString(TEXT("item/iron_axe")));
    TestEqual(TEXT("Hotbar destination receives the source shortcut"), Hud->Hotbar->GetLayout()[7], FString(TEXT("build/foundation")));
    USFSlotWidget* HotbarSource = Hud->Hotbar->GetCell(7);
    HotbarSource->SetHotbarDrag(7);
    HotbarSource->NativeOnDragCancelled(FDragDropEvent(FPointerEvent(), nullptr), nullptr);
    TestEqual(TEXT("Escape/focus cancellation retains the source shortcut"), Hud->Hotbar->GetLayout()[7], FString(TEXT("build/foundation")));

    USFItemDragOperation* LootDrag = NewObject<USFItemDragOperation>(Hud->Hotbar);
    LootDrag->bFromLoot = true;
    LootDrag->DefinitionId = TEXT("build/roof");
    TestFalse(TEXT("External loot cannot assign a hotbar shortcut"), Hud->Hotbar->ApplyDropToSlot(8, *LootDrag));

    // A deliberate world release removes only the hotbar shortcut. Cancellation keeps it bound.
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) { return false; }
    UWorld* TestWorld = Fixture.GetTestWorld();
    TestWorld->GetWorldSettings()->NotifyBeginPlay();
    ASFPlayerController* Player = TestWorld->SpawnActor<ASFPlayerController>();
    if (!TestNotNull(TEXT("World test controller"), Player)) { return false; }
    ULocalPlayer* Local = NewObject<ULocalPlayer>(GEngine);
    Player->Player = Local;
    Local->PlayerController = Player;
    USFHudWidget* WorldHud = CreateWidget<USFHudWidget>(Player, HudClass);
    if (!TestNotNull(TEXT("World HUD"), WorldHud) || !WorldHud->Hotbar) { return false; }
    Player->HudWidget = WorldHud;
    WorldHud->Hotbar->Assign(2, TEXT("build/roof"));
    USFWorldDropTarget* Surface = CreateWidget<USFWorldDropTarget>(Player);
    USFItemDragOperation* WorldDrag = NewObject<USFItemDragOperation>(Player);
    WorldDrag->FromHotbar = 2;
    WorldDrag->WorldDropTarget = Surface;
    if (!TestNotNull(TEXT("World release surface"), Surface)) { return false; }
    Surface->Configure(WorldDrag);
    WorldHud->Hotbar->GetCell(2)->SetHotbarDrag(2);
    WorldHud->Hotbar->GetCell(2)->NativeOnDragCancelled(FDragDropEvent(FPointerEvent(), nullptr), WorldDrag);
    TestEqual(TEXT("Escape/focus cancellation preserves the world source slot"), WorldHud->Hotbar->GetLayout()[2], FString(TEXT("build/roof")));
    Surface->Configure(WorldDrag);
    const FGeometry Viewport = FGeometry::MakeRoot(FVector2D(1920.f, 1080.f), FSlateLayoutTransform());
    const auto ReleaseAt = [](bool bButtonHeld)
    {
        const TSet<FKey> Buttons = bButtonHeld ? TSet<FKey>{EKeys::LeftMouseButton} : TSet<FKey>{};
        return FPointerEvent(0, FVector2D(1500.f, 500.f), FVector2D(1500.f, 500.f), Buttons,
            EKeys::LeftMouseButton, 0, FModifierKeysState());
    };
    TestFalse(TEXT("Held-button motion is not a deliberate world release"), Surface->NativeOnDrop(Viewport, FDragDropEvent(ReleaseAt(true), nullptr), WorldDrag));
    TestEqual(TEXT("Rejected world release retains the shortcut"), WorldHud->Hotbar->GetLayout()[2], FString(TEXT("build/roof")));
    Surface->Configure(WorldDrag);
    TestTrue(TEXT("Left-button release on world removes the shortcut"), Surface->NativeOnDrop(Viewport, FDragDropEvent(ReleaseAt(false), nullptr), WorldDrag));
    TestTrue(TEXT("World release clears only the hotbar assignment"), WorldHud->Hotbar->GetLayout()[2].IsEmpty());
    return true;
}
#endif
