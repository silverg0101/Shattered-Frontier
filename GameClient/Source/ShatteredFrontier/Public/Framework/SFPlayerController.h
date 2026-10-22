#pragma once

#include "GameFramework/PlayerController.h"
#include "Interaction/SFE1InteractionFeedback.h"
#include "Construction/SFConstructionNetwork.h"
#include "SFPlayerController.generated.h"

class ASFE1InteractionTarget;
class ISFExchangeService;
class FSFMarketViewModel;
class FSFTradeViewModel;
class USFGatheringHud;
class USFHudLayout;
class USFMarketBoard;
class USFTradeWindow;
class USFInventoryPanel;
class USFDialoguePanel;
class ASFDialogueNpc;
enum class ESFDialogueStep : uint8;
class UInputAction;

UENUM()
enum class ESFE1InteractionResult : uint8
{
    Accepted, InvalidController, InvalidTarget, StaleTarget, Disabled, OutOfRange, Occluded, Cooldown
};

// GM-01: the one player controller. Member bodies stay in their feature .cpp: interaction, inventory and trading UI in
// SFPlayerController.cpp, build mode, construction RPCs and gathering input in SFConstructionNetwork.cpp.
UCLASS()
class SHATTEREDFRONTIER_API ASFPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    ASFPlayerController();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    // INT-05: with in-flight commits, a closed connection detaches the controller instead of destroying it (linkdead drain).
    virtual void OnNetCleanup(UNetConnection* Connection) override;
    virtual void ClientRestart_Implementation(APawn* NewPawn) override;
    virtual void SetupInputComponent() override;
    virtual void AcknowledgePossession(APawn* P) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void AssignRole(bool bBuilder);
    FString GetBuildStatusText() const;
    // Console (development): grant chr_dev_<name> build rights on the plot the pawn stands in, or claim that plot.
    UFUNCTION(Exec) void SFGrantBuild(const FString& DevName);
    UFUNCTION(Exec) void SFClaimPlot();
    // Console (development, UI evidence captures): toggle one screen by name - inventory, market, trade or build.
    UFUNCTION(Exec) void SFUi(const FString& Screen);
    void InteractWithSelectedTarget();
    void SelectNextCombatTarget();
    AActor* GetSelectedCombatTarget() const;
    UFUNCTION(Server, Reliable) void ServerSelectCombatTarget(AActor* Target);
    UFUNCTION(Client, Unreliable) void ClientPilotHit(const FString& Message);
    FString GetPilotMessage() const;
    void ToggleInventoryPanel();
    // Trading UI: F6 trade window, F7 market board, on the live exchange (the fake only with -ExchangeFake).
    void ToggleTradeDemo();
    void ToggleMarketDemo();
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    // Observes entry before local request coalescing, so input-routing tests can
    // detect duplicate pawn dispatch even when cooldown hides another request.
    uint64 GetInteractionInputAttemptCount() const { return InteractionInputAttempts; }
#endif
    // B toggles build mode where a construction state exists; the character layers IMC_Build on while it lasts.
    bool IsBuildModeActive() const { return bBuildMode; }
    // CTL-02 NPC dialogue (Dialogue/SFPlayerControllerDialogue.cpp): E near an ASFDialogueNpc opens its board; keys 1-4, the rows, Esc and walking away drive it.
    bool IsDialogueOpen() const;
    UFUNCTION(Exec) void SFTalk();
    UFUNCTION(Exec) void SFSpawnDialogueNpc();  // development: stands the Millwarden 4 m ahead and opens his board
    UFUNCTION(Exec) void SFDialogueChoose(int32 Index);  // development: picks response Index (0-3)
    UFUNCTION(Exec) void SFStageAt(float XMetres, float YMetres, float YawDegrees);  // development: stands the pawn on the ground at X,Y facing Yaw (UI captures)
    bool IsStationPanelOpen() const;  // the crafting-station panel is on screen
    void ToggleBuildMode();
protected:
    // The standalone construction sandbox subclass owns its own build input, palette and cursor; it returns false.
    virtual bool HostsBuildMode() const { return true; }
private:
    friend class USFQuestPilotComponent;  // dev scenario driver sends the same placement RPC the cursor does
    UPROPERTY(VisibleAnywhere) TObjectPtr<USFGatheringPilotComponent> GatheringPilot;
    UPROPERTY() TObjectPtr<UActorComponent> PilotInventory;
    void InitializeLocalControls();
    void ApplyCursorMode();
    void LayoutPalette();
    void EnsureGatheringSkillsPanel();
    void ToggleGatheringSkillsPanel();
    void HandleEscape();
    void BeginCameraLook();
    void EndCameraLook();
    void ApplicationActivationChanged(bool bActive);
    bool CanUseConstructionControls() const;
    bool GetConstructionRay(FVector& Start, FVector& Direction) const;
    bool PrepareCursorAction();
    void ToggleDemolition();
    void TogglePlacementPlane();
    void LayerDown();
    void LayerUp();
    FString GetPlaneStatusText() const;
    bool GetLayerPoint(const FVector& Start, const FVector& Direction, double RayLength, const FHitResult* Hit, FVector& OutPoint) const;
    void CancelCursorAction();
    void PaletteAction(ESFConstructionPaletteAction Action);
    void PlaceFromView();
    void RemoveFromView();
    void ToggleDoorFromView();
    void EquipGatheringAxe();
    void EquipGatheringPickaxe();
    void CycleGatheringRecipe();
    void ClaimPlotFromView();  // K: claim the build plot the pawn stands in
    // BLD-02 A2: claim a plot and grant build rights through Housing; the server checks everything with the service.
    UFUNCTION(Server, Reliable) void ServerClaimPlot(const FString& ClaimId);
    UFUNCTION(Server, Reliable) void ServerGrantBuild(const FString& ClaimId, const FString& TargetCharacterId);
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    void TickClaimScenario();  // -ClaimScenario=owner|owner_restart|guest (tools/Test-ClaimHousing.ps1)
    int32 ClaimScenarioStep = 0;
    bool bPlainCheckLogged = false;
    uint32 ClaimScenarioSentSerial = 0;
    double ClaimScenarioNextAt = 0;
#endif
    ESFConstructionNetworkResult LastResultCode = ESFConstructionNetworkResult::InvalidContext;
    uint32 ResultSerial = 0;  // counts ClientPlacementResult replies
    void RefreshPlacementPreview(bool bDrawOutline);
    void ProcessEdit(FIntVector Cell, uint64 ExpectedRevision, bool bRemove, uint8 Shape, uint8 Rotation, const FString& ClaimId);
    // BLD-02 A2: the claim state the pawn stands in (the only state in the sandbox); "" when none.
    class ASFConstructionNetworkState* ActiveState() const;
    FString ActiveClaimId() const;
    void SelectCube();
    void SelectSlab();
    void SelectSlope();
    void SelectDoor();
    void RotatePiece();
    void ToggleLineMode();
    void CancelLine();
    void SelectShape(uint8 Shape);
    UFUNCTION(Server, Reliable) void ServerPlaceLine(FIntVector Start, FIntVector End, uint64 ExpectedRevision, uint8 Shape, uint8 Rotation, const FString& ClaimId);
    UFUNCTION(Server, Reliable) void ServerPlace(FIntVector Cell, uint64 ExpectedRevision, uint8 Shape, uint8 Rotation, const FString& ClaimId);
    UFUNCTION(Server, Reliable) void ServerRemove(FIntVector Cell, uint64 ExpectedRevision, const FString& ClaimId);
    UFUNCTION(Server, Reliable) void ServerPlaceDoor(FIntVector Origin, uint64 ExpectedRevision, uint8 Yaw, const FString& ClaimId);
    UFUNCTION(Server, Reliable) void ServerRemoveDoor(FIntVector Origin, uint64 ExpectedRevision, const FString& ClaimId);
    UFUNCTION(Server, Reliable) void ServerToggleDoor(FIntVector Origin, uint64 ExpectedRevision, const FString& ClaimId);
    UFUNCTION(Client, Reliable) void ClientPlacementResult(ESFConstructionNetworkResult Result, uint64 Revision);
    UPROPERTY(Replicated) bool bRoleAssigned = false;
    UPROPERTY(Replicated) bool bBuilderRole = false; // Advisory; permission is derived again on server.
    uint64 LastRequestFrame = MAX_uint64;
    bool bHousingPending = false;  // INT-04: one housing commit at a time per builder
    bool bHasPlacementPreview = false;
    FIntVector PreviewCell = FIntVector::ZeroValue;
    bool bHasRemovalPreview = false;
    FIntVector RemovalCell = FIntVector::ZeroValue;
    FString PreviewStatus = TEXT("Connecting to construction state...");
    FString LastPlacementStatus = TEXT("No placement requested.");
    uint8 SelectedShape = 0;
    uint8 SelectedRotation = 0;
    uint8 SelectedYaw = 0;
    UPROPERTY(Transient) TObjectPtr<USFConstructionSandboxPalette> Palette;
    UPROPERTY(Transient) TObjectPtr<USFGatheringSkillsPanel> GatheringSkillsPanel;
    FIntPoint SkillsPanelViewportSize = FIntPoint::ZeroValue;
    float SkillsPanelViewportScale = 0;
    bool bCameraLookHeld = false;
    bool bOwnsLookSuppression = false;
    bool bApplicationActive = true;
    bool bDispatchingPaletteAction = false;
    FDelegateHandle ActivationHandle;
    uint64 SampledRevision = MAX_uint64;
    FString CachedPaletteStatus;
    FString CachedPaletteShown;
    FIntPoint PaletteViewportSize = FIntPoint::ZeroValue;
    float PaletteViewportScale = 0;
    bool bLayerPlacement = false;
    int32 PlacementLayer = 0; // Local selection on the released Z0..19 construction lattice.
    bool bDemolitionMode = false;
    bool bDoorSelected = false;
    bool bLineMode = false;
    bool bHasLineSeed = false;
    FIntVector LineStart = FIntVector::ZeroValue;
    uint64 LineRevision = 0;
    struct FLinePreviewCache
    {
        TWeakObjectPtr<ASFConstructionNetworkState> State;
        uint64 Revision = MAX_uint64;
        FIntVector Start = FIntVector::ZeroValue, End = FIntVector::ZeroValue;
        uint8 Shape = 255, Rotation = 255;
        bool bSupported = false;
    } LineCache;
    bool bHasDoorTarget = false;
    FIntVector TargetDoorOrigin = FIntVector::ZeroValue;
    struct FSupportCache
    {
        TWeakObjectPtr<ASFConstructionNetworkState> State;
        uint64 Revision = MAX_uint64;
        FIntVector PlacementCell = FIntVector::ZeroValue;
        FIntVector RemovalCell = FIntVector::ZeroValue;
        uint8 Shape = 255, Rotation = 255;
        bool bHasRemoval = false;
        bool bPlacementAllowed = false, bRemovalAllowed = false;
    } SupportCache;
    FString RemovalStatus = TEXT("Aim at a placed piece to remove it.");
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    friend class FSFConstructionNetworkCursorTest;
    bool bScenarioCursor = false;
    bool bSyntheticPointerOverPalette = false;
    void TickFiniteScenario();
    void ScenarioTap(const FKey& Key);
    TArray<FKey> ScenarioHeldKeys;
    int32 ScenarioBuildModeFrames = 0;
    int32 ScenarioBuildModeSettle = 0;
    int32 ScenarioCursorCheck = 0;
    int32 ScenarioStep = 0;
    double ScenarioStarted = 0;
    FString ScenarioScreenshotPath;
    bool bScenarioLayerReadyReported = false;
    bool bScenarioLayerSeedQueued = false;
    bool bScenarioLayerSeedReported = false;
    bool bScenarioLineSeedQueued = false;
    bool bScenarioLineSeedReported = false;
    bool bScenarioLineReadyReported = false;
    bool bScenarioEmptyReported = false;
    bool bScenarioDoorCameraAimed = false;
    bool bScenarioDoorCapsuleClosedReported = false;
    bool bScenarioDoorCapsuleOpenReported = false;
    bool bScenarioDoorRemoveReported = false;
    bool bScenarioDoorClosedReported = false;
    bool bScenarioDoorOpenReported = false;
    bool bScenarioDoorOpenCaptureStarted = false;
    FString ScenarioDoorOpenPath;
#endif
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    uint64 InteractionInputAttempts = 0;
#endif
    // The owning controller is the network intent endpoint; identity/pawn/position/time are never payloads.
    UFUNCTION(Server, Reliable) void ServerRequestInteraction(AActor* Target, uint32 ExpectedRevision);
    UFUNCTION(Client, Reliable) void ClientInteractionResult(ESFE1InteractionResult Result, float ServerCooldownRemainingSeconds);
    ESFE1InteractionResult ValidateTarget(AActor* Target, uint32 ExpectedRevision) const;
    ESFE1InteractionResult ProcessInteraction(AActor* Target, uint32 ExpectedRevision);
    void RefreshLocalFeedback();

    UPROPERTY(Transient) TObjectPtr<USFInventoryPanel> InventoryPanel;
    UPROPERTY(Transient) TObjectPtr<USFGatheringHud> GatheringHud;
    UPROPERTY(Transient) TObjectPtr<USFHudLayout> HudLayout;
    UPROPERTY(Transient) TObjectPtr<USFTradeWindow> TradeWindow;
    UPROPERTY(Transient) TObjectPtr<USFMarketBoard> MarketBoard;
    TSharedPtr<ISFExchangeService> DemoExchange;  // the fake, or the live client service when this controller has an exchange component
    TSharedPtr<FSFTradeViewModel> DemoTrade;
    TSharedPtr<FSFMarketViewModel> DemoMarket;
    TWeakObjectPtr<ASFE1InteractionTarget> SelectedTarget;
    TArray<TWeakObjectPtr<AActor>> TabTargetSnapshot;
    int32 NextTabTargetIndex = 0;
    double LastTabAt = -1.0e30;
    FString PilotMessage;
    double PilotMessageUntil = 0.0;
    uint32 SelectedRevision = 0;
    double LastAcceptedServerSeconds = -1.0e30;
    double NextLocalRefreshSeconds = 0.0;
    double ResultVisibleUntilSeconds = 0.0;
    FSFE1InteractionFeedback LocalFeedback;
    float InteractionRangeCm = 0.0f;
    float AcceptedCooldownSeconds = 0.0f;
    bool bSettingsCaptured = false;
    ESFE1InteractionResult LastResult = ESFE1InteractionResult::InvalidTarget;

    friend class FSFE1InteractionAuthorityTest;
    void BeginPlayInteraction();
    void TickInteraction();
    void SetBuildMode(bool bActive);
    void CloseSidePanelsForModal();
    bool TryOpenDialogue();
    void CloseDialogue();
    void HandleDialogueStep(ESFDialogueStep Step);
    void TickDialogue();
    UPROPERTY(Transient) TObjectPtr<USFDialoguePanel> DialoguePanel;
    TWeakObjectPtr<ASFDialogueNpc> DialogueNpc;
    bool bBuildMode = false;
};
