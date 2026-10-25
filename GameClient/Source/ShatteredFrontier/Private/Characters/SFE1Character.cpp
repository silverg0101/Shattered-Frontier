#include "Characters/SFE1Character.h"
#include "Characters/SFMetaHumanRetarget.h"
#include "Retargeter/IKRetargeter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#if SF_WITH_MUTABLE
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableSkeletalComponent.h"
#endif
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Framework/SFE1Settings.h"
#include "Framework/SFGameUserSettings.h"
#include "Characters/SFCombatFramingLogic.h"
#include "Characters/SFSwimRules.h"
#include "Characters/SFRoadSpeedVolume.h"
#include "Creatures/SFRestAreaVolume.h"
#include "Combat/SFCombatPilot.h"
#include "Combat/SFAutoAttackRules.h"
#include "Combat/SFTabTargetSelection.h"
#include "Combat/SFCrowdControlComponent.h"
#include "Combat/SFCastingComponent.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Framework/SFPlayerController.h"
#include "Framework/SFGameMode.h"
#include "Services/SFBackpackComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PhysicsVolume.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"
#include "InputCoreTypes.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "Townsfolk/SFTownsfolk.h"
#include "Construction/SFConstructionNetwork.h"
#include "Construction/SFConstructionSandboxController.h"
#include "Characters/SFBackpedalMovement.h"
#include "Characters/SFTravelMountRules.h"
#include "Combat/SFResurrectionAcceptRules.h"

ASFE1Character::ASFE1Character(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<USFBackpedalMovement>(ACharacter::CharacterMovementComponentName))
{
    const USFE1Settings* Settings = GetDefault<USFE1Settings>();
    FString Error;
    if (!Settings->Validate(Error))
    {
        UE_LOG(LogTemp, Fatal, TEXT("Invalid E1 release configuration: %s"), *Error);
    }

    bReplicates = true;
    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, Settings->TurnRateDegreesPerSecond, 0.0f);
    GetCharacterMovement()->MaxWalkSpeed = Settings->RunSpeedMetresPerSecond * 100.0f;
    GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
    GetCharacterMovement()->MaxWalkSpeedCrouched = Settings->CrouchSpeedMetresPerSecond * 100.0f;
    GetCharacterMovement()->MaxSwimSpeed = Settings->SwimSpeedMetresPerSecond * 100.0f;
    GetCharacterMovement()->JumpZVelocity = FMath::Sqrt(2.0f * Settings->GravityMetresPerSecondSquared * Settings->JumpHeightMetres) * 100.0f;
    GetCharacterMovement()->MaxStepHeight = Settings->StepHeightMetres * 100.0f;
    GetCharacterMovement()->AirControl = 0.0f;

    CrowdControl = CreateDefaultSubobject<USFCrowdControlComponent>(TEXT("CrowdControl"));
    Casting = CreateDefaultSubobject<USFCastingComponent>(TEXT("Casting"));
    // Rigged body on the UE4 mannequin skeleton the NPC pipeline also fits. Locomotion comes from the AnimBP reading
    // replicated CharacterMovement velocity and falling state, so remote proxies animate with no extra replication.
    GetMesh()->SetRelativeLocationAndRotation(FVector(0, 0, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()), FRotator(0, -90, 0));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // Skinned body: always invalidate its virtual shadow map pages, so no stale shadow pieces stay behind it.
    GetMesh()->ShadowCacheInvalidationBehavior = EShadowCacheInvalidationBehavior::Always;
    if (!IsRunningDedicatedServer())
    {
        static ConstructorHelpers::FObjectFinder<USkeletalMesh> Body(TEXT("/Game/Mannequin/Character/Mesh/SK_Mannequin.SK_Mannequin"));
        static ConstructorHelpers::FClassFinder<UAnimInstance> Locomotion(TEXT("/Game/Mannequin/Animations/ThirdPerson_AnimBP"));
        GetMesh()->SetSkeletalMeshAsset(Body.Object);
        GetMesh()->SetAnimInstanceClass(Locomotion.Class);
        MetaHumanRetargeter = TSoftObjectPtr<UIKRetargeter>(FSoftObjectPath(TEXT("/Game/Frontier/Characters/Player/Retarget/RTG_SF_MannequinToMetaHuman.RTG_SF_MannequinToMetaHuman")));
        Outfit = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Frontier/Characters/Player/Outfit/CO_PlayerOutfit.CO_PlayerOutfit")));
        OutfitFemale = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Frontier/Characters/Player/Outfit/CO_PlayerOutfit_F.CO_PlayerOutfit_F")));
    }

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = Settings->CameraBoomLengthCm;
    CameraBoom->bUsePawnControlRotation = true;
    CameraBoom->SetRelativeLocation(FVector(0, 0, Settings->CameraPivotHeightCm));
    CameraBoom->bEnableCameraLag = Settings->CameraLagSpeed > 0.0f;
    CameraBoom->CameraLagSpeed = Settings->CameraLagSpeed;
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
}

void ASFE1Character::BeginPlay()
{
    Super::BeginPlay();
    const float WorldGravity = GetWorld()->GetGravityZ();
    if (!FMath::IsFinite(WorldGravity) || WorldGravity >= -UE_SMALL_NUMBER)
    {
        UE_LOG(LogTemp, Fatal, TEXT("E1 fixture requires finite downward world gravity"));
    }
    GetCharacterMovement()->GravityScale = GetDefault<USFE1Settings>()->GravityMetresPerSecondSquared * 100.0f / -WorldGravity;
    if (HasAuthority() && GetWorld()->GetOutermost()->GetName() == TEXT("/Game/Frontier/Maps/Sandbox/SB_TravelMount"))
    { CombatSetHealth(GetDefault<USFCombatPilotSettings>()->PlayerHealth); }
    if (GetWorld()->GetOutermost()->GetName() == TEXT("/Game/Frontier/Maps/Sandbox/SB_Backpedal"))
    {
        bUseControllerRotationYaw = true;
        GetCharacterMovement()->bOrientRotationToMovement = false;
    }
    if (GetNetMode() != NM_DedicatedServer) { SpawnMetaHuman(); }
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (GetNetMode() == NM_Client && (FParse::Param(FCommandLine::Get(), TEXT("CombatPilotScenario")) ||
        FParse::Param(FCommandLine::Get(), TEXT("CombatTargetScenario")) ||
        FParse::Param(FCommandLine::Get(), TEXT("CombatGroupScenario"))))
    { GetWorldTimerManager().SetTimer(CombatScenarioTimer, this, &ASFE1Character::TickCombatScenario, 1.0f, true); }
#endif
}

void ASFE1Character::Landed(const FHitResult& Hit)
{
    const float ImpactMps = FMath::Max(0.0f, -GetVelocity().Z / 100.0f);
    Super::Landed(Hit);
    const FString Map = GetWorld()->GetOutermost()->GetName();
    if (HasAuthority() && (Map == TEXT("/Game/Frontier/Maps/Sandbox/SB_FallDamage") ||
        Map == TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook")))
    {
        if (ASFGameMode* Mode = GetWorld()->GetAuthGameMode<ASFGameMode>())
        { Mode->ApplyFallDamage(this, ImpactMps); }
    }
}

bool ASFE1Character::IsAllowedFaceId(FName Id)
{
    return !Id.IsNone() && GetDefault<USFE1Settings>()->PlayerFaceIds.Contains(Id);
}

// GEAR-01 dev switch: -SFOutfitAssetTest shows the builds of tools/Create-PlayerOutfitAsset.py (tunic as a MetaHuman outfit, no Mutable).
static bool IsOutfitAssetTest()
{
#if UE_BUILD_SHIPPING
    return false;
#else
    static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("SFOutfitAssetTest"));
    return bTest;
#endif
}

FSoftClassPath ASFE1Character::FaceClassPath(FName Id)
{
    if (!IsAllowedFaceId(Id)) { return FSoftClassPath(); }
    const FString Name = Id.ToString();
    const TCHAR* Root = IsOutfitAssetTest() ? TEXT("OutfitAsset/Built") : TEXT("Built");
    return FSoftClassPath(FString::Printf(TEXT("/Game/Frontier/Characters/Player/%s/MHC_%s/BP_MHC_%s.BP_MHC_%s_C"), Root, *Name, *Name, *Name));
}

void ASFE1Character::SetFaceId(FName NewFaceId)
{
    if (!HasAuthority() || NewFaceId == FaceId) { return; }
    if (!IsAllowedFaceId(NewFaceId))
    {
        UE_LOG(LogTemp, Warning, TEXT("PLAYER_FACE_REJECTED %s (not in USFE1Settings::PlayerFaceIds); keeping %s"), *NewFaceId.ToString(), *FaceId.ToString());
        return;
    }
    FaceId = NewFaceId;
    OnRep_FaceId();  // listen server / standalone: show it here too
}

void ASFE1Character::OnRep_FaceId()
{
    if (GetNetMode() == NM_DedicatedServer || !HasActorBegunPlay()) { return; }  // BeginPlay spawns the current face
    if (MetaHuman) { MetaHuman->Destroy(); MetaHuman = nullptr; }
    GetMesh()->SetHiddenInGame(false);  // the mannequin again, should the new face not be built here
    SpawnMetaHuman();
    OnFaceChanged.Broadcast();
}

AActor* ASFE1Character::SpawnPosedMetaHuman(USceneComponent* Parent, const FTransform& Relative) const
{
    const FSoftClassPath ClassPath = FaceClassPath(FaceId);
    UClass* Class = ClassPath.IsNull() ? nullptr : ClassPath.TryLoadClass<AActor>();
    UIKRetargeter* Retargeter = MetaHumanRetargeter.IsNull() ? nullptr : MetaHumanRetargeter.LoadSynchronous();
    if (!Class || !Retargeter || !Parent) { UE_LOG(LogTemp, Warning, TEXT("PLAYER_METAHUMAN_MISSING face %s %s (run tools/Build-PlayerMetaHumans.py); showing the mannequin"), *FaceId.ToString(), *ClassPath.ToString()); return nullptr; }
    FActorSpawnParameters Spawn; Spawn.Owner = const_cast<ASFE1Character*>(this); Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* Body = GetWorld()->SpawnActor<AActor>(Class, Parent->GetComponentTransform(), Spawn);
    if (!Body) { return nullptr; }
    Body->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    Body->GetRootComponent()->SetRelativeTransform(Relative);
    USkeletalMeshComponent* Skin = nullptr;
    for (USkeletalMeshComponent* Part : TInlineComponentArray<USkeletalMeshComponent*>(Body)) { if (Part->GetFName() == TEXT("Body")) { Skin = Part; } }
    if (!Skin) { UE_LOG(LogTemp, Warning, TEXT("PLAYER_METAHUMAN_NO_BODY %s"), *GetNameSafe(Class)); Body->Destroy(); return nullptr; }
    Skin->SetAnimInstanceClass(USFMetaHumanRetargetAnimInstance::StaticClass());
    if (auto* Pose = Cast<USFMetaHumanRetargetAnimInstance>(Skin->GetAnimInstance())) { Pose->SetSource(GetMesh(), Retargeter); }
    Skin->AddTickPrerequisiteComponent(GetMesh());
#if SF_WITH_MUTABLE
    const TSoftObjectPtr<UObject>& Wear = GetDefault<USFE1Settings>()->FemaleFaceIds.Contains(FaceId) ? OutfitFemale : Outfit;
    if (UCustomizableObject* Object = Cast<UCustomizableObject>(Wear.IsNull() || IsOutfitAssetTest() ? nullptr : Wear.LoadSynchronous()))
    {
        // Mutable generates the outfit mesh onto the Body component (keeping its retarget pose); the preset's own clothes give way to it.
        for (USkeletalMeshComponent* Part : TInlineComponentArray<USkeletalMeshComponent*>(Body))
        {
            if (Part != Skin && Part->GetSkeletalMeshAsset() && Part->GetSkeletalMeshAsset()->GetName().Contains(TEXT("_Outfits"))) { Part->SetVisibility(false, true); }
        }
        UCustomizableObjectInstance* Instance = Object->CreateInstance();
        // The outfit's body section carries the bare build's skin; put this face's own body skin back after every update (same topology).
        Instance->UpdatedNativeDelegate.AddWeakLambda(Skin, [Skin, FaceSkin = TWeakObjectPtr<UMaterialInterface>(Skin->GetMaterial(0))](UCustomizableObjectInstance*)
        {
            if (FaceSkin.IsValid()) { Skin->SetMaterial(0, FaceSkin.Get()); }
        });
        UCustomizableSkeletalComponent* Usage = NewObject<UCustomizableSkeletalComponent>(Body, TEXT("PlayerOutfit"));
        Usage->SetCustomizableObjectInstance(Instance);
        Usage->SetupAttachment(Skin);
        Usage->RegisterComponent();
        // The component name comes from the compiled data, so it is read when generating.
        auto Generate = [Instance, Object, Usage = TWeakObjectPtr<UCustomizableSkeletalComponent>(Usage)]()
        {
            if (!Usage.IsValid()) { return; }
            Usage->SetComponentName(Object->GetComponentName(0));
            Instance->UpdateSkeletalMeshAsync(/*bIgnoreCloseDist*/ true, /*bForceHighPriority*/ true);
        };
#if WITH_EDITORONLY_DATA
        // A cooked game ships the object compiled; an editor -game run compiles it on first use, and an update asked for before that fails.
        if (!Object->IsCompiled()) { Object->GetPostCompileDelegate().AddWeakLambda(Instance, Generate); }
        else
#endif
        { Generate(); }
    }
#endif
    return Body;
}

void ASFE1Character::SpawnMetaHuman()
{
    MetaHuman = SpawnPosedMetaHuman(GetMesh(), FTransform::Identity);
    if (!MetaHuman) { return; }
    // The mannequin keeps animating unseen: it is the pose source.
    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    GetMesh()->SetHiddenInGame(true);
    // Keep the first-person / dialogue hiding (TickCameraControls) when the body is swapped for a new face.
    for (UPrimitiveComponent* Part : TInlineComponentArray<UPrimitiveComponent*>(MetaHuman)) { Part->SetOwnerNoSee(GetMesh()->bOwnerNoSee); }
    UE_LOG(LogTemp, Display, TEXT("PLAYER_METAHUMAN_SPAWNED %s"), *GetNameSafe(MetaHuman->GetClass()));
}

void ASFE1Character::EndPlay(const EEndPlayReason::Type Reason)
{
    if (MetaHuman) { MetaHuman->Destroy(); MetaHuman = nullptr; }
    Super::EndPlay(Reason);
}

void ASFE1Character::TickEvadeScenario()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    FString ScenarioRoleName;
    FParse::Value(FCommandLine::Get(), TEXT("CombatEvadeRole="), ScenarioRoleName);
    if (ScenarioRoleName != TEXT("Driver") && ScenarioRoleName != TEXT("Observer")) { return; }
    ASFPilotBoar* Boar = nullptr;
    for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It)
    { if (It->GroupId() == TEXT("evade_sandbox")) { Boar = *It; break; } }
    if (!Boar || !Controller) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now >= NextEvadeSampleAt)
    {
        NextEvadeSampleAt = Now + 0.2;
        UE_LOG(LogTemp, Display, TEXT("BOAR_EVADE_SAMPLE Role=%s T=%.3f X=%.1f Y=%.1f HP=%d Returning=%d PawnX=%.1f Target=%d"),
            *ScenarioRoleName, Now, Boar->GetActorLocation().X, Boar->GetActorLocation().Y,
            Boar->Health(), Boar->IsLeashing() ? 1 : 0, GetActorLocation().X, CombatTarget == Boar ? 1 : 0);
    }
    if (ScenarioRoleName != TEXT("Driver") || PilotHealth <= 0) { return; }
    const FVector Anchor(1800, 0, 0); // Authored SB_Evade boar anchor; only this fixture uses it.
    if (EvadeScenarioStage == 0)
    {
        const FVector ToBoar = (Boar->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
        if (FVector::Dist2D(GetActorLocation(), Boar->GetActorLocation()) > 230)
        { AddMovementInput(ToBoar, 1.0f); return; }
        if (FVector::DotProduct(Controller->GetControlRotation().Vector().GetSafeNormal2D(), ToBoar) < 0.9)
        { Controller->SetControlRotation(ToBoar.Rotation()); return; }
        if (Now >= CombatScenarioNextAttackAt)
        { PilotAttack(); CombatScenarioNextAttackAt = Now + 0.9; }
        if (Boar->Health() < GetDefault<USFCombatPilotSettings>()->BoarHealth)
        { EvadeScenarioStage = 1; UE_LOG(LogTemp, Display, TEXT("BOAR_EVADE_AGGRO HP=%d"), Boar->Health()); }
    }
    else if (EvadeScenarioStage == 1)
    {
        if (GetActorLocation().X < 8400) { AddMovementInput(FVector::ForwardVector, 1.0f); }
        else { EvadeScenarioStage = 2; UE_LOG(LogTemp, Display, TEXT("BOAR_EVADE_LEASH_CROSSED PawnX=%.1f"), GetActorLocation().X); }
    }
    else if (EvadeScenarioStage == 2 && Boar->IsLeashing())
    { EvadeScenarioStage = 3; UE_LOG(LogTemp, Display, TEXT("BOAR_EVADE_RETURNING HP=%d Target=%d"), Boar->Health(), CombatTarget == Boar ? 1 : 0); }
    else if (EvadeScenarioStage == 3 && !Boar->IsLeashing() &&
        Boar->Health() == GetDefault<USFCombatPilotSettings>()->BoarHealth)
    { EvadeScenarioStage = 4; UE_LOG(LogTemp, Display, TEXT("BOAR_EVADE_FULL_RESET HP=%d"), Boar->Health()); }
    else if (EvadeScenarioStage == 4)
    {
        const FVector ToBoar = (Boar->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
        if (FVector::Dist2D(GetActorLocation(), Boar->GetActorLocation()) > 230)
        { AddMovementInput(ToBoar, 1.0f); return; }
        Controller->SetControlRotation(ToBoar.Rotation());
        if (Now >= CombatScenarioNextAttackAt)
        { PilotAttack(); CombatScenarioNextAttackAt = Now + 0.9; }
        if (Boar->Health() < GetDefault<USFCombatPilotSettings>()->BoarHealth)
        { EvadeScenarioStage = 5; UE_LOG(LogTemp, Display, TEXT("BOAR_EVADE_REENGAGED HP=%d"), Boar->Health()); }
    }
#endif
}

void ASFE1Character::TickThreatScenario()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    FString ScenarioRoleName;
    FParse::Value(FCommandLine::Get(), TEXT("CombatThreatRole="), ScenarioRoleName);
    if (ScenarioRoleName != TEXT("Driver") && ScenarioRoleName != TEXT("Observer")) { return; }
    ASFPilotBoar* Boar = nullptr;
    for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It)
    { if (It->GroupId() == TEXT("threat_sandbox")) { Boar = *It; break; } }
    if (!Boar || !Controller || PilotHealth <= 0) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    const ASFE1Character* Victim = Boar->ThreatTarget();
    const TCHAR* TargetRole = Victim == this ? TEXT("Self") : Victim ? TEXT("Other") : TEXT("None");
    if (Now >= NextEvadeSampleAt)
    {
        NextEvadeSampleAt = Now + 0.2;
        UE_LOG(LogTemp, Display, TEXT("BOAR_THREAT_SAMPLE Role=%s T=%.3f HP=%d Target=%s SelfHP=%d PawnX=%.0f BoarX=%.0f"),
            *ScenarioRoleName, Now, Boar->Health(), TargetRole, PilotHealth,
            GetActorLocation().X, Boar->GetActorLocation().X);
    }
    const FVector Toward = (Boar->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
    if (ScenarioRoleName == TEXT("Driver"))
    {
        if (ThreatScenarioStage == 0)
        {
            if (Victim != this) { AddMovementInput(Toward, 1.0f); return; }
            ThreatScenarioStage = 1;
            UE_LOG(LogTemp, Display, TEXT("BOAR_THREAT_INITIAL Role=Driver HP=%d"), Boar->Health());
        }
        else if (ThreatScenarioStage == 1 && Victim && Victim != this && Boar->Health() == 40)
        { ThreatScenarioStage = 2; UE_LOG(LogTemp, Display, TEXT("BOAR_THREAT_SWITCH Role=Driver HP=%d"), Boar->Health()); }
        else if (ThreatScenarioStage == 2)
        {
            if (FVector::Dist2D(GetActorLocation(), Boar->GetActorLocation()) > 230)
            { AddMovementInput(Toward, 1.0f); return; }
            Controller->SetControlRotation(Toward.Rotation());
            if (Now >= CombatScenarioNextAttackAt)
            { PilotAttack(); CombatScenarioNextAttackAt = Now + 0.9; ThreatScenarioStage = 3; }
        }
        else if (ThreatScenarioStage == 3 && Boar->Health() == 20 && Victim && Victim != this)
        { ThreatScenarioStage = 4; UE_LOG(LogTemp, Display, TEXT("BOAR_THREAT_STICK Role=Driver HP=%d"), Boar->Health()); }
    }
    else
    {
        if (ThreatScenarioStage == 0 && Victim && Victim != this)
        { ThreatScenarioStage = 1; UE_LOG(LogTemp, Display, TEXT("BOAR_THREAT_INITIAL Role=Observer HP=%d"), Boar->Health()); }
        else if (ThreatScenarioStage == 1)
        {
            if (FVector::Dist2D(GetActorLocation(), Boar->GetActorLocation()) > 230)
            { AddMovementInput(Toward, 1.0f); return; }
            Controller->SetControlRotation(Toward.Rotation());
            if (Now >= CombatScenarioNextAttackAt)
            { PilotAttack(); CombatScenarioNextAttackAt = Now + 0.9; ThreatScenarioStage = 2; }
        }
        else if (ThreatScenarioStage == 2 && Boar->Health() == 40 && Victim == this)
        { ThreatScenarioStage = 3; UE_LOG(LogTemp, Display, TEXT("BOAR_THREAT_SWITCH Role=Observer HP=%d"), Boar->Health()); }
        else if (ThreatScenarioStage == 3 && Boar->Health() == 20 && Victim == this)
        { ThreatScenarioStage = 4; UE_LOG(LogTemp, Display, TEXT("BOAR_THREAT_STICK Role=Observer HP=%d"), Boar->Health()); }
    }
#endif
}

void ASFE1Character::TickAssistScenario()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    FString ScenarioRole;
    FParse::Value(FCommandLine::Get(), TEXT("CombatAssistRole="), ScenarioRole);
    if ((ScenarioRole != TEXT("Driver") && ScenarioRole != TEXT("Observer")) || !Controller || PilotHealth <= 0) { return; }
    ASFPilotBoar* Lead = nullptr;
    ASFPilotBoar* Mate = nullptr;
    for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It)
    {
        if (It->GroupId() != TEXT("assist_sandbox")) { continue; }
        if (It->ActorHasTag(TEXT("SF_AssistDefensive"))) { Mate = *It; }
        else { Lead = *It; }
    }
    if (!Lead || !Mate) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now >= NextEvadeSampleAt)
    {
        NextEvadeSampleAt = Now + 0.1;
        UE_LOG(LogTemp, Display, TEXT("BOAR_ASSIST_SAMPLE Role=%s T=%.3f Lead=%d Mate=%d LeadHP=%d MateHP=%d SelfHP=%d"),
            *ScenarioRole, Now, Lead->ThreatTarget() ? 1 : 0, Mate->ThreatTarget() ? 1 : 0,
            Lead->Health(), Mate->Health(), PilotHealth);
    }
    if (ScenarioRole == TEXT("Driver"))
    {
        if (AssistScenarioStage == 0)
        {
            if (AssistScenarioReadyAt < 0)
            {
                int32 PresentPawns = 0;
                for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It) { ++PresentPawns; }
                if (PresentPawns < 2) { return; }
                AssistScenarioReadyAt = Now + 3.0;
                UE_LOG(LogTemp, Display, TEXT("BOAR_ASSIST_TWO_CLIENTS_READY"));
            }
            if (Now < AssistScenarioReadyAt) { return; }
            if (Mate->ThreatTarget()) { UE_LOG(LogTemp, Error, TEXT("BOAR_ASSIST_EARLY_TARGET")); return; }
            if (Lead->ThreatTarget() != this)
            { AddMovementInput((Lead->GetActorLocation() - GetActorLocation()).GetSafeNormal2D(), 1.0f); return; }
            AssistScenarioStage = 1;
            UE_LOG(LogTemp, Display, TEXT("BOAR_ASSIST_LEAD_ENGAGED Role=Driver"));
        }
        else if (AssistScenarioStage == 1 && Mate->ThreatTarget() == this)
        { AssistScenarioStage = 2; UE_LOG(LogTemp, Display, TEXT("BOAR_ASSIST_PACK_JOINED Role=Driver")); }
    }
    else
    {
        if (AssistScenarioStage == 0 && Lead->ThreatTarget() && !Mate->ThreatTarget())
        { AssistScenarioStage = 1; UE_LOG(LogTemp, Display, TEXT("BOAR_ASSIST_LEAD_ENGAGED Role=Observer")); }
        else if (AssistScenarioStage == 1 && Mate->ThreatTarget() &&
            Mate->ThreatTarget() == Lead->ThreatTarget() && Mate->ThreatTarget() != this)
        { AssistScenarioStage = 2; UE_LOG(LogTemp, Display, TEXT("BOAR_ASSIST_PACK_JOINED Role=Observer")); }
    }
#endif
}

void ASFE1Character::TickRespawnScenario()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    FString ScenarioRole;
    FParse::Value(FCommandLine::Get(), TEXT("CombatRespawnRole="), ScenarioRole);
    if ((ScenarioRole != TEXT("Driver") && ScenarioRole != TEXT("Observer")) || !Controller || PilotHealth <= 0) { return; }
    ASFPilotBoar* Boar = nullptr;
    for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It)
    { if (It->GroupId() == TEXT("respawn_sandbox")) { Boar = *It; break; } }
    const double Now = GetWorld()->GetTimeSeconds();
    if (RespawnScenarioStage == 0)
    {
        if (!Boar || Boar->Health() != GetDefault<USFCombatPilotSettings>()->BoarHealth) { return; }
        int32 Pawns = 0;
        for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It) { ++Pawns; }
        if (Pawns < 2) { return; }
        if (RespawnScenarioReadyAt < 0) { RespawnScenarioReadyAt = Now + 3.0; }
        if (Now < RespawnScenarioReadyAt) { return; }
        RespawnScenarioStage = 1;
        UE_LOG(LogTemp, Display, TEXT("BOAR_RESPAWN_INITIAL Role=%s HP=%d X=%.0f"),
            *ScenarioRole, Boar->Health(), Boar->GetActorLocation().X);
    }
    if (RespawnScenarioStage == 1 && ScenarioRole == TEXT("Driver") && Boar)
    {
        const FVector Toward = (Boar->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
        if (FVector::Dist2D(GetActorLocation(), Boar->GetActorLocation()) > 220.0)
        { AddMovementInput(Toward, 1.0f); return; }
        Controller->SetControlRotation(Toward.Rotation());
        if (Now >= CombatScenarioNextAttackAt)
        { PilotAttack(); CombatScenarioNextAttackAt = Now + 0.9; }
    }
    if (RespawnScenarioStage == 1 && !Boar)
    {
        RespawnScenarioStage = 2;
        RespawnScenarioDeathAt = Now;
        UE_LOG(LogTemp, Display, TEXT("BOAR_RESPAWN_ABSENT Role=%s T=%.3f"), *ScenarioRole, Now);
    }
    if (RespawnScenarioStage == 2 && Boar)
    {
        RespawnScenarioStage = 3;
        UE_LOG(LogTemp, Display, TEXT("BOAR_RESPAWN_RETURNED Role=%s T=%.3f Elapsed=%.3f HP=%d X=%.0f Y=%.0f"),
            *ScenarioRole, Now, Now - RespawnScenarioDeathAt, Boar->Health(),
            Boar->GetActorLocation().X, Boar->GetActorLocation().Y);
    }
#endif
}

void ASFE1Character::TickAggroScenario()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    FString ScenarioRole;
    FParse::Value(FCommandLine::Get(), TEXT("CombatAggroRole="), ScenarioRole);
    if (ScenarioRole != TEXT("Driver") && ScenarioRole != TEXT("Observer")) { return; }
    ASFPilotBoar* Boar = nullptr;
    for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It)
    { if (It->GroupId() == TEXT("aggro_sandbox")) { Boar = *It; break; } }
    if (!Boar || !Controller || PilotHealth <= 0) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    const ASFE1Character* Target = Boar->ThreatTarget();
    if (Now >= NextEvadeSampleAt)
    {
        NextEvadeSampleAt = Now + 0.2;
        UE_LOG(LogTemp, Display, TEXT("BOAR_AGGRO_SAMPLE Role=%s X=%.0f Y=%.0f DistanceCm=%.0f Target=%s"),
            *ScenarioRole, GetActorLocation().X, GetActorLocation().Y,
            FVector::Dist2D(GetActorLocation(), Boar->GetActorLocation()),
            Target == this ? TEXT("Self") : Target ? TEXT("Other") : TEXT("None"));
    }
    if (ScenarioRole == TEXT("Driver"))
    {
        if (AggroScenarioStage == 0 && FMath::Abs(GetActorLocation().X - 650.0) < 100.0)
        { AggroScenarioStartedAt = Now; AggroScenarioStage = 1; }
        else if (AggroScenarioStage == 1 && Now - AggroScenarioStartedAt >= 2.0)
        {
            if (Target) { UE_LOG(LogTemp, Error, TEXT("BOAR_AGGRO_OCCLUSION_FAILED")); return; }
            UE_LOG(LogTemp, Display, TEXT("BOAR_AGGRO_OCCLUDED DistanceCm=%.0f"),
                FVector::Dist2D(GetActorLocation(), Boar->GetActorLocation()));
            AggroScenarioStage = 2;
        }
        else if (AggroScenarioStage == 2)
        {
            if (Target == this)
            { AggroScenarioStage = 3; UE_LOG(LogTemp, Display, TEXT("BOAR_AGGRO_ACQUIRED DistanceCm=%.0f"),
                FVector::Dist2D(GetActorLocation(), Boar->GetActorLocation())); }
            else if (GetActorLocation().Y < 700.0)
            { AddMovementInput(FVector::RightVector, 1.0f); }
            else if (GetActorLocation().X < 1450.0)
            { AddMovementInput(FVector::ForwardVector, 1.0f); }
        }
    }
    else if (AggroScenarioStage == 0 && FMath::Abs(GetActorLocation().Y + 1200.0) < 100.0)
    { AggroScenarioStage = 1; UE_LOG(LogTemp, Display, TEXT("BOAR_AGGRO_OBSERVER_CLEAR")); }
    else if (AggroScenarioStage == 1 && Target && Target != this)
    { AggroScenarioStage = 2; UE_LOG(LogTemp, Display, TEXT("BOAR_AGGRO_OBSERVER_ACQUIRED")); }
#endif
}

void ASFE1Character::TickMemoryScenario()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    FString ScenarioRoleName;
    FParse::Value(FCommandLine::Get(), TEXT("CombatMemoryRole="), ScenarioRoleName);
    if (ScenarioRoleName != TEXT("Driver") && ScenarioRoleName != TEXT("Observer")) { return; }
    ASFPilotBoar* Boar = nullptr;
    for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It)
    { if (It->GroupId() == TEXT("memory_sandbox")) { Boar = *It; break; } }
    if (!Boar || !Controller || PilotHealth <= 0) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    const ASFE1Character* Victim = Boar->ThreatTarget();
    const FVector Toward = (Boar->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
    if (Now >= NextEvadeSampleAt)
    {
        NextEvadeSampleAt = Now + 1.0;
        UE_LOG(LogTemp, Display, TEXT("BOAR_MEMORY_SAMPLE Role=%s Stage=%d X=%.0f Y=%.0f HP=%d Returning=%d Target=%s"),
            *ScenarioRoleName, MemoryScenarioStage, GetActorLocation().X, GetActorLocation().Y,
            Boar->Health(), Boar->IsLeashing() ? 1 : 0,
            Victim == this ? TEXT("Self") : Victim ? TEXT("Other") : TEXT("None"));
    }
    if (ScenarioRoleName == TEXT("Driver"))
    {
        if (MemoryScenarioStage == 0)
        {
            int32 VisiblePlayers = 0;
            for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It) { ++VisiblePlayers; }
            if (VisiblePlayers < 2) { return; }
            if (FVector::Dist2D(GetActorLocation(), Boar->GetActorLocation()) > 230)
            { AddMovementInput(Toward, 1.0f); return; }
            if (Victim) { UE_LOG(LogTemp, Error, TEXT("BOAR_MEMORY_PREATTACK_TARGET")); return; }
            Controller->SetControlRotation(Toward.Rotation());
            if (Now >= CombatScenarioNextAttackAt)
            {
                UE_LOG(LogTemp, Display, TEXT("BOAR_MEMORY_INITIAL_NEUTRAL"));
                PilotAttack(); CombatScenarioNextAttackAt = Now + 0.9;
            }
            if (Boar->Health() < GetDefault<USFCombatPilotSettings>()->BoarHealth)
            { MemoryScenarioStage = 1; UE_LOG(LogTemp, Display, TEXT("BOAR_MEMORY_DAMAGED HP=%d"), Boar->Health()); }
        }
        else if (MemoryScenarioStage == 1)
        {
            if (GetActorLocation().X < 8400) { AddMovementInput(FVector::ForwardVector, 1.0f); }
            else { MemoryScenarioStage = 2; UE_LOG(LogTemp, Display, TEXT("BOAR_MEMORY_LEASH_CROSSED X=%.0f"), GetActorLocation().X); }
        }
        else if (MemoryScenarioStage == 2 && !Boar->IsLeashing() &&
            Boar->Health() == GetDefault<USFCombatPilotSettings>()->BoarHealth)
        { MemoryScenarioStage = 3; MemoryScenarioStartedAt = Now;
          UE_LOG(LogTemp, Display, TEXT("BOAR_MEMORY_FULL_RESET")); }
        else if (MemoryScenarioStage == 3 && Now - MemoryScenarioStartedAt >= 5.0)
        { MemoryScenarioStage = 4; }
        else if (MemoryScenarioStage == 4)
        {
            if (Victim == this)
            { MemoryScenarioStage = 5; UE_LOG(LogTemp, Display, TEXT("BOAR_MEMORY_REENGAGED HP=%d"), Boar->Health()); }
            else { AddMovementInput(FVector::BackwardVector, 1.0f); }
        }
    }
    else
    {
        if (MemoryScenarioStage == 0)
        {
            // Stand in the boar's unobstructed forward sight cone, inside its normal aggro radius.
            const FVector HoldPoint(2900, 500, GetActorLocation().Z);
            if (FVector::Dist2D(GetActorLocation(), HoldPoint) > 80)
            { AddMovementInput((HoldPoint - GetActorLocation()).GetSafeNormal2D(), 1.0f); return; }
            MemoryScenarioStage = 1;
        }
        if (MemoryScenarioStage == 1)
        {
            if (Boar->Health() < GetDefault<USFCombatPilotSettings>()->BoarHealth || Boar->IsLeashing())
            { bMemorySawCombat = true; }
            if (bMemorySawCombat && !Boar->IsLeashing() &&
                Boar->Health() == GetDefault<USFCombatPilotSettings>()->BoarHealth)
            { MemoryScenarioStage = 2; MemoryScenarioStartedAt = Now; }
        }
        else if (MemoryScenarioStage == 2 && Now - MemoryScenarioStartedAt >= 2.0)
        {
            if (Victim) { UE_LOG(LogTemp, Error, TEXT("BOAR_MEMORY_NEUTRAL_FAILED")); return; }
            MemoryScenarioStage = 3;
            UE_LOG(LogTemp, Display, TEXT("BOAR_MEMORY_NEUTRAL DistanceCm=%.0f"),
                FVector::Dist2D(GetActorLocation(), Boar->GetActorLocation()));
        }
        else if (MemoryScenarioStage == 3 && Victim && Victim != this)
        { MemoryScenarioStage = 4; UE_LOG(LogTemp, Display, TEXT("BOAR_MEMORY_OBSERVED_REENGAGE")); }
    }
#endif
}

void ASFE1Character::TickRegenScenario()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    FString ScenarioRoleName;
    FParse::Value(FCommandLine::Get(), TEXT("CombatRegenRole="), ScenarioRoleName);
    if (ScenarioRoleName != TEXT("Driver") && ScenarioRoleName != TEXT("Observer")) { return; }
    ASFPilotBoar* Boar = nullptr;
    for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It)
    { if (It->GroupId() == TEXT("regeneration_sandbox")) { Boar = *It; break; } }
    if (!Boar || !Controller) { return; }
    ASFE1Character* Watched = this;
    if (ScenarioRoleName == TEXT("Observer"))
    {
        Watched = nullptr;
        for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It)
        { if (*It != this) { Watched = *It; break; } }
        if (GetActorLocation().Y > -3000) { AddMovementInput(-FVector::RightVector, 1.0f); }
    }
    if (!Watched || Watched->CombatHealth() <= 0) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now >= NextRegenSampleAt)
    {
        NextRegenSampleAt = Now + 1.0;
        UE_LOG(LogTemp, Display, TEXT("REGEN_SAMPLE Role=%s Time=%.2f Health=%d Combat=%d X=%.0f BoarReturning=%d"),
            *ScenarioRoleName, Now, Watched->CombatHealth(), Watched->IsPilotInCombat() ? 1 : 0,
            Watched->GetActorLocation().X, Boar->IsLeashing() ? 1 : 0);
    }
    if (ScenarioRoleName == TEXT("Driver"))
    {
        if (RegenScenarioStage == 0)
        {
            int32 Players = 0;
            for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It) { ++Players; }
            if (Players < 2) { return; }
            if (PilotHealth < GetDefault<USFCombatPilotSettings>()->PlayerHealth)
            { RegenScenarioStage = 1; UE_LOG(LogTemp, Display, TEXT("REGEN_DRIVER_HURT Health=%d"), PilotHealth); }
            else { AddMovementInput((Boar->GetActorLocation() - GetActorLocation()).GetSafeNormal2D(), 1.0f); }
        }
        if (RegenScenarioStage == 1 && GetActorLocation().X < 8200)
        { AddMovementInput(FVector::ForwardVector, 1.0f); }
    }
#endif
}

void ASFE1Character::TickRestRegenScenario()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    FString ScenarioRole;
    FParse::Value(FCommandLine::Get(), TEXT("RestRegenRole="), ScenarioRole);
    if ((ScenarioRole != TEXT("Driver") && ScenarioRole != TEXT("Observer")) || !Controller) { return; }
    ASFE1Character* Watched = this;
    int32 Players = 0;
    for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It)
    {
        ++Players;
        if (ScenarioRole == TEXT("Observer") && *It != this) { Watched = *It; }
    }
    if (Players < 2 || (ScenarioRole == TEXT("Observer") && Watched == this) || Watched->CombatHealth() <= 0) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now >= NextRestRegenSampleAt)
    {
        NextRestRegenSampleAt = Now + 1.0;
        UE_LOG(LogTemp, Display, TEXT("REST_REGEN_SAMPLE Role=%s Time=%.2f Health=%d Combat=%d X=%.0f Y=%.0f Z=%.0f Rest=%d"),
            *ScenarioRole, Now, Watched->CombatHealth(), Watched->IsPilotInCombat() ? 1 : 0,
            Watched->GetActorLocation().X, Watched->GetActorLocation().Y, Watched->GetActorLocation().Z,
            ASFRestAreaVolume::IsAtRestArea(GetWorld(), Watched->GetActorLocation()) ? 1 : 0);
    }
    if (ScenarioRole == TEXT("Observer"))
    {
        if (GetActorLocation().Y > -3000) { AddMovementInput(-FVector::RightVector, 1.0f); }
        return;
    }
    ASFPilotBoar* Boar = nullptr;
    for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It)
    { if (It->GroupId() == TEXT("rest_regen_sandbox")) { Boar = *It; break; } }
    if (!Boar) { return; }
    if (RestRegenScenarioStage == 0)
    {
        if (PilotHealth <= 55)
        {
            RestRegenHurtHealth = PilotHealth;
            RestRegenScenarioStage = 1;
            UE_LOG(LogTemp, Display, TEXT("REST_REGEN_DRIVER_HURT Health=%d"), PilotHealth);
        }
        else { AddMovementInput((Boar->GetActorLocation() - GetActorLocation()).GetSafeNormal2D(), 1.0f); }
    }
    if (RestRegenScenarioStage == 1)
    {
        if (GetActorLocation().X < 8400) { AddMovementInput(FVector::ForwardVector, 1.0f); }
        else if (PilotHealth >= RestRegenHurtHealth + 4) { RestRegenScenarioStage = 2; }
    }
    if (RestRegenScenarioStage == 2)
    {
        for (TActorIterator<ASFRestAreaVolume> It(GetWorld()); It; ++It)
        {
            const FVector ToRest = It->GetActorLocation() - GetActorLocation();
            if (ToRest.Size2D() > 150.f) { AddMovementInput(ToRest.GetSafeNormal2D(), 1.0f); }
            break;
        }
    }
#endif
}

void ASFE1Character::TickFallScenario()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    FString ScenarioRole;
    FParse::Value(FCommandLine::Get(), TEXT("CombatFallRole="), ScenarioRole);
    if ((ScenarioRole != TEXT("Driver") && ScenarioRole != TEXT("Observer")) || !Controller) { return; }
    ASFE1Character* Watched = this;
    int32 Players = 0;
    for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It)
    {
        ++Players;
        if (ScenarioRole == TEXT("Observer") && *It != this) { Watched = *It; }
    }
    if (Players < 2 || (Watched == this && ScenarioRole == TEXT("Observer")) || Watched->CombatHealth() <= 0) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now >= NextFallSampleAt)
    {
        NextFallSampleAt = Now + 0.25;
        UE_LOG(LogTemp, Display, TEXT("FALL_SAMPLE Role=%s Time=%.2f Health=%d X=%.0f Z=%.0f"),
            *ScenarioRole, Now, Watched->CombatHealth(), Watched->GetActorLocation().X, Watched->GetActorLocation().Z);
    }
    if (ScenarioRole == TEXT("Driver") && PilotHealth > 0 && GetActorLocation().Z > 300)
    {
        AddMovementInput(FVector::ForwardVector, 1.0f);
    }
#endif
}

void ASFE1Character::TickSwimScenario()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    FString ScenarioRole;
    FParse::Value(FCommandLine::Get(), TEXT("SwimRole="), ScenarioRole);
    if ((ScenarioRole != TEXT("Driver") && ScenarioRole != TEXT("Observer")) || !Controller) { return; }
    ASFE1Character* Watched = this;
    int32 Players = 0;
    for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It)
    {
        ++Players;
        if (ScenarioRole == TEXT("Observer") && *It != this) { Watched = *It; }
    }
    if (Players < 2 || (ScenarioRole == TEXT("Observer") && Watched == this)) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now >= NextSwimSampleAt)
    {
        NextSwimSampleAt = Now + 0.5;
        UE_LOG(LogTemp, Display, TEXT("SWIM_SAMPLE Role=%s Stage=%d X=%.0f Mode=%d Speed=%.0f"),
            *ScenarioRole, SwimScenarioStage, Watched->GetActorLocation().X,
            int32(Watched->GetCharacterMovement()->MovementMode), Watched->GetVelocity().Size2D());
    }
    if (ScenarioRole != TEXT("Driver") || SwimScenarioStage >= 3) { return; }
    if (SwimScenarioStage == 0 && GetCharacterMovement()->IsSwimming()) { SwimScenarioStage = 1; }
    if (SwimScenarioStage == 1 && GetActorLocation().X > 2200) { SwimScenarioStage = 2; }
    if (SwimScenarioStage == 2 && GetActorLocation().X < 1000 && !GetCharacterMovement()->IsSwimming())
    { SwimScenarioStage = 3; UE_LOG(LogTemp, Display, TEXT("SWIM_DRIVER_COMPLETE")); return; }
    AddMovementInput(SwimScenarioStage == 2 ? -FVector::ForwardVector : FVector::ForwardVector, 1.0f);
#endif
}

void ASFE1Character::TickCrouchScenario()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (GetWorld()->GetOutermost()->GetName() != TEXT("/Game/Frontier/Maps/Sandbox/SB_Crouch")) { return; }
    FString ScenarioRole;
    FParse::Value(FCommandLine::Get(), TEXT("CrouchRole="), ScenarioRole);
    if ((ScenarioRole != TEXT("Driver") && ScenarioRole != TEXT("Observer")) || !Controller) { return; }
    ASFE1Character* Watched = this;
    int32 Players = 0;
    for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It)
    {
        ++Players;
        if (ScenarioRole == TEXT("Observer") && *It != this) { Watched = *It; }
    }
    if (Players < 2 || (ScenarioRole == TEXT("Observer") && Watched == this)) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (CrouchScenarioStartAt < 0) { CrouchScenarioStartAt = Now; }
    const double Elapsed = Now - CrouchScenarioStartAt;
    if (ScenarioRole == TEXT("Driver"))
    {
        if (CrouchScenarioStage == 0 && Elapsed >= 0.5 && GetCharacterMovement()->IsMovingOnGround())
        { Crouch(); CrouchScenarioStage = 1; UE_LOG(LogTemp, Display, TEXT("CROUCH_DRIVER_BEGIN")); }
        if (CrouchScenarioStage == 1 && Elapsed >= 4.0)
        { UnCrouch(); CrouchScenarioStage = 2; UE_LOG(LogTemp, Display, TEXT("CROUCH_DRIVER_RETURN")); }
        if (CrouchScenarioStage == 2 && Elapsed >= 7.0)
        { CrouchScenarioStage = 3; UE_LOG(LogTemp, Display, TEXT("CROUCH_DRIVER_COMPLETE")); }
        if ((CrouchScenarioStage == 1 && Elapsed >= 1.0) || CrouchScenarioStage == 2)
        { AddMovementInput(FVector::ForwardVector, 1.0f); }
    }
    if (Now >= NextCrouchSampleAt)
    {
        NextCrouchSampleAt = Now + 0.2;
        UE_LOG(LogTemp, Display, TEXT("CROUCH_CLIENT_SAMPLE Role=%s X=%.0f C=%d Speed=%.0f"),
            *ScenarioRole, Watched->GetActorLocation().X, Watched->bIsCrouched ? 1 : 0,
            Watched->GetVelocity().Size2D());
    }
#endif
}

void ASFE1Character::TickBackpedalScenario()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (GetWorld()->GetOutermost()->GetName() != TEXT("/Game/Frontier/Maps/Sandbox/SB_Backpedal")) { return; }
    FString ScenarioRole;
    FParse::Value(FCommandLine::Get(), TEXT("BackpedalRole="), ScenarioRole);
    if ((ScenarioRole != TEXT("Driver") && ScenarioRole != TEXT("Observer")) || !Controller) { return; }
    ASFE1Character* Watched = this;
    int32 Players = 0;
    for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It)
    {
        ++Players;
        if (ScenarioRole == TEXT("Observer") && *It != this) { Watched = *It; }
    }
    if (Players < 2 || (ScenarioRole == TEXT("Observer") && Watched == this)) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (BackpedalScenarioStartAt < 0) { BackpedalScenarioStartAt = Now; }
    const double Elapsed = Now - BackpedalScenarioStartAt;
    const int32 Stage = Elapsed < 3.0 ? 0 : Elapsed < 6.0 ? 1 : Elapsed < 9.0 ? 2 : 3;
    if (ScenarioRole == TEXT("Driver"))
    {
        Controller->SetControlRotation(FRotator::ZeroRotator);
        if (Stage < 3) { AddMovementInput(Stage == 1 ? -FVector::ForwardVector : FVector::ForwardVector, 1.0f); }
        if (Stage == 3 && !bBackpedalScenarioComplete)
        { bBackpedalScenarioComplete = true; UE_LOG(LogTemp, Display, TEXT("BACKPEDAL_DRIVER_COMPLETE")); }
    }
    if (Now >= NextBackpedalSampleAt)
    {
        NextBackpedalSampleAt = Now + 0.2;
        UE_LOG(LogTemp, Display, TEXT("BACKPEDAL_CLIENT_SAMPLE Role=%s Stage=%d X=%.0f Speed=%.0f Facing=%.2f"),
            *ScenarioRole, Stage, Watched->GetActorLocation().X, Watched->GetVelocity().Size2D(),
            FVector::DotProduct(Watched->GetActorForwardVector(), FVector::ForwardVector));
    }
#endif
}

void ASFE1Character::TickRoadScenario()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    const bool bSnow = GetWorld()->GetOutermost()->GetName() == TEXT("/Game/Frontier/Maps/Sandbox/SB_DeepSnow");
    if (!bSnow && GetWorld()->GetOutermost()->GetName() != TEXT("/Game/Frontier/Maps/Sandbox/SB_RoadSpeed")) { return; }
    FString RoadRole;
    FParse::Value(FCommandLine::Get(), bSnow ? TEXT("SnowRole=") : TEXT("RoadRole="), RoadRole);
    if ((RoadRole != TEXT("Driver") && RoadRole != TEXT("Observer")) || !Controller) { return; }
    ASFE1Character* Watched = this;
    int32 Players = 0;
    for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It)
    {
        ++Players;
        if (RoadRole == TEXT("Observer") && *It != this) { Watched = *It; }
    }
    if (Players < 2 || (RoadRole == TEXT("Observer") && Watched == this)) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (RoadScenarioStartAt < 0) { RoadScenarioStartAt = Now; }
    const double Elapsed = Now - RoadScenarioStartAt;
    if (RoadRole == TEXT("Driver"))
    {
        if (Elapsed < 10.0) { AddMovementInput(FVector::ForwardVector, 1.0f); }
        else if (!bRoadScenarioComplete)
        { bRoadScenarioComplete = true; UE_LOG(LogTemp, Display, TEXT("%s_DRIVER_COMPLETE"), bSnow ? TEXT("SNOW") : TEXT("ROAD")); }
    }
    if (Now >= NextRoadSampleAt)
    {
        NextRoadSampleAt = Now + 0.2;
        UE_LOG(LogTemp, Display, TEXT("%s_CLIENT_SAMPLE Role=%s X=%.0f Speed=%.0f"),
            bSnow ? TEXT("SNOW") : TEXT("ROAD"), *RoadRole, Watched->GetActorLocation().X, Watched->GetVelocity().Size2D());
    }
#endif
}

void ASFE1Character::RequestTravelMountToggle()
{
    if (GetWorld()->GetOutermost()->GetName() != TEXT("/Game/Frontier/Maps/Sandbox/SB_TravelMount")) { return; }
    if (HasAuthority()) { ServerToggleTravelMount_Implementation(); }
    else { ServerToggleTravelMount(); }
}

void ASFE1Character::ServerToggleTravelMount_Implementation()
{
    if (!HasAuthority() || GetWorld()->GetOutermost()->GetName() != TEXT("/Game/Frontier/Maps/Sandbox/SB_TravelMount")) { return; }
    if (bTravelMounted) { DismountTravelMount(TEXT("Player")); return; }
    const APhysicsVolume* Volume = GetPhysicsVolume();
    const bool bRestrictedVolume = Volume && (Volume->bWaterVolume || Volume->ActorHasTag(TEXT("SF_Roof")) || Volume->ActorHasTag(TEXT("SF_NoMount")));
    // SB_TravelMount grants its pilot character one tier-1 travel entitlement as a development fixture.
    const bool bAllowed = SFTravelMountRules::CanBegin(true, PilotHealth > 0, bPilotInCombat,
        GetCharacterMovement()->IsSwimming(), bRestrictedVolume, bTravelMounted, bTravelMountCasting)
        && GetCharacterMovement()->IsMovingOnGround();
    if (!bAllowed)
    {
        UE_LOG(LogTemp, Display, TEXT("MOUNT_SERVER_DENIED Pawn=%s Casting=%d Mounted=%d"),
            *GetName(), bTravelMountCasting ? 1 : 0, bTravelMounted ? 1 : 0);
        return;
    }
    bTravelMountCasting = true;
    TravelMountCastEndsAt = GetWorld()->GetTimeSeconds() + GetDefault<USFE1Settings>()->TravelMountCastSeconds;
    if (bIsCrouched) { UnCrouch(); }
    GetCharacterMovement()->StopMovementImmediately();
    ForceNetUpdate();
    UE_LOG(LogTemp, Display, TEXT("MOUNT_SERVER_CAST Pawn=%s T=%.2f"), *GetName(), GetWorld()->GetTimeSeconds());
}

void ASFE1Character::DismountTravelMount(const TCHAR* Reason)
{
    if (!HasAuthority() || (!bTravelMounted && !bTravelMountCasting)) { return; }
    bTravelMounted = false;
    bTravelMountCasting = false;
    TravelMountCastEndsAt = 0.0;
    ForceNetUpdate();
    UE_LOG(LogTemp, Display, TEXT("MOUNT_SERVER_DISMOUNT Pawn=%s Reason=%s"), *GetName(), Reason);
}

void ASFE1Character::TickTravelMount()
{
    if (!HasAuthority()) { return; }
    const APhysicsVolume* Volume = GetPhysicsVolume();
    const bool bNoMount = Volume && (Volume->ActorHasTag(TEXT("SF_Roof")) || Volume->ActorHasTag(TEXT("SF_NoMount")));
    bool bDeepWater = false;
    if (Volume && Volume->bWaterVolume)
    {
        FVector Origin, Extent;
        Volume->GetActorBounds(false, Origin, Extent);
        bDeepWater = SFSwimRules::IsDeepEnough(Origin.Z + Extent.Z,
            GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),
            GetDefault<USFE1Settings>()->SwimDepthMetres);
    }
    if ((bTravelMounted || bTravelMountCasting) &&
        (PilotHealth <= 0 || bPilotInCombat || bIsCrouched || bNoMount || bDeepWater || GetCharacterMovement()->IsSwimming()))
    { DismountTravelMount(TEXT("State")); return; }
    if (bTravelMountCasting && GetWorld()->GetTimeSeconds() >= TravelMountCastEndsAt)
    {
        if (Volume && Volume->bWaterVolume) { DismountTravelMount(TEXT("Water")); return; }
        bTravelMountCasting = false;
        bTravelMounted = true;
        ForceNetUpdate();
        UE_LOG(LogTemp, Display, TEXT("MOUNT_SERVER_MOUNTED Pawn=%s T=%.2f"), *GetName(), GetWorld()->GetTimeSeconds());
    }
}

void ASFE1Character::TickTravelMountScenario()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    FString ScenarioRole;
    FParse::Value(FCommandLine::Get(), TEXT("TravelMountRole="), ScenarioRole);
    if ((ScenarioRole != TEXT("Driver") && ScenarioRole != TEXT("Observer")) || !Controller) { return; }
    ASFE1Character* Watched = this;
    int32 Players = 0;
    for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It)
    { ++Players; if (ScenarioRole == TEXT("Observer") && *It != this) { Watched = *It; } }
    if (Players < 2 || (ScenarioRole == TEXT("Observer") && Watched == this)) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (TravelMountScenarioStartedAt < 0.0) { TravelMountScenarioStartedAt = Now; }
    const double Elapsed = Now - TravelMountScenarioStartedAt;
    if (ScenarioRole == TEXT("Driver"))
    {
        if (TravelMountScenarioStage == 0) { RequestTravelMountToggle(); TravelMountScenarioStage = 1; }
        if (TravelMountScenarioStage == 1 && Elapsed >= 0.35)
        { RequestTravelMountToggle(); TravelMountScenarioStage = 2; }
        if (Elapsed >= 2.0 && Elapsed < 4.8) { AddMovementInput(FVector::ForwardVector, 1.0f); }
        if (TravelMountScenarioStage == 2 && Elapsed >= 5.0)
        { RequestTravelMountToggle(); TravelMountScenarioStage = 3; }
        if (Elapsed >= 5.5 && Elapsed < 7.5) { AddMovementInput(FVector::ForwardVector, 1.0f); }
        if (Elapsed >= 8.0 && !bTravelMountScenarioCompleted)
        { bTravelMountScenarioCompleted = true; UE_LOG(LogTemp, Display, TEXT("MOUNT_DRIVER_COMPLETE")); }
    }
    if (Now >= NextTravelMountSampleAt)
    {
        NextTravelMountSampleAt = Now + 0.2;
        UE_LOG(LogTemp, Display, TEXT("MOUNT_CLIENT_SAMPLE Role=%s T=%.2f X=%.0f Speed=%.0f Casting=%d Mounted=%d"),
            *ScenarioRole, Now, Watched->GetActorLocation().X, Watched->GetVelocity().Size2D(),
            Watched->IsTravelMountCasting() ? 1 : 0, Watched->IsTravelMounted() ? 1 : 0);
    }
#endif
}

void ASFE1Character::TickGreenlandsMovementScenario()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    FString ScenarioRole;
    FParse::Value(FCommandLine::Get(), TEXT("GreenlandsMovementRole="), ScenarioRole);
    if ((ScenarioRole != TEXT("Driver") && ScenarioRole != TEXT("Observer")) || !Controller) { return; }
    ASFE1Character* Watched = this;
    int32 Players = 0;
    for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It)
    {
        ++Players;
        if (ScenarioRole == TEXT("Observer") && *It != this) { Watched = *It; }
    }
    if (Players < 2 || (ScenarioRole == TEXT("Observer") && Watched == this)) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (Watched->GetActorLocation().Y < -100000.0)
    {
        if (RoadScenarioStartAt < 0) { RoadScenarioStartAt = Now; }
        if (ScenarioRole == TEXT("Driver"))
        {
            Controller->SetControlRotation(FRotator::ZeroRotator);
            if (Now - RoadScenarioStartAt < 16.0) { AddMovementInput(FVector::ForwardVector, 1.0f); }
            else if (!bRoadScenarioComplete)
            { bRoadScenarioComplete = true; UE_LOG(LogTemp, Display, TEXT("GREENLANDS_ROAD_COMPLETE")); }
        }
        if (Now >= NextRoadSampleAt)
        {
            NextRoadSampleAt = Now + 0.2;
            UE_LOG(LogTemp, Display, TEXT("GREENLANDS_ROAD_CLIENT Role=%s X=%.0f Speed=%.0f Road=%d"),
                *ScenarioRole, Watched->GetActorLocation().X, Watched->GetVelocity().Size2D(),
                Cast<ASFRoadSpeedVolume>(Watched->GetPhysicsVolume()) ? 1 : 0);
        }
        return;
    }
    if (Watched->GetActorLocation().X < 49500.0) { return; }
    if (CrouchScenarioStartAt < 0) { CrouchScenarioStartAt = Now; }
    const double Elapsed = Now - CrouchScenarioStartAt;
    const int32 Stage = Elapsed < 3.5 ? 0 : Elapsed < 6.5 ? 1 : Elapsed < 9.5 ? 2 : Elapsed < 12.0 ? 3 : 4;
    if (ScenarioRole == TEXT("Driver"))
    {
        Controller->SetControlRotation(FRotator::ZeroRotator);
        if (Stage == 0 && CrouchScenarioStage == 0 && GetCharacterMovement()->IsMovingOnGround())
        { Crouch(); CrouchScenarioStage = 1; }
        if (Stage >= 1 && CrouchScenarioStage == 1)
        { UnCrouch(); CrouchScenarioStage = 2; }
        if (Stage < 4) { AddMovementInput(Stage == 2 ? -FVector::ForwardVector : FVector::ForwardVector, 1.0f); }
        if (Stage == 4 && !bBackpedalScenarioComplete)
        { bBackpedalScenarioComplete = true; UE_LOG(LogTemp, Display, TEXT("GREENLANDS_MOVEMENT_COMPLETE")); }
    }
    if (Now >= NextCrouchSampleAt)
    {
        NextCrouchSampleAt = Now + 0.2;
        UE_LOG(LogTemp, Display, TEXT("GREENLANDS_MOVEMENT_CLIENT Role=%s Stage=%d X=%.0f C=%d Speed=%.0f Facing=%.2f"),
            *ScenarioRole, Stage, Watched->GetActorLocation().X, Watched->bIsCrouched ? 1 : 0,
            Watched->GetVelocity().Size2D(),
            FVector::DotProduct(Watched->GetActorForwardVector(), FVector::ForwardVector));
    }
#endif
}

void ASFE1Character::TickGreenlandsEnvironmentScenario()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    FString ScenarioRole;
    FParse::Value(FCommandLine::Get(), TEXT("GreenlandsEnvironmentRole="), ScenarioRole);
    if ((ScenarioRole != TEXT("Driver") && ScenarioRole != TEXT("Observer")) || !Controller) { return; }
    ASFE1Character* Watched = this;
    int32 Players = 0;
    for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It)
    {
        ++Players;
        if (ScenarioRole == TEXT("Observer") && *It != this) { Watched = *It; }
    }
    if (Players < 2 || (ScenarioRole == TEXT("Observer") && Watched == this)) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now >= NextSwimSampleAt)
    {
        NextSwimSampleAt = Now + 0.25;
        UE_LOG(LogTemp, Display, TEXT("GREENLANDS_ENV_SAMPLE Role=%s HP=%d X=%.0f Y=%.0f Z=%.0f Mode=%d Speed=%.0f"),
            *ScenarioRole, Watched->CombatHealth(), Watched->GetActorLocation().X, Watched->GetActorLocation().Y,
            Watched->GetActorLocation().Z, int32(Watched->GetCharacterMovement()->MovementMode),
            Watched->GetVelocity().Size2D());
    }
    if (ScenarioRole != TEXT("Driver") || SwimScenarioStage >= 2) { return; }
    if (SwimScenarioStage == 1 && GetActorLocation().X <= 51500.0 &&
        !GetCharacterMovement()->IsSwimming())
    { SwimScenarioStage = 2; UE_LOG(LogTemp, Display, TEXT("GREENLANDS_ENV_COMPLETE")); return; }
    if (CombatHealth() >= GetDefault<USFCombatPilotSettings>()->PlayerHealth || CombatHealth() <= 0) { return; }
    if (SwimScenarioStage == 0 && GetActorLocation().X >= 56000.0) { SwimScenarioStage = 1; }
    AddMovementInput(SwimScenarioStage == 1 ? -FVector::ForwardVector : FVector::ForwardVector, 1.0f);
#endif
}

void ASFE1Character::TickCombatScenario()
{
    if (FParse::Param(FCommandLine::Get(), TEXT("CombatGroupScenario")))
    {
        if (!IsLocallyControlled()) { return; }
        int32 ScenarioRole = 0;
        FParse::Value(FCommandLine::Get(), TEXT("CombatGroupRole="), ScenarioRole);
        const bool bPartyXp = FParse::Param(FCommandLine::Get(), TEXT("PartyXpScenario"));
        ASFPilotBoar* Boar = nullptr;
        double NearestDistance = TNumericLimits<double>::Max();
        for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It)
        {
            if (bPartyXp && (It->GroupId() != TEXT("boar_farm_edge") || It->SlotIndex() != 0)) { continue; }
            const double Distance = FVector::Dist2D(GetActorLocation(), It->GetActorLocation());
            if (It->Health() > 0 && Distance < NearestDistance) { Boar = *It; NearestDistance = Distance; }
        }
        FString CsvDir;
        if (FParse::Value(FCommandLine::Get(), TEXT("M3CsvDir="), CsvDir))
        {
            if (!bM3ClientCaptureStarted && Boar && NearestDistance <= 300.0)
            {
                bM3ClientCaptureStarted = true;
                M3ClientTarget = Boar;
                FCsvProfiler::Get()->BeginCapture(-1, CsvDir, TEXT("client-m3.csv"));
                UE_LOG(LogTemp, Display, TEXT("M3_CLIENT_CSV_START Role=%d"), ScenarioRole);
            }
            if (bM3ClientCaptureStarted && !bM3ClientStopQueued &&
                (!M3ClientTarget.IsValid() || M3ClientTarget->Health() <= 0))
            {
                bM3ClientStopQueued = true;
                FTimerHandle Timer;
                GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, ScenarioRole]()
                {
                    FCsvProfiler::Get()->EndCapture();
                    UE_LOG(LogTemp, Display, TEXT("M3_CLIENT_CSV_STOP Role=%d"), ScenarioRole);
                }), 1.0f, false);
            }
        }
        const int32 MaxAttacks = bPartyXp && ScenarioRole == 1 ? 2 : 1;
        if (ScenarioRole < 1 || ScenarioRole > 3 || CombatScenarioAttackCount >= MaxAttacks || PilotHealth <= 0 ||
            GetWorld()->GetTimeSeconds() < CombatScenarioNextAttackAt) { return; }
        float AttackDelaySeconds = 0.0f;
        if (ScenarioRole == 3 && FParse::Value(FCommandLine::Get(), TEXT("CombatGroupAttackDelaySeconds="), AttackDelaySeconds) &&
            GetGameTimeSinceCreation() < AttackDelaySeconds) { return; }
        if (!Boar || NearestDistance > 300.0) { return; }
        auto* PC = Cast<ASFPlayerController>(Controller);
        if (!PC) { return; }
        if (bPartyXp && (FParse::Param(FCommandLine::Get(), TEXT("PartyXpHoldAttack")) || PC->PartyMemberCount() != 3)) { return; }
        const FVector Aim = (Boar->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
        if (FVector::DotProduct(Controller->GetControlRotation().Vector().GetSafeNormal2D(), Aim) < 0.9)
        { Controller->SetControlRotation(Aim.Rotation()); return; }
        if (CombatTarget != Boar) { PC->SelectNextCombatTarget(); return; }
        UE_LOG(LogTemp, Display, TEXT("COMBAT_GROUP_TARGET Role=%d Target=%s"), ScenarioRole, *GetNameSafe(CombatTarget));
        PilotAttack();
        CombatScenarioAttackCount++;
        CombatScenarioNextAttackAt = GetWorld()->GetTimeSeconds() + 1.0;
        UE_LOG(LogTemp, Display, TEXT("COMBAT_GROUP_ATTACK Role=%d"), ScenarioRole);
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("CombatTargetScenario"))) { TickTargetScenario(); return; }
    if (!IsLocallyControlled() || CombatScenarioAttackCount >= 3) { return; }
    ASFPilotBoar* Nearest = nullptr; double Distance = 300;
    for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It)
    {
        const double D = FVector::Dist2D(GetActorLocation(), It->GetActorLocation());
        if (It->Health() > 0 && D < Distance) { Nearest = *It; Distance = D; }
    }
    if (!Nearest || !Controller || PilotHealth <= 0) { return; }
    const FVector ToBoar = (Nearest->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
    // The server judges facing from the control rotation replicated with movement, so aim on one tick and swing once it has arrived.
    if (FVector::DotProduct(Controller->GetControlRotation().Vector().GetSafeNormal2D(), ToBoar) < 0.9)
    { Controller->SetControlRotation(ToBoar.Rotation()); return; }
    PilotAttack();
    ++CombatScenarioAttackCount;
    UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_CLIENT_ATTACK Index=%d"), CombatScenarioAttackCount);
}

void ASFE1Character::TickTargetScenario()
{
    if (!IsLocallyControlled()) { return; }
    auto* PC = Cast<ASFPlayerController>(Controller);
    if (!PC) { return; }
    if (TargetScenarioStage == 0)
    {
        TArray<ASFPilotBoar*> Nearby;
        for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It)
        { if (It->Health() > 0 && FVector::Dist2D(GetActorLocation(), It->GetActorLocation()) < 2900.0) { Nearby.Add(*It); } }
        if (Nearby.Num() < 2) { return; }
        Nearby.Sort([this](const ASFPilotBoar& A, const ASFPilotBoar& B)
        { return FVector::DistSquared2D(GetActorLocation(), A.GetActorLocation()) < FVector::DistSquared2D(GetActorLocation(), B.GetActorLocation()); });
        const FVector Aim = (Nearby[0]->GetActorLocation() + Nearby[1]->GetActorLocation()) * 0.5 - GetActorLocation();
        PC->SetControlRotation(Aim.Rotation());
        TargetScenarioStage = 1;
        return;
    }
    if (TargetScenarioStage == 1) { PC->SelectNextCombatTarget(); TargetScenarioStage = 2; return; }
    if (TargetScenarioStage == 2 && CombatTarget)
    {
        TargetScenarioFirst = CombatTarget;
        UE_LOG(LogTemp, Display, TEXT("TARGET_PILOT_CLIENT_SELECT Step=1 Target=%s"), *GetNameSafe(CombatTarget));
        PC->SelectNextCombatTarget(); TargetScenarioStage = 3; return;
    }
    if (TargetScenarioStage == 3 && CombatTarget && CombatTarget != TargetScenarioFirst.Get())
    {
        TargetScenarioSecond = CombatTarget;
        UE_LOG(LogTemp, Display, TEXT("TARGET_PILOT_CLIENT_SELECT Step=2 Target=%s"), *GetNameSafe(CombatTarget));
        ServerTargetScenarioStep(1); TargetScenarioStage = 4; return;
    }
    if (TargetScenarioStage == 4 && !CombatTarget && PilotHealth == 0)
    {
        UE_LOG(LogTemp, Display, TEXT("TARGET_PILOT_CLIENT_DROP Cause=Death"));
        ServerTargetScenarioStep(2); TargetScenarioStage = 5; return;
    }
    if (TargetScenarioStage == 5 && PilotHealth > 0)
    { PC->SelectNextCombatTarget(); TargetScenarioStage = 6; return; }
    if (TargetScenarioStage == 6 && CombatTarget)
    {
        TargetScenarioSecond = CombatTarget;
        UE_LOG(LogTemp, Display, TEXT("TARGET_PILOT_CLIENT_RESELECT Target=%s"), *GetNameSafe(CombatTarget));
        ServerTargetScenarioStep(3); TargetScenarioStage = 7; return;
    }
    if (TargetScenarioStage == 7 && !CombatTarget && TargetScenarioSecond.IsValid() &&
        FVector::Dist2D(GetActorLocation(), TargetScenarioSecond->GetActorLocation()) > 4500.0)
    { UE_LOG(LogTemp, Display, TEXT("TARGET_PILOT_CLIENT_DROP Cause=Range")); TargetScenarioStage = 8; }
}

void ASFE1Character::ServerTargetScenarioStep_Implementation(int32 Step)
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (!FParse::Param(FCommandLine::Get(), TEXT("CombatTargetScenario"))) { return; }
    if (Step == 1 && CombatTarget && PilotHealth > 0)
    {
        CombatSetHealth(0);
        UE_LOG(LogTemp, Display, TEXT("TARGET_PILOT_SERVER_DROP Cause=Death Target=None"));
    }
    else if (Step == 2 && PilotHealth == 0)
    { CombatSetHealth(GetDefault<USFCombatPilotSettings>()->PlayerHealth); }
    else if (Step == 3 && CombatTarget && PilotHealth > 0)
    {
        const FVector Away = (GetActorLocation() - CombatTarget->GetActorLocation()).GetSafeNormal2D();
        SetActorLocation(CombatTarget->GetActorLocation() + Away * 6000.0, false, nullptr, ETeleportType::TeleportPhysics);
        UE_LOG(LogTemp, Display, TEXT("TARGET_PILOT_SERVER_MOVE DistanceCm=%.0f"), FVector::Dist2D(GetActorLocation(), CombatTarget->GetActorLocation()));
    }
#endif
}

namespace SFE1CameraControls
{
    // Order matches the IA_* names below; tools/Create-PlayerInputAssets.py creates the assets.
    enum EAction { IA_MoveForward, IA_MoveBack, IA_StrafeRight, IA_StrafeLeft, IA_TurnRight, IA_TurnLeft, IA_Look, IA_LookStick, IA_Zoom,
                   IA_MouseLook, IA_Orbit, IA_Jump, IA_Autorun, IA_Walk, IA_WalkToggle, IA_Crouch, IA_Interact, IA_Count };
    const TCHAR* const ActionNames[IA_Count] = { TEXT("IA_MoveForward"), TEXT("IA_MoveBack"), TEXT("IA_StrafeRight"),
        TEXT("IA_StrafeLeft"), TEXT("IA_TurnRight"), TEXT("IA_TurnLeft"), TEXT("IA_Look"), TEXT("IA_LookStick"), TEXT("IA_Zoom"),
        TEXT("IA_MouseLook"), TEXT("IA_Orbit"), TEXT("IA_Jump"), TEXT("IA_Autorun"), TEXT("IA_Walk"), TEXT("IA_WalkToggle"), TEXT("IA_Crouch"),
        TEXT("IA_Interact") };
}

bool ASFE1Character::UsesCameraControls() const
{
    // Every player controller; IMC_Build is layered on while build mode is active (TickCameraControls).
    return Cast<ASFPlayerController>(Controller) != nullptr;
}

void ASFE1Character::ApplyControlScheme()
{
    // Runs on the server and the owning client so both turn the pawn the same way; facing is the replicated
    // control yaw (ServerMove carries it), the camera orbits on its own local rotation.
    const bool bCameraControls = UsesCameraControls();
    bUseControllerRotationYaw = bCameraControls;
    GetCharacterMovement()->bOrientRotationToMovement = !bCameraControls;
    CameraBoom->bUsePawnControlRotation = !bCameraControls;
    CameraBoom->SetUsingAbsoluteRotation(bCameraControls);
}

void ASFE1Character::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    ApplyControlScheme();
}

void ASFE1Character::PawnClientRestart()
{
    Super::PawnClientRestart();
    ApplyControlScheme();
    APlayerController* PC = Cast<APlayerController>(Controller);
    if (!UsesCameraControls() || !PC || !PC->GetLocalPlayer()) { return; }
    if (!PlayerMapping)
    {
        PlayerMapping = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Frontier/Input/IMC_Player.IMC_Player"));
        BuildMapping = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Frontier/Input/IMC_Build.IMC_Build"));
        for (const TCHAR* Name : SFE1CameraControls::ActionNames)
        { PlayerActions.Add(LoadObject<UInputAction>(nullptr, *FString::Printf(TEXT("/Game/Frontier/Input/%s.%s"), Name, Name))); }
    }
    UEnhancedInputLocalPlayerSubsystem* Input = PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if (!PlayerMapping || !BuildMapping || PlayerActions.Contains(nullptr) || !Input)
    {
        UE_LOG(LogTemp, Error, TEXT("CTL-01 player input assets missing; run tools/Create-PlayerInputAssets.py"));
        return;
    }
    Input->AddMappingContext(PlayerMapping, 0);
    Input->RemoveMappingContext(BuildMapping);  // TickCameraControls adds it back while build mode is active
    bBuildMappingActive = false;
    const USFE1Settings* Settings = GetDefault<USFE1Settings>();
    CameraRotation = FRotator(-15.0f, GetActorRotation().Yaw, 0.0f);
    PC->SetControlRotation(FRotator(0.0f, CameraRotation.Yaw, 0.0f));
    LastControlYaw = CameraRotation.Yaw;
    LastControlPitch = 0.0f;
    DesiredArmCm = Settings->CameraBoomLengthCm;
    CombatFramingBlend = 0.0f;
    CombatDofBlend = 0.0f;
    bFramingEngaged = false;
    LastFramingTarget.Reset();
    CameraBoom->SocketOffset = FVector::ZeroVector;
    FollowCamera->PostProcessBlendWeight = 0.0f;
    SetMouseCaptured(false);
}

void ASFE1Character::SetMouseCaptured(bool bCaptured)
{
    APlayerController* PC = Cast<APlayerController>(Controller);
    if (!PC) { return; }
    bMouseCaptured = bCaptured;
    PC->bShowMouseCursor = !bCaptured;
    if (bCaptured)
    { FInputModeGameOnly Mode; Mode.SetConsumeCaptureMouseDown(false); PC->SetInputMode(Mode); }
    else
    { FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); PC->SetInputMode(Mode); }
}

void ASFE1Character::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const FString CombatMap = GetWorld()->GetOutermost()->GetName();
    if (CombatMap == TEXT("/Game/Frontier/Maps/Sandbox/SB_TravelMount")) { TickTravelMount(); }
    if (CombatMap == TEXT("/Game/Frontier/Maps/Sandbox/SB_AutoAttack") ||
        CombatMap == TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook"))
    {
        if (HasAuthority())
        {
            const double Now = GetWorld()->GetTimeSeconds();
            ASFPilotBoar* Boar = Cast<ASFPilotBoar>(CombatTarget);
            const bool bLiveTarget = IsValid(Boar) && Boar->Health() > 0 &&
                FVector::Dist2D(GetActorLocation(), Boar->GetActorLocation()) <=
                    GetDefault<USFCombatPilotSettings>()->MeleeRangeMetres * 100.0;
            const bool bCasting = Casting && Casting->GetCastView().Phase != ESFCastPhase::Idle;
            if (SFAutoAttackRules::CanSwing(bAutoAttacking, bLiveTarget, PilotHealth > 0, bCasting, Now, NextAutoSwingAt))
            {
                if (ASFGameMode* Mode = GetWorld()->GetAuthGameMode<ASFGameMode>())
                {
                    Mode->TryPlayerAttack(this, Boar);
                    NextAutoSwingAt = Now + SFAutoAttackRules::UnarmedSwingSeconds;
                    UE_LOG(LogTemp, Display, TEXT("AUTO_ATTACK_SERVER_SWING T=%.2f Target=%s Health=%d"),
                        Now, *GetNameSafe(Boar), Boar->Health());
                }
            }
        }
        else if (IsLocallyControlled())
        {
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
            if (FParse::Param(FCommandLine::Get(), TEXT("AutoAttackScenario"))) { TickAutoAttackScenario(); }
            else
#endif
            if (APlayerController* PC = Cast<APlayerController>(Controller); PC && PC->WasInputKeyJustPressed(EKeys::Four))
            { SetAutoAttack(!bAutoAttacking); }
        }
    }
    if (HasAuthority() && (CombatMap == TEXT("/Game/Frontier/Maps/Sandbox/SB_Swimming") ||
        CombatMap == TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook")))
    {
        const APhysicsVolume* Volume = GetPhysicsVolume();
        bool bDeepWater = false;
        if (Volume && Volume->bWaterVolume)
        {
            FVector Origin, Extent;
            Volume->GetActorBounds(false, Origin, Extent);
            const float FeetZ = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            bDeepWater = SFSwimRules::IsDeepEnough(Origin.Z + Extent.Z, FeetZ,
                GetDefault<USFE1Settings>()->SwimDepthMetres);
        }
        if (bDeepWater != GetCharacterMovement()->IsSwimming())
        {
            GetCharacterMovement()->SetMovementMode(bDeepWater ? MOVE_Swimming : MOVE_Walking);
        }
        if (bDeepWater != bLastServerDeepWater)
        {
            bLastServerDeepWater = bDeepWater;
            UE_LOG(LogTemp, Display, TEXT("SWIM_SERVER_MODE Swimming=%d X=%.0f Mode=%d"),
                bDeepWater ? 1 : 0, GetActorLocation().X, int32(GetCharacterMovement()->MovementMode));
        }
    }
    if (HasAuthority() && PilotHealth > 0 &&
        (CombatMap == TEXT("/Game/Frontier/Maps/Sandbox/SB_Regeneration") ||
         CombatMap == TEXT("/Game/Frontier/Maps/Sandbox/SB_RestRegen") ||
         CombatMap == TEXT("/Game/Frontier/Maps/Sandbox/SB_TravelMount") ||
         CombatMap == TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook")))
    {
        bool bThreatened = false;
        for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It)
        { if (It->HasThreatFor(this)) { bThreatened = true; break; } }
        const auto* Settings = GetDefault<USFCombatPilotSettings>();
        const bool bWasInCombat = PilotRegen.bInCombat;
        const bool bRestRegenSandbox = CombatMap == TEXT("/Game/Frontier/Maps/Sandbox/SB_RestRegen");
        const bool bAtRestArea = ASFRestAreaVolume::IsAtRestArea(GetWorld(), GetActorLocation());
        // The pilot has no attribute snapshot yet; use the authored minimum until attributes are integrated.
        const int32 Regenerated = PilotRegen.Advance(GetWorld()->GetTimeSeconds(), bThreatened,
            PilotHealth, Settings->PlayerHealth, 10, Settings->CombatExitSeconds,
            Settings->RegenDelaySeconds, Settings->HealthRegenOocPct, bAtRestArea);
        if (bWasInCombat != PilotRegen.bInCombat)
        {
            bPilotInCombat = PilotRegen.bInCombat;
            UE_LOG(LogTemp, Display, TEXT("REGEN_COMBAT_EXIT Health=%d Time=%.2f"), PilotHealth, GetWorld()->GetTimeSeconds());
            ForceNetUpdate();
        }
        if (Regenerated != PilotHealth)
        {
            CombatSetHealth(Regenerated);
            UE_LOG(LogTemp, Display, TEXT("REGEN_HEALTH_TICK Health=%d Time=%.2f"), PilotHealth, GetWorld()->GetTimeSeconds());
            if (bRestRegenSandbox)
            { UE_LOG(LogTemp, Display, TEXT("REST_REGEN_TICK Health=%d Rest=%d Time=%.2f"), PilotHealth, bAtRestArea ? 1 : 0, GetWorld()->GetTimeSeconds()); }
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
            if (CombatMap == TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook") &&
                FParse::Param(FCommandLine::Get(), TEXT("GreenlandsBoarNetcheck")) &&
                (GreenlandsNetcheckStage == 6 || GreenlandsNetcheckStage == 12))
            { UE_LOG(LogTemp, Display, TEXT("BOAR_GREENLANDS_REGEN_TICK Stage=%d Health=%d Rest=%d Time=%.2f"),
                GreenlandsNetcheckStage, PilotHealth, bAtRestArea ? 1 : 0, GetWorld()->GetTimeSeconds()); }
#endif
        }
    }
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (HasAuthority() && CombatMap == TEXT("/Game/Frontier/Maps/Sandbox/SB_Crouch") &&
        FParse::Param(FCommandLine::Get(), TEXT("CrouchScenario")) &&
        GetWorld()->GetTimeSeconds() >= NextCrouchSampleAt)
    {
        NextCrouchSampleAt = GetWorld()->GetTimeSeconds() + 0.2;
        UE_LOG(LogTemp, Display, TEXT("CROUCH_SERVER_SAMPLE Pawn=%s T=%.2f X=%.0f C=%d Speed=%.0f"),
            *GetName(), GetWorld()->GetTimeSeconds(), GetActorLocation().X,
            bIsCrouched ? 1 : 0, GetVelocity().Size2D());
    }
    if (HasAuthority() && CombatMap == TEXT("/Game/Frontier/Maps/Sandbox/SB_Backpedal") &&
        FParse::Param(FCommandLine::Get(), TEXT("BackpedalScenario")) &&
        GetWorld()->GetTimeSeconds() >= NextBackpedalSampleAt)
    {
        NextBackpedalSampleAt = GetWorld()->GetTimeSeconds() + 0.2;
        UE_LOG(LogTemp, Display, TEXT("BACKPEDAL_SERVER_SAMPLE Pawn=%s T=%.2f X=%.0f Speed=%.0f Dot=%.2f Cap=%.0f"),
            *GetName(), GetWorld()->GetTimeSeconds(), GetActorLocation().X, GetVelocity().Size2D(),
            FVector::DotProduct(GetActorForwardVector(), GetVelocity().GetSafeNormal2D()),
            GetCharacterMovement()->GetMaxSpeed());
    }
    if (HasAuthority() && CombatMap == TEXT("/Game/Frontier/Maps/Sandbox/SB_RoadSpeed") &&
        FParse::Param(FCommandLine::Get(), TEXT("RoadScenario")) &&
        GetWorld()->GetTimeSeconds() >= NextRoadSampleAt)
    {
        NextRoadSampleAt = GetWorld()->GetTimeSeconds() + 0.2;
        UE_LOG(LogTemp, Display, TEXT("ROAD_SERVER_SAMPLE Pawn=%s X=%.0f Speed=%.0f Cap=%.0f"),
            *GetName(), GetActorLocation().X, GetVelocity().Size2D(), GetCharacterMovement()->GetMaxSpeed());
    }
    if (HasAuthority() && CombatMap == TEXT("/Game/Frontier/Maps/Sandbox/SB_DeepSnow") &&
        FParse::Param(FCommandLine::Get(), TEXT("SnowScenario")) &&
        GetWorld()->GetTimeSeconds() >= NextRoadSampleAt)
    {
        NextRoadSampleAt = GetWorld()->GetTimeSeconds() + 0.2;
        UE_LOG(LogTemp, Display, TEXT("SNOW_SERVER_SAMPLE Pawn=%s X=%.0f Speed=%.0f Cap=%.0f"),
            *GetName(), GetActorLocation().X, GetVelocity().Size2D(), GetCharacterMovement()->GetMaxSpeed());
    }
    if (HasAuthority() && CombatMap == TEXT("/Game/Frontier/Maps/Sandbox/SB_TravelMount") &&
        FParse::Param(FCommandLine::Get(), TEXT("TravelMountScenario")) &&
        GetWorld()->GetTimeSeconds() >= NextTravelMountSampleAt)
    {
        NextTravelMountSampleAt = GetWorld()->GetTimeSeconds() + 0.2;
        UE_LOG(LogTemp, Display, TEXT("MOUNT_SERVER_SAMPLE Pawn=%s T=%.2f X=%.0f Speed=%.0f Cap=%.0f Casting=%d Mounted=%d"),
            *GetName(), GetWorld()->GetTimeSeconds(), GetActorLocation().X, GetVelocity().Size2D(),
            GetCharacterMovement()->GetMaxSpeed(), bTravelMountCasting ? 1 : 0, bTravelMounted ? 1 : 0);
    }
    if (HasAuthority() && CombatMap == TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook") &&
        FParse::Param(FCommandLine::Get(), TEXT("GreenlandsMovementNetcheck")) &&
        GetWorld()->GetTimeSeconds() >= NextCrouchSampleAt)
    {
        NextCrouchSampleAt = GetWorld()->GetTimeSeconds() + 0.2;
        UE_LOG(LogTemp, Display, TEXT("GREENLANDS_MOVEMENT_SERVER Pawn=%s T=%.2f X=%.0f C=%d Speed=%.0f Dot=%.2f Cap=%.0f"),
            *GetName(), GetWorld()->GetTimeSeconds(), GetActorLocation().X, bIsCrouched ? 1 : 0,
            GetVelocity().Size2D(), FVector::DotProduct(GetActorForwardVector(), GetVelocity().GetSafeNormal2D()),
            GetCharacterMovement()->GetMaxSpeed());
    }
    if (HasAuthority() && CombatMap == TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook") &&
        FParse::Param(FCommandLine::Get(), TEXT("GreenlandsMovementNetcheck")) &&
        GetActorLocation().Y < -100000.0 && GetWorld()->GetTimeSeconds() >= NextRoadSampleAt)
    {
        NextRoadSampleAt = GetWorld()->GetTimeSeconds() + 0.2;
        UE_LOG(LogTemp, Display, TEXT("GREENLANDS_ROAD_SERVER Pawn=%s X=%.0f Speed=%.0f Cap=%.0f Road=%d"),
            *GetName(), GetActorLocation().X, GetVelocity().Size2D(), GetCharacterMovement()->GetMaxSpeed(),
            Cast<ASFRoadSpeedVolume>(GetPhysicsVolume()) ? 1 : 0);
    }
#endif
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (GetNetMode() == NM_Client && IsLocallyControlled() &&
        FParse::Param(FCommandLine::Get(), TEXT("GreenlandsBoarNetcheck")))
    {
        FString ScenarioRole;
        FParse::Value(FCommandLine::Get(), TEXT("GreenlandsBoarRole="), ScenarioRole);
        if (ScenarioRole == TEXT("Driver") || ScenarioRole == TEXT("Observer"))
        {
            ASFPilotBoar* Lead = nullptr;
            ASFPilotBoar* Mate = nullptr;
            for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It)
            {
                if (It->GroupId() == TEXT("boar_farm_edge"))
                {
                    if (It->SlotIndex() == GreenlandsLeadSlot) { Lead = *It; }
                    if (It->SlotIndex() == GreenlandsMateSlot) { Mate = *It; }
                }
            }
            const double Now = GetWorld()->GetTimeSeconds();
            if (GreenlandsLeadSlot >= 0 && Now >= GreenlandsNextSampleAt)
            {
                GreenlandsNextSampleAt = Now +
                    (GreenlandsNetcheckStage <= 4 || GreenlandsNetcheckStage == 6 ||
                     GreenlandsNetcheckStage == 7 || GreenlandsNetcheckStage == 13 ||
                     GreenlandsNetcheckStage == 14 ? 0.2 : 1.0);
                ASFE1Character* ObservedPlayer = this;
                if (ScenarioRole == TEXT("Driver"))
                {
                    for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It)
                    { if (*It != this) { ObservedPlayer = *It; break; } }
                }
                auto TargetRole = [this](const ASFPilotBoar* Boar) -> const TCHAR*
                {
                    return !Boar || !Boar->ThreatTarget() ? TEXT("None") :
                        Boar->ThreatTarget() == this ? TEXT("Self") : TEXT("Other");
                };
                UE_LOG(LogTemp, Display, TEXT("BOAR_GREENLANDS_SAMPLE Role=%s Stage=%d LeadSlot=%d LeadHP=%d LeadReturn=%d LeadTarget=%s MateSlot=%d MateHP=%d MateReturn=%d MateTarget=%s ObserverHP=%d ObserverCombat=%d ObserverRest=%d"),
                    *ScenarioRole, GreenlandsNetcheckStage, GreenlandsLeadSlot, Lead ? Lead->Health() : -1,
                    Lead && Lead->IsLeashing() ? 1 : 0, TargetRole(Lead), GreenlandsMateSlot,
                    Mate ? Mate->Health() : -1, Mate && Mate->IsLeashing() ? 1 : 0,
                    TargetRole(Mate), ObservedPlayer->CombatHealth(), ObservedPlayer->IsPilotInCombat() ? 1 : 0,
                    ASFRestAreaVolume::IsAtRestArea(GetWorld(), ObservedPlayer->GetActorLocation()) ? 1 : 0);
            }
            if (ScenarioRole == TEXT("Driver") && GreenlandsNetcheckStage == 13 && Lead)
            {
                if (AutoAttackScenarioStage == 0 && CombatTarget == Lead)
                {
                    SetAutoAttack(true);
                    AutoAttackScenarioStage = 1;
                    UE_LOG(LogTemp, Display, TEXT("BOAR_GREENLANDS_AUTO_CLIENT_START Slot=%d"), GreenlandsLeadSlot);
                }
                else if (AutoAttackScenarioStage == 1 && Lead->Health() <= 20)
                {
                    SetAutoAttack(false);
                    AutoAttackScenarioStage = 2;
                    UE_LOG(LogTemp, Display, TEXT("BOAR_GREENLANDS_AUTO_CLIENT_STOP Health=%d"), Lead->Health());
                }
            }
            if (ScenarioRole == TEXT("Driver") && bGreenlandsKill && Lead && Lead->Health() > 0 &&
                Now >= GreenlandsNextAttackAt)
            {
                GreenlandsNextAttackAt = Now + 1.0;
                if (Controller)
                { Controller->SetControlRotation((Lead->GetActorLocation() - GetActorLocation()).Rotation()); }
                PilotAttack();
            }
        }
    }
    static bool bReconnectSawAlive = false;
    static bool bReconnectTravelled = false;
    FString ReconnectUrl;
    if (GetNetMode() == NM_Client && IsLocallyControlled() &&
        FParse::Value(FCommandLine::Get(), TEXT("ReconnectUrl="), ReconnectUrl))
    {
        if (PilotHealth > 0) bReconnectSawAlive = true;
        if (bReconnectSawAlive && !bReconnectTravelled && PilotHealth == 0)
        {
            bReconnectTravelled = true;
            UE_LOG(LogTemp, Display, TEXT("RECONNECT_CLIENT_TRAVEL Face=%s"), *FaceId.ToString());
            if (auto* PC = Cast<APlayerController>(Controller)) PC->ClientTravel(ReconnectUrl, TRAVEL_Absolute);
        }
    }
#endif
    if (HasAuthority() && CombatTarget)
    {
        const ASFPilotBoar* Boar = Cast<ASFPilotBoar>(CombatTarget);
        const ASFTownsfolkNpc* Npc = Cast<ASFTownsfolkNpc>(CombatTarget);
        if (PilotHealth <= 0 || !IsValid(CombatTarget) || CombatTarget->GetWorld() != GetWorld()
            || (Boar && (Boar->Health() <= 0 || Boar->IsLeashing()))
            || (Npc && Npc->Health() <= 0)
            || FSFTabTargetSelection::ShouldDrop(FVector::Dist2D(GetActorLocation(), CombatTarget->GetActorLocation()),
                GetDefault<USFCombatPilotSettings>()->TabTargetDropRangeMetres * 100.0))
        {
            if (FParse::Param(FCommandLine::Get(), TEXT("CombatTargetScenario")) && IsValid(CombatTarget) &&
                FVector::Dist2D(GetActorLocation(), CombatTarget->GetActorLocation()) >
                    GetDefault<USFCombatPilotSettings>()->TabTargetDropRangeMetres * 100.0)
            { UE_LOG(LogTemp, Display, TEXT("TARGET_PILOT_SERVER_DROP Cause=Range Target=None")); }
            SetCombatTarget(nullptr);
        }
    }
    if (IsLocallyControlled() && PlayerMapping && UsesCameraControls()) { TickCameraControls(DeltaSeconds); }
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (GetNetMode() == NM_Client && IsLocallyControlled() &&
        FParse::Param(FCommandLine::Get(), TEXT("CombatEvadeScenario"))) { TickEvadeScenario(); }
    if (GetNetMode() == NM_Client && IsLocallyControlled() &&
        FParse::Param(FCommandLine::Get(), TEXT("CombatThreatScenario"))) { TickThreatScenario(); }
    if (GetNetMode() == NM_Client && IsLocallyControlled() &&
        FParse::Param(FCommandLine::Get(), TEXT("CombatAggroScenario"))) { TickAggroScenario(); }
    if (GetNetMode() == NM_Client && IsLocallyControlled() &&
        FParse::Param(FCommandLine::Get(), TEXT("CombatMemoryScenario"))) { TickMemoryScenario(); }
    if (GetNetMode() == NM_Client && IsLocallyControlled() &&
        FParse::Param(FCommandLine::Get(), TEXT("CombatAssistScenario"))) { TickAssistScenario(); }
    if (GetNetMode() == NM_Client && IsLocallyControlled() &&
        FParse::Param(FCommandLine::Get(), TEXT("CombatRespawnScenario"))) { TickRespawnScenario(); }
    if (GetNetMode() == NM_Client && IsLocallyControlled() &&
        FParse::Param(FCommandLine::Get(), TEXT("CombatRegenScenario"))) { TickRegenScenario(); }
    if (GetNetMode() == NM_Client && IsLocallyControlled() &&
        FParse::Param(FCommandLine::Get(), TEXT("RestRegenScenario"))) { TickRestRegenScenario(); }
    if (GetNetMode() == NM_Client && IsLocallyControlled() &&
        FParse::Param(FCommandLine::Get(), TEXT("CombatFallScenario"))) { TickFallScenario(); }
    if (GetNetMode() == NM_Client && IsLocallyControlled() &&
        FParse::Param(FCommandLine::Get(), TEXT("SwimScenario"))) { TickSwimScenario(); }
    if (GetNetMode() == NM_Client && IsLocallyControlled() &&
        FParse::Param(FCommandLine::Get(), TEXT("CrouchScenario"))) { TickCrouchScenario(); }
    if (GetNetMode() == NM_Client && IsLocallyControlled() &&
        FParse::Param(FCommandLine::Get(), TEXT("BackpedalScenario"))) { TickBackpedalScenario(); }
    if (GetNetMode() == NM_Client && IsLocallyControlled() &&
        FParse::Param(FCommandLine::Get(), TEXT("RoadScenario"))) { TickRoadScenario(); }
    if (GetNetMode() == NM_Client && IsLocallyControlled() &&
        FParse::Param(FCommandLine::Get(), TEXT("SnowScenario"))) { TickRoadScenario(); }
    if (GetNetMode() == NM_Client && IsLocallyControlled() &&
        FParse::Param(FCommandLine::Get(), TEXT("TravelMountScenario"))) { TickTravelMountScenario(); }
    if (IsLocallyControlled() && FParse::Param(FCommandLine::Get(), TEXT("UnstuckScenario"))) { TickUnstuckScenario(); }
    if (CombatMap == TEXT("/Game/Frontier/Maps/Sandbox/SB_SpiritRelease") && IsLocallyControlled())
    {
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
        if (FParse::Param(FCommandLine::Get(), TEXT("SpiritReleaseScenario"))) { TickSpiritReleaseScenario(); }
        else
#endif
        if (PilotHealth == 0)
        { if (ASFPlayerController* PC = Cast<ASFPlayerController>(Controller); PC && PC->WasInputKeyJustPressed(EKeys::R)) { PC->SFReleaseSpirit(); } }
    }
    if (CombatMap == TEXT("/Game/Frontier/Maps/Sandbox/SB_ResurrectionAccept") && IsLocallyControlled())
    {
        if (FParse::Param(FCommandLine::Get(), TEXT("ResurrectionAcceptScenario"))) { TickResurrectionAcceptScenario(); }
        else if (PilotHealth == 0 && ResurrectionOfferId != 0)
        { if (ASFPlayerController* PC = Cast<ASFPlayerController>(Controller); PC && PC->WasInputKeyJustPressed(EKeys::R)) { PC->SFAcceptResurrection(); } }
    }
    if (GetNetMode() == NM_Client && IsLocallyControlled() &&
        FParse::Param(FCommandLine::Get(), TEXT("GreenlandsEnvironmentNetcheck"))) { TickGreenlandsEnvironmentScenario(); }
    if (GetNetMode() == NM_Client && IsLocallyControlled() &&
        FParse::Param(FCommandLine::Get(), TEXT("GreenlandsMovementNetcheck"))) { TickGreenlandsMovementScenario(); }
    if (GetNetMode() == NM_Client && IsLocallyControlled() &&
        FParse::Param(FCommandLine::Get(), TEXT("GreenlandsDeathNetcheck")))
    {
        FString DeathRole, Resolution;
        FParse::Value(FCommandLine::Get(), TEXT("GreenlandsDeathRole="), DeathRole);
        FParse::Value(FCommandLine::Get(), TEXT("DeathResolution="), Resolution);
        ASFE1Character* Subject = this;
        if (DeathRole == TEXT("Observer"))
        {
            for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It)
            { if (*It != this) { Subject = *It; break; } }
        }
        const double Now = GetWorld()->GetTimeSeconds();
        if (GreenlandsDeathClientStage == 0 && FParse::Param(FCommandLine::Get(), TEXT("DeathRestoreOnly")) &&
            Subject->CombatHealth() == 0)
        { GreenlandsDeathClientStage = 1; }
        if (GreenlandsDeathClientStage == 0 && Subject->CombatHealth() == 35)
        {
            GreenlandsDeathClientStage = 1;
            UE_LOG(LogTemp, Display, TEXT("GREENLANDS_DEATH_CLIENT_INITIAL Role=%s HP=35"), *DeathRole);
        }
        if (GreenlandsDeathClientStage == 1 && Subject->CombatHealth() == 0)
        {
            GreenlandsDeathClientStage = 2; GreenlandsDeathClientAt = Now;
            UE_LOG(LogTemp, Display, TEXT("GREENLANDS_DEATH_CLIENT_DEAD Role=%s HP=0"), *DeathRole);
        }
        if (GreenlandsDeathClientStage == 2 && DeathRole == TEXT("Driver") && Now - GreenlandsDeathClientAt >= 12.0)
        {
            if (ASFPlayerController* PC = Cast<ASFPlayerController>(Controller))
            {
                if (Resolution == TEXT("Resurrection"))
                {
                    if (ResurrectionOfferId == 0) return;
                    GreenlandsDeathAcceptedOffer = ResurrectionOfferId;
                    PC->ServerAcceptResurrection(ResurrectionOfferId, FString()); // Wrong event must not consume the offer.
                    PC->SFAcceptResurrection();
                }
                else { PC->SFReleaseSpirit(); }
                GreenlandsDeathClientStage = 3;
                UE_LOG(LogTemp, Display, TEXT("GREENLANDS_DEATH_CLIENT_REQUEST Role=Driver Mode=%s Offer=%u"),
                    *Resolution, GreenlandsDeathAcceptedOffer);
            }
        }
        if (GreenlandsDeathClientStage == 2 && DeathRole == TEXT("Observer")) { GreenlandsDeathClientStage = 3; }
        const int32 Expected = Resolution == TEXT("Resurrection") ? 28 : 40;
        if (GreenlandsDeathClientStage == 3 && Subject->CombatHealth() == Expected)
        {
            GreenlandsDeathClientStage = 4; GreenlandsDeathClientAt = Now;
            UE_LOG(LogTemp, Display, TEXT("GREENLANDS_DEATH_CLIENT_RESTORED Role=%s Mode=%s HP=%d X=%.0f Y=%.0f"),
                *DeathRole, *Resolution, Subject->CombatHealth(), Subject->GetActorLocation().X, Subject->GetActorLocation().Y);
            if (DeathRole == TEXT("Driver"))
            {
                if (ASFPlayerController* PC = Cast<ASFPlayerController>(Controller))
                {
                    if (Resolution == TEXT("Resurrection")) { PC->ServerAcceptResurrection(GreenlandsDeathAcceptedOffer, FString()); }
                    else { PC->SFReleaseSpirit(); }
                }
            }
        }
        if (GreenlandsDeathClientStage == 4 && Now - GreenlandsDeathClientAt >= 2.0)
        {
            GreenlandsDeathClientStage = 5;
            UE_LOG(LogTemp, Display, TEXT("GREENLANDS_DEATH_CLIENT_REPLAY_SAFE Role=%s HP=%d X=%.0f Y=%.0f"),
                *DeathRole, Subject->CombatHealth(), Subject->GetActorLocation().X, Subject->GetActorLocation().Y);
        }
    }
#endif
    if (CombatMap == TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook") &&
        IsLocallyControlled() && PilotHealth == 0)
    {
        if (ASFPlayerController* PC = Cast<ASFPlayerController>(Controller))
        {
            if (PC->WasInputKeyJustPressed(EKeys::R)) { PC->SFReleaseSpirit(); }
            else if (ResurrectionOfferId != 0 && PC->WasInputKeyJustPressed(EKeys::Y)) { PC->SFAcceptResurrection(); }
        }
    }
    if (CombatMap == TEXT("/Game/Frontier/Maps/Sandbox/SB_TravelMount") && IsLocallyControlled())
    { if (APlayerController* PC = Cast<APlayerController>(Controller); PC && PC->WasInputKeyJustPressed(EKeys::M)) { RequestTravelMountToggle(); } }
}

void ASFE1Character::TickCameraControls(float Dt)
{
    using namespace SFE1CameraControls;
    APlayerController* PC = Cast<APlayerController>(Controller);
    UEnhancedInputLocalPlayerSubsystem* Subsystem = PC && PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
    UEnhancedPlayerInput* In = Subsystem ? Subsystem->GetPlayerInput() : nullptr;
    if (!In) { return; }
    const USFE1Settings* S = GetDefault<USFE1Settings>();
    const auto Value = [&](EAction A) { return In->GetActionValue(PlayerActions[A]); };
    const auto Axis = [&](EAction A) { return Value(A).Get<float>(); };
    uint32 Held = 0;
    for (EAction A : { IA_MouseLook, IA_Orbit, IA_Jump, IA_Autorun, IA_Walk, IA_WalkToggle, IA_Crouch, IA_Interact }) { if (Value(A).Get<bool>()) { Held |= 1u << A; } }
    const auto IsHeld = [Held](EAction A) { return (Held & (1u << A)) != 0; };
    const uint32 HeldBefore = HeldLastTick;
    HeldLastTick = Held;
    const auto Pressed = [&](EAction A) { return IsHeld(A) && !(HeldBefore & (1u << A)); };
    const auto Released = [&](EAction A) { return !IsHeld(A) && (HeldBefore & (1u << A)) != 0; };

    // Build mode (GM-01): toggled with B on the player controller; the standalone sandbox fixture is always in it.
    // IMC_Build carries the build actions at a higher priority and consumes their keys from IMC_Player while it lasts.
    const ASFPlayerController* Player = Cast<ASFPlayerController>(PC);
    const bool bBuildMode = PC->IsA<ASFConstructionSandboxController>() || (Player && Player->IsBuildModeActive());
    if (bBuildMode != bBuildMappingActive)
    {
        bBuildMappingActive = bBuildMode;
        if (bBuildMode) { Subsystem->AddMappingContext(BuildMapping, 1); } else { Subsystem->RemoveMappingContext(BuildMapping); }
    }

    // A SetControlRotation from elsewhere (combat aim, construction camera aim, review scripts) moves the camera to it.
    FRotator Control = PC->GetControlRotation();
    if (!FMath::IsNearlyEqual(FRotator::NormalizeAxis(Control.Yaw - LastControlYaw), 0.0f, 0.01f)
        || !FMath::IsNearlyEqual(FRotator::NormalizeAxis(Control.Pitch - LastControlPitch), 0.0f, 0.01f))
    {
        CameraRotation.Yaw = Control.Yaw;
        CameraRotation.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Control.Pitch), S->CameraPitchMinDegrees, S->CameraPitchMaxDegrees);
    }

    // LMB/RMB: a press becomes a camera drag once the cursor moves or the press outlasts a click; then the cursor is
    // hidden and locked, and it comes back where it was when the last drag ends. A short still press is a click.
    const double Now = GetWorld()->GetRealTimeSeconds();
    float CursorX = 0, CursorY = 0;
    PC->GetMousePosition(CursorX, CursorY);
    bool bClickOrbit = false, bClickMouseLook = false;
    const auto UpdateButton = [&](FSFMouseButtonState& B, EAction A, bool& bClicked)
    {
        if (Pressed(A)) { B = FSFMouseButtonState{ true, false, Now, FVector2D(CursorX, CursorY) }; }
        if (B.bDown && !B.bDragging && (Now - B.DownAt > S->ClickMaxSeconds
            || FVector2D::Distance(B.DownCursor, FVector2D(CursorX, CursorY)) > S->ClickDragThresholdPixels))
        { B.bDragging = true; }
        if (Released(A)) { bClicked = B.bDown && !B.bDragging; B.bDown = B.bDragging = false; }
    };
    const FVector2D RestoreCursor = OrbitButton.bDragging ? OrbitButton.DownCursor : MouseLookButton.DownCursor;
    UpdateButton(OrbitButton, IA_Orbit, bClickOrbit);
    UpdateButton(MouseLookButton, IA_MouseLook, bClickMouseLook);
    const bool bDragging = OrbitButton.bDragging || MouseLookButton.bDragging;
    if (bDragging != bMouseCaptured)
    {
        SetMouseCaptured(bDragging);
        if (!bDragging) { PC->SetMouseLocation(FMath::RoundToInt(RestoreCursor.X), FMath::RoundToInt(RestoreCursor.Y)); }
    }
    if (bClickOrbit) { PilotAttack(); }
    if (bClickMouseLook) { Interact(); }

    // Camera rotation: mouse while dragging, right stick always (mouselook semantics).
    const FVector2D Stick = Value(IA_LookStick).Get<FVector2D>();
    FVector2D LookDelta = Stick * S->GamepadLookDegreesPerSecond * Dt;
    if (bDragging) { LookDelta += Value(IA_Look).Get<FVector2D>() * S->MouseLookDegreesPerCount; }
    CameraRotation.Yaw += LookDelta.X;
    CameraRotation.Pitch = FMath::Clamp(CameraRotation.Pitch + LookDelta.Y * (S->bInvertLookY ? -1.0f : 1.0f), S->CameraPitchMinDegrees, S->CameraPitchMaxDegrees);
    const bool bMouseLook = MouseLookButton.bDragging || !Stick.IsNearlyZero(0.05f);

    // Facing: RMB or the stick snaps the character to the camera; otherwise A/D turn it (and the camera behind it,
    // unless LMB is orbiting) or strafe when the setting says so.
    const float TurnAxis = Axis(IA_TurnRight) - Axis(IA_TurnLeft);
    const bool bTurnKeysStrafe = bMouseLook || S->bStrafeWithAD;
    if (bMouseLook) { Control.Yaw = CameraRotation.Yaw; }
    else if (!bTurnKeysStrafe && TurnAxis != 0.0f)
    {
        const float Turn = TurnAxis * S->TurnRateDegreesPerSecond * Dt;
        Control.Yaw += Turn;
        if (!OrbitButton.bDragging) { CameraRotation.Yaw += Turn; }
    }

    // Movement in the facing frame. Walking scales the analog input, which CharacterMovement replicates.
    float Forward = Axis(IA_MoveForward) - Axis(IA_MoveBack);
    const float Strafe = Axis(IA_StrafeRight) - Axis(IA_StrafeLeft) + (bTurnKeysStrafe ? TurnAxis : 0.0f);
    if (Pressed(IA_Autorun)) { bAutorun = !bAutorun; }
    if (Forward != 0.0f) { bAutorun = false; }
    if (bAutorun || (OrbitButton.bDown && MouseLookButton.bDown)) { Forward = 1.0f; }
    if (Pressed(IA_WalkToggle)) { bWalkToggled = !bWalkToggled; }
    if (Pressed(IA_Crouch)) { if (bIsCrouched) { UnCrouch(); } else { Crouch(); } }
    const FRotator Facing(0.0f, Control.Yaw, 0.0f);
    FVector Move = FRotationMatrix(Facing).GetUnitAxis(EAxis::X) * Forward + FRotationMatrix(Facing).GetUnitAxis(EAxis::Y) * Strafe;
    Move = Move.GetClampedToMaxSize(1.0f) * ((IsHeld(IA_Walk) != bWalkToggled) ? S->WalkInputScale : 1.0f);
    if (!Move.IsNearlyZero()) { AddMovementInput(Move, 1.0f); }
    if (Pressed(IA_Jump)) { ACharacter::Jump(); }
    if (Released(IA_Jump)) { StopJumping(); }
    if (Pressed(IA_Interact)) { Interact(); }

    // Optional follow: swing behind the character while it moves and the player is not steering the camera.
    if (S->bCameraFollowWhileMoving && !bDragging && !bMouseLook && GetVelocity().Size2D() > 10.0f)
    { CameraRotation.Yaw = FMath::FInterpTo(CameraRotation.Yaw, CameraRotation.Yaw + FRotator::NormalizeAxis(Control.Yaw - CameraRotation.Yaw), Dt, S->CameraFollowInterpSpeed); }

    // Zoom: wheel steps the target length; in past the minimum is first person (owner mesh hidden), out leaves it.
    const float Wheel = Axis(IA_Zoom);
    if (Wheel > 0.0f && !bFirstPerson)
    {
        DesiredArmCm -= Wheel * S->CameraZoomStepCm;
        if (DesiredArmCm < S->CameraZoomMinCm) { bFirstPerson = true; DesiredArmCm = S->CameraZoomMinCm; }
    }
    else if (Wheel < 0.0f)
    {
        if (bFirstPerson) { bFirstPerson = false; }
        else { DesiredArmCm = FMath::Min(DesiredArmCm - Wheel * S->CameraZoomStepCm, S->CameraZoomMaxCm); }
    }
    // While a dialogue speaker is framed the camera comes in to DialogueArmCm; the player's own zoom (DesiredArmCm) is kept for after.
    const float ArmGoal = bFirstPerson ? 0.0f : IsValid(DialogueFramingTarget.Get()) ? S->DialogueArmCm : DesiredArmCm;
    CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, ArmGoal, Dt, S->CameraZoomInterpSpeed);
    // The own body is hidden in first person, and while a dialogue board is open (it would show behind the board).
    const bool bHideBody = (bFirstPerson && CameraBoom->TargetArmLength < S->CameraZoomMinCm * 0.5f) || IsValid(DialogueFramingTarget.Get());
    if (GetMesh()->bOwnerNoSee != bHideBody)
    {
        GetMesh()->SetOwnerNoSee(bHideBody);
        if (MetaHuman) { for (UPrimitiveComponent* Part : TInlineComponentArray<UPrimitiveComponent*>(MetaHuman)) { Part->SetOwnerNoSee(bHideBody); } }
    }
    CameraBoom->bEnableCameraLag = S->CameraLagSpeed > 0.0f && !bFirstPerson;

    // CTL-02: all camera changes are local. The owner-only CombatTarget is also the selected Tab target.
    const USFGameUserSettings* Preferences = GEngine ? Cast<USFGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
    AActor* FrameTarget = IsValid(DialogueFramingTarget.Get()) ? DialogueFramingTarget.Get() : CombatTarget.Get();
    if (!IsValid(FrameTarget) || FrameTarget->IsActorBeingDestroyed() || FrameTarget->GetWorld() != GetWorld()) { FrameTarget = nullptr; }
    const bool bWanted = SFCombatFramingLogic::ShouldEngage(bFramingEngaged, FrameTarget != nullptr,
        S->bCombatFramingEnabled && (!Preferences || Preferences->bCombatFraming), bFirstPerson,
        DesiredArmCm, S->CombatFramingMaxZoomCm);
    if (FrameTarget && FrameTarget != LastFramingTarget.Get())
    {
        // A dialogue speaker is framed at once, beside the board (x 20 % of the screen) at eye level, rather than eased in.
        if (FrameTarget == DialogueFramingTarget.Get())
        {
            const FRotator ToHead = (DialogueHeadPoint(FrameTarget) - FollowCamera->GetComponentLocation()).Rotation();
            CameraRotation.Yaw = ToHead.Yaw + S->DialogueFramingYawDegrees;
            CameraRotation.Pitch = FMath::Clamp(ToHead.Pitch - S->DialogueHeadAboveCentreDegrees, S->CameraPitchMinDegrees, S->CameraPitchMaxDegrees);
        }
        // Keep the shoulder continuous; the new yaw and focus ease from their current values.
        FramingOccludedSince = -1.0;
        LastFramingProbeAt = -100.0;
    }
    LastFramingTarget = FrameTarget;
    bFramingEngaged = bWanted;
    CombatFramingBlend = SFCombatFramingLogic::AdvanceBlend(CombatFramingBlend, bWanted, Dt, S->CombatFramingBlendSeconds);
    const float Side = Preferences && Preferences->CombatShoulderSide == ESFCombatShoulderSide::Left ? -1.0f : 1.0f;
    CameraBoom->SocketOffset = FVector(0.0f, Side * S->CombatShoulderOffsetCm,
        S->CombatShoulderHeightCm) * CombatFramingBlend;

    const bool bSteering = OrbitButton.bDown || MouseLookButton.bDown || !Stick.IsNearlyZero(0.05f);
    if (bSteering) { LastFramingSteerAt = Now; }
    if (FrameTarget && bWanted && Now - LastFramingProbeAt >= S->CombatOcclusionProbeSeconds)
    {
        LastFramingProbeAt = Now;
        FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(CombatFramingVisibility), false, this);
        const FVector Centre = FrameTarget->GetActorLocation();
        const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, FollowCamera->GetComponentLocation(), Centre,
            ECC_Visibility, Query) && Hit.GetActor() != FrameTarget;
        bFramingVisible = !bBlocked;
        if (bBlocked && FramingOccludedSince < 0.0) { FramingOccludedSince = Now; }
        if (!bBlocked) { FramingOccludedSince = -1.0; }
    }
    const bool bOccluded = !bFramingVisible && FramingOccludedSince >= 0.0
        && Now - FramingOccludedSince > S->CombatOcclusionGraceSeconds;
    if (FrameTarget && FrameTarget == DialogueFramingTarget.Get() && bWanted && !bSteering)
    {
        // Dialogue holds the speaker beside the board (about x 20 % of the screen) at eye level. The board is modal, so there is
        // no deadzone or occlusion release; the angle is measured from the camera, so it is the speaker's place on screen.
        // Pitch aims at the speaker's head, held DialogueHeadAboveCentreDegrees above the screen centre (y 30 %).
        const FRotator ToHead = (DialogueHeadPoint(FrameTarget) - FollowCamera->GetComponentLocation()).Rotation();
        const float AimYaw = ToHead.Yaw + S->DialogueFramingYawDegrees;
        const float AimPitch = FMath::Clamp(ToHead.Pitch - S->DialogueHeadAboveCentreDegrees, S->CameraPitchMinDegrees, S->CameraPitchMaxDegrees);
        CameraRotation.Yaw += FMath::FInterpTo(0.0f, FRotator::NormalizeAxis(AimYaw - CameraRotation.Yaw), Dt, S->CombatFramingYawSpeed);
        CameraRotation.Pitch = FMath::FInterpTo(CameraRotation.Pitch, AimPitch, Dt, S->CombatFramingYawSpeed);
    }
    else if (FrameTarget && bWanted && !bOccluded && !bSteering
        && Now - LastFramingSteerAt >= S->CombatFramingSteerHoldSeconds)
    {
        const FRotator ToTarget = (FrameTarget->GetActorLocation() - CameraBoom->GetComponentLocation()).Rotation();
        CameraRotation.Yaw = SFCombatFramingLogic::AssistAxis(CameraRotation.Yaw, ToTarget.Yaw,
            S->CombatFramingDeadzoneDegrees, S->CombatFramingYawSpeed, CombatFramingBlend, Dt);
        const float DesiredPitch = FMath::Clamp(FRotator::NormalizeAxis(ToTarget.Pitch) + S->CombatPitchBiasDegrees,
            S->CameraPitchMinDegrees, S->CameraPitchMaxDegrees);
        CameraRotation.Pitch = FMath::Clamp(SFCombatFramingLogic::AssistAxis(CameraRotation.Pitch, DesiredPitch,
            S->CombatFramingDeadzoneDegrees, S->CombatFramingYawSpeed, CombatFramingBlend, Dt),
            S->CameraPitchMinDegrees, S->CameraPitchMaxDegrees);
    }
    // Physical DoF has one focal plane: the target is sharp, while the near shoulder is mildly soft.
    const FVector FocusPoint = FrameTarget && !bOccluded ? FrameTarget->GetActorLocation() : GetActorLocation();
    const float DesiredFocusCm = FVector::Distance(FollowCamera->GetComponentLocation(), FocusPoint);
    CombatFocusDistanceCm = CombatFocusDistanceCm <= 0.0f ? DesiredFocusCm
        : FMath::FInterpTo(CombatFocusDistanceCm, DesiredFocusCm, Dt, 1.0f / S->CombatFramingBlendSeconds);
    FollowCamera->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
    FollowCamera->PostProcessSettings.bOverride_DepthOfFieldFstop = true;
    FollowCamera->PostProcessSettings.bOverride_DepthOfFieldSensorWidth = true;
    FollowCamera->PostProcessSettings.DepthOfFieldFocalDistance = CombatFocusDistanceCm;
    FollowCamera->PostProcessSettings.DepthOfFieldFstop = S->CombatDofFstop;
    FollowCamera->PostProcessSettings.DepthOfFieldSensorWidth = S->CombatDofSensorWidthMm;
    const float DesiredDofBlend = Preferences && !Preferences->bCombatDepthOfField ? 0.0f
        : CombatFramingBlend * S->CombatDofStrength * (bOccluded ? 0.0f : 1.0f);
    CombatDofBlend = FMath::FInterpTo(CombatDofBlend, DesiredDofBlend, Dt, 1.0f / S->CombatFramingBlendSeconds);
    FollowCamera->PostProcessBlendWeight = CombatDofBlend;

    CameraRotation.Yaw = FRotator::NormalizeAxis(CameraRotation.Yaw);
    CameraBoom->SetWorldRotation(FRotator(CameraRotation.Pitch, CameraRotation.Yaw, 0.0f));
    Control.Yaw = FRotator::NormalizeAxis(Control.Yaw);
    Control.Pitch = CameraRotation.Pitch;
    PC->SetControlRotation(Control);
    LastControlYaw = Control.Yaw;
    LastControlPitch = Control.Pitch;
}

void ASFE1Character::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFE1Character, PilotHealth);
    DOREPLIFETIME_CONDITION(ASFE1Character, ResurrectionOfferId, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(ASFE1Character, ResurrectionOfferDeathEventId, COND_OwnerOnly);
    DOREPLIFETIME(ASFE1Character, bPilotInCombat);
    DOREPLIFETIME(ASFE1Character, bAutoAttacking);
    DOREPLIFETIME(ASFE1Character, bTravelMounted);
    DOREPLIFETIME(ASFE1Character, bTravelMountCasting);
    DOREPLIFETIME(ASFE1Character, GreenlandsNetcheckStage);
    DOREPLIFETIME(ASFE1Character, GreenlandsLeadSlot);
    DOREPLIFETIME(ASFE1Character, GreenlandsMateSlot);
    DOREPLIFETIME(ASFE1Character, bGreenlandsKill);
    DOREPLIFETIME(ASFE1Character, FaceId);
    DOREPLIFETIME_CONDITION(ASFE1Character, CombatTarget, COND_OwnerOnly);
}

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
void ASFE1Character::SetGreenlandsNetcheckStage(int32 Stage, int32 LeadSlot, int32 MateSlot, bool bKill)
{
    if (!HasAuthority()) { return; }
    GreenlandsNetcheckStage = Stage;
    GreenlandsLeadSlot = LeadSlot;
    GreenlandsMateSlot = MateSlot;
    bGreenlandsKill = bKill;
    ForceNetUpdate();
}
#endif

void ASFE1Character::SetCombatTarget(AActor* Target)
{
    if (HasAuthority() && CombatTarget != Target)
    {
        CombatTarget = Target;
        ForceNetUpdate();
        if (Target && FParse::Param(FCommandLine::Get(), TEXT("CombatTargetScenario")))
        { UE_LOG(LogTemp, Display, TEXT("TARGET_PILOT_SERVER_SELECT Target=%s"), *GetNameSafe(Target)); }
        if (FParse::Param(FCommandLine::Get(), TEXT("CombatGroupScenario")))
        {
            USFBackpackComponent* Bag = USFBackpackComponent::FindOrCreate(Cast<APlayerController>(GetController()));
            UE_LOG(LogTemp, Display, TEXT("COMBAT_GROUP_SERVER_TARGET Character=%s Target=%s"),
                Bag ? *Bag->GetCharacterId() : TEXT("none"), *GetNameSafe(Target));
        }
    }
}

// The top of the speaker's bounds, a little down: its head, whatever the body's pose or origin.
FVector ASFE1Character::DialogueHeadPoint(const AActor* Speaker)
{
    FVector Origin, Extent; Speaker->GetActorBounds(true, Origin, Extent);
    return FVector(Origin.X, Origin.Y, Origin.Z + Extent.Z - 12.0f);
}

void ASFE1Character::SetDialogueFramingTarget(AActor* Speaker)
{
    if (IsLocallyControlled()) { DialogueFramingTarget = Speaker; }
}

void ASFE1Character::CombatSetHealth(int32 Value)
{
    if (HasAuthority())
    {
        if (Value < PilotHealth && (bTravelMounted || bTravelMountCasting)) { DismountTravelMount(TEXT("Damage")); }
        if (Value < PilotHealth) { CombatMarkHostile(); }
        if (Value < PilotHealth && Casting) { Casting->NotifyDamage(); }
        PilotHealth = Value;
        if (Value > 0 && ResurrectionOfferId != 0) { ClearResurrectionOffer(); }
        if (Value <= 0) { SetCombatTarget(nullptr); }
        ForceNetUpdate();
    }
}

void ASFE1Character::BeginResurrectionOffer(double ExpiresAt, const FString& DeathEventId)
{
    if (!HasAuthority()) return;
    ++NextResurrectionOfferId;
    if (NextResurrectionOfferId == 0) ++NextResurrectionOfferId;
    ResurrectionOfferId = NextResurrectionOfferId;
    ResurrectionOfferDeathEventId = DeathEventId;
    ResurrectionOfferExpiresAt = ExpiresAt;
    ForceNetUpdate();
}

void ASFE1Character::ClearResurrectionOffer()
{
    if (!HasAuthority()) return;
    ResurrectionOfferId = 0;
    ResurrectionOfferDeathEventId.Empty();
    ResurrectionOfferExpiresAt = 0.0;
    ForceNetUpdate();
}

bool ASFE1Character::AcceptResurrectionOffer(uint32 RequestedOffer, double Now, int32 MaximumHealth, float Fraction)
{
    if (!CanAcceptResurrectionOffer(RequestedOffer, Now)) return false;
    ClearResurrectionOffer();
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    CombatSetHealth(FSFResurrectionAcceptRules::RestoredHealth(MaximumHealth, Fraction));
    bPilotInCombat = false;
    ForceNetUpdate();
    return true;
}

bool ASFE1Character::CanAcceptResurrectionOffer(uint32 RequestedOffer, double Now) const
{
    const FString Map = GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString();
    const bool bSupported = Map == TEXT("/Game/Frontier/Maps/Sandbox/SB_ResurrectionAccept") ||
        Map == TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook");
    return FSFResurrectionAcceptRules::CanAccept(bSupported, HasAuthority(), GetController() != nullptr,
        PilotHealth, ResurrectionOfferId, RequestedOffer, Now, ResurrectionOfferExpiresAt);
}

void ASFE1Character::CombatMarkHostile()
{
    const FString CombatMap = GetWorld()->GetOutermost()->GetName();
    if (!HasAuthority() ||
        (CombatMap != TEXT("/Game/Frontier/Maps/Sandbox/SB_Regeneration") &&
         CombatMap != TEXT("/Game/Frontier/Maps/Sandbox/SB_RestRegen") &&
         CombatMap != TEXT("/Game/Frontier/Maps/Sandbox/SB_TravelMount") &&
         CombatMap != TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook"))) { return; }
    PilotRegen.Hostile(GetWorld()->GetTimeSeconds());
    if (!bPilotInCombat) { bPilotInCombat = true; ForceNetUpdate(); }
}

void ASFE1Character::PilotAttack()
{
    if (!IsLocallyControlled()) { return; }
    AActor* Selected = Cast<ASFPlayerController>(Controller) ? Cast<ASFPlayerController>(Controller)->GetSelectedCombatTarget() : nullptr;
    if (ASFTownsfolkNpc* Npc = Cast<ASFTownsfolkNpc>(Selected))
    {
        if (USFTownsfolkHandoffComponent* Link = Controller->FindComponentByClass<USFTownsfolkHandoffComponent>())
        { Link->ServerAttackSelectedNpc(Npc); }
        return;
    }
    if (Selected) { if (PilotHealth > 0) { ServerPilotAttack(Selected); } return; }
    // A townsfolk resident in reach takes the swing (hand-off); otherwise it goes to the combat pilot.
    if (USFTownsfolkSubsystem* Town = GetWorld()->GetSubsystem<USFTownsfolkSubsystem>(); Town && Town->LocalAttack(this)) { return; }
    if (PilotHealth > 0) { ServerPilotAttack(nullptr); }
}

void ASFE1Character::ServerPilotAttack_Implementation(AActor* SelectedCombatTarget)
{
    if (auto* Mode = Cast<ASFGameMode>(GetWorld()->GetAuthGameMode()))
    { Mode->TryPlayerAttack(this, SelectedCombatTarget); }
}

void ASFE1Character::SetAutoAttack(bool bEnabled)
{
    const FString Map = GetWorld()->GetOutermost()->GetName();
    if (Map != TEXT("/Game/Frontier/Maps/Sandbox/SB_AutoAttack") &&
        Map != TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook")) { return; }
    if (HasAuthority()) { ServerSetAutoAttack_Implementation(bEnabled); }
    else { ServerSetAutoAttack(bEnabled); }
}

void ASFE1Character::TickSpiritReleaseScenario()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    FString ScenarioRole;
    FParse::Value(FCommandLine::Get(), TEXT("SpiritReleaseRole="), ScenarioRole);
    if (ScenarioRole != TEXT("Driver") && ScenarioRole != TEXT("Observer")) return;
    ASFPlayerController* PC = Cast<ASFPlayerController>(Controller);
    if (!PC) return;
    ASFE1Character* Subject = this;
    if (ScenarioRole == TEXT("Observer"))
    {
        Subject = nullptr;
        for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It)
        { if (*It != this) { Subject = *It; break; } }
    }
    if (!Subject) return;
    ASFPilotBoar* Boar = nullptr;
    for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It)
    { if (It->GroupId() == TEXT("spirit_release_sandbox")) { Boar = *It; break; } }
    if (!Boar) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (SpiritReleaseScenarioStage == 0)
    {
        int32 Pawns = 0;
        for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It) { ++Pawns; }
        if (Pawns < 2 || Subject->CombatHealth() <= 0) return;
        UE_LOG(LogTemp, Display, TEXT("SPIRIT_RELEASE_CLIENT_INITIAL Role=%s HP=%d X=%.0f"),
            *ScenarioRole, Subject->CombatHealth(), Subject->GetActorLocation().X);
        if (ScenarioRole == TEXT("Driver")) { PC->SFReleaseSpirit(); }
        SpiritReleaseScenarioStage = 1;
    }
    if (ScenarioRole == TEXT("Driver") && SpiritReleaseScenarioStage == 1)
    {
        const FVector Toward = (Boar->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
        if (FVector::Dist2D(GetActorLocation(), Boar->GetActorLocation()) > 220.0)
        { AddMovementInput(Toward, 1.0f); return; }
        Controller->SetControlRotation(Toward.Rotation());
        if (SpiritReleaseScenarioAt == 0.0) { SpiritReleaseScenarioAt = Now + 0.5; return; }
        if (Now < SpiritReleaseScenarioAt) return;
        PilotAttack();
        UE_LOG(LogTemp, Display, TEXT("SPIRIT_RELEASE_CLIENT_ATTACK Role=Driver"));
        SpiritReleaseScenarioStage = 2;
        SpiritReleaseScenarioAt = Now + 1.0;
    }
    if (ScenarioRole == TEXT("Observer") && SpiritReleaseScenarioStage == 1)
    { SpiritReleaseScenarioStage = 2; }
    if (ScenarioRole == TEXT("Driver") && SpiritReleaseScenarioStage == 2 &&
        Boar->Health() == GetDefault<USFCombatPilotSettings>()->BoarHealth && Now >= SpiritReleaseScenarioAt)
    {
        PilotAttack();
        SpiritReleaseScenarioAt = Now + 1.0;
    }
    if (ScenarioRole == TEXT("Driver") && SpiritReleaseScenarioStage == 2 && Now >= NextSpiritReleaseSampleAt)
    {
        NextSpiritReleaseSampleAt = Now + 2.0;
        UE_LOG(LogTemp, Display, TEXT("SPIRIT_RELEASE_CLIENT_SAMPLE HP=%d X=%.0f Y=%.0f BoarX=%.0f BoarY=%.0f BoarHP=%d"),
            Subject->CombatHealth(), Subject->GetActorLocation().X, Subject->GetActorLocation().Y,
            Boar->GetActorLocation().X, Boar->GetActorLocation().Y, Boar->Health());
    }
    if (SpiritReleaseScenarioStage == 2 && Subject->CombatHealth() == 0)
    {
        SpiritReleaseScenarioStage = 3;
        SpiritReleaseScenarioAt = Now;
        UE_LOG(LogTemp, Display, TEXT("SPIRIT_RELEASE_CLIENT_DEAD Role=%s HP=0 X=%.0f"),
            *ScenarioRole, Subject->GetActorLocation().X);
    }
    if (SpiritReleaseScenarioStage == 3 && ScenarioRole == TEXT("Driver") && Now - SpiritReleaseScenarioAt >= 5.0)
    {
        if (Subject->CombatHealth() != 0)
        { UE_LOG(LogTemp, Error, TEXT("SPIRIT_RELEASE_CLIENT_FAILED PrematureHealth=%d"), Subject->CombatHealth()); return; }
        PC->SFReleaseSpirit();
        UE_LOG(LogTemp, Display, TEXT("SPIRIT_RELEASE_CLIENT_REQUEST Role=Driver"));
        SpiritReleaseScenarioStage = 4;
    }
    if (SpiritReleaseScenarioStage == 3 && ScenarioRole == TEXT("Observer"))
    { SpiritReleaseScenarioStage = 4; }
    if (SpiritReleaseScenarioStage == 4 && Subject->CombatHealth() == 40 &&
        FMath::Abs(Subject->GetActorLocation().X - 3500.0) < 100.0)
    {
        SpiritReleaseScenarioStage = 5;
        UE_LOG(LogTemp, Display, TEXT("SPIRIT_RELEASE_CLIENT_RESTORED Role=%s HP=%d X=%.0f"),
            *ScenarioRole, Subject->CombatHealth(), Subject->GetActorLocation().X);
        if (ScenarioRole == TEXT("Driver")) { PC->SFReleaseSpirit(); }
    }
#endif
}

void ASFE1Character::TickResurrectionAcceptScenario()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    FString ScenarioRole;
    FParse::Value(FCommandLine::Get(), TEXT("ResurrectionAcceptRole="), ScenarioRole);
    if (ScenarioRole != TEXT("Driver") && ScenarioRole != TEXT("Observer")) return;
    ASFPlayerController* PC = Cast<ASFPlayerController>(Controller);
    if (!PC) return;
    ASFE1Character* Subject = this;
    if (ScenarioRole == TEXT("Observer"))
    {
        Subject = nullptr;
        for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It)
        { if (*It != this) { Subject = *It; break; } }
    }
    if (!Subject) return;
    ASFPilotBoar* Boar = nullptr;
    for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It)
    { if (It->GroupId() == TEXT("resurrection_accept_sandbox")) { Boar = *It; break; } }
    if (!Boar) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (ResurrectionAcceptScenarioStage == 0)
    {
        int32 Pawns = 0;
        for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It) { ++Pawns; }
        if (Pawns < 2 || Subject->CombatHealth() != 35) return;
        UE_LOG(LogTemp, Display, TEXT("RES_ACCEPT_CLIENT_INITIAL Role=%s HP=%d"), *ScenarioRole, Subject->CombatHealth());
        if (ScenarioRole == TEXT("Driver")) { PC->SFAcceptResurrection(); } // Living request must fail.
        ResurrectionAcceptScenarioStage = 1;
    }
    if (ScenarioRole == TEXT("Driver") && ResurrectionAcceptScenarioStage == 1)
    {
        const FVector Toward = (Boar->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
        if (FVector::Dist2D(GetActorLocation(), Boar->GetActorLocation()) > 220.0)
        { AddMovementInput(Toward, 1.0f); return; }
        Controller->SetControlRotation(Toward.Rotation());
        if (ResurrectionAcceptScenarioAt == 0.0) { ResurrectionAcceptScenarioAt = Now + 0.5; return; }
        if (Now < ResurrectionAcceptScenarioAt) return;
        PilotAttack();
        ResurrectionAcceptScenarioStage = 2;
        ResurrectionAcceptScenarioAt = Now + 1.0;
        UE_LOG(LogTemp, Display, TEXT("RES_ACCEPT_CLIENT_ATTACK Role=Driver"));
    }
    if (ScenarioRole == TEXT("Observer") && ResurrectionAcceptScenarioStage == 1)
    { ResurrectionAcceptScenarioStage = 2; }
    if (ScenarioRole == TEXT("Driver") && ResurrectionAcceptScenarioStage == 2 &&
        Boar->Health() == GetDefault<USFCombatPilotSettings>()->BoarHealth && Now >= ResurrectionAcceptScenarioAt)
    { PilotAttack(); ResurrectionAcceptScenarioAt = Now + 1.0; }
    if (ResurrectionAcceptScenarioStage == 2 && Subject->CombatHealth() == 0)
    {
        ResurrectionAcceptScenarioStage = 3;
        ResurrectionAcceptScenarioAt = Now;
        UE_LOG(LogTemp, Display, TEXT("RES_ACCEPT_CLIENT_DEAD Role=%s HP=0"), *ScenarioRole);
    }
    if (ScenarioRole == TEXT("Driver") && ResurrectionAcceptScenarioStage == 3 &&
        GetResurrectionOfferId() != 0 && Now - ResurrectionAcceptScenarioAt >= 5.0)
    {
        UE_LOG(LogTemp, Display, TEXT("RES_ACCEPT_CLIENT_OFFER Role=Driver Offer=%u"), GetResurrectionOfferId());
        PC->SFAcceptResurrection();
        ResurrectionAcceptScenarioStage = 4;
    }
    if (ScenarioRole == TEXT("Observer") && ResurrectionAcceptScenarioStage == 3)
    { ResurrectionAcceptScenarioStage = 4; }
    if (ResurrectionAcceptScenarioStage == 4 && Subject->CombatHealth() == 28)
    {
        ResurrectionAcceptScenarioStage = 5;
        UE_LOG(LogTemp, Display, TEXT("RES_ACCEPT_CLIENT_RESTORED Role=%s HP=28"), *ScenarioRole);
        if (ScenarioRole == TEXT("Driver")) { PC->ServerAcceptResurrection(1, FString()); } // Replayed offer must fail.
    }
#endif
}

void ASFE1Character::TickUnstuckScenario()
{
#if WITH_DEV_AUTOMATION_TESTS
    FString ScenarioRole;
    FParse::Value(FCommandLine::Get(), TEXT("UnstuckRole="), ScenarioRole);
    if (ScenarioRole != TEXT("Driver") && ScenarioRole != TEXT("Observer")) return;
    ASFPlayerController* PC = Cast<ASFPlayerController>(Controller);
    if (!PC) return;
    const double Now = GetWorld()->GetTimeSeconds();
    ASFE1Character* Remote = nullptr;
    for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It)
    { if (*It != this) { Remote = *It; break; } }
    if (!Remote) return;
    const FVector Observed = ScenarioRole == TEXT("Driver") ? GetActorLocation() : Remote->GetActorLocation();
    if (UnstuckScenarioStage == 0)
    {
        UnstuckScenarioInitial = Observed;
        UnstuckScenarioAt = Now + 2.0;
        UnstuckScenarioStage = 1;
        UE_LOG(LogTemp, Display, TEXT("UNSTUCK_CLIENT_READY Role=%s"), *ScenarioRole);
    }
    if (ScenarioRole == TEXT("Driver") && UnstuckScenarioStage == 1 && Now >= UnstuckScenarioAt)
    {
        PC->SFUnstuck();
        UnstuckScenarioStage = 2;
        UE_LOG(LogTemp, Display, TEXT("UNSTUCK_CLIENT_REQUEST Role=Driver"));
    }
    if (UnstuckScenarioStage >= 1 && UnstuckScenarioStage < 3 &&
        FVector::Dist2D(UnstuckScenarioInitial, Observed) >= 200.0f)
    {
        UE_LOG(LogTemp, Display, TEXT("UNSTUCK_CLIENT_MOVED Role=%s Distance=%.0f"),
            *ScenarioRole, FVector::Dist2D(UnstuckScenarioInitial, Observed));
        if (ScenarioRole == TEXT("Driver")) { PC->SFUnstuck(); }
        UnstuckScenarioStage = 3;
    }
#endif
}

void ASFE1Character::TickAutoAttackScenario()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    FString ScenarioRole;
    FParse::Value(FCommandLine::Get(), TEXT("AutoAttackRole="), ScenarioRole);
    if (ScenarioRole != TEXT("Driver") && ScenarioRole != TEXT("Observer")) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    ASFPilotBoar* Boar = nullptr;
    for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It) { Boar = *It; break; }
    if (!Boar || !Controller) { return; }
    if (Now >= NextAutoAttackSampleAt)
    {
        NextAutoAttackSampleAt = Now + 0.2;
        UE_LOG(LogTemp, Display, TEXT("AUTO_ATTACK_CLIENT_SAMPLE Role=%s T=%.2f HP=%d Stage=%d Enabled=%d"),
            *ScenarioRole, Now, Boar->Health(), AutoAttackScenarioStage, bAutoAttacking ? 1 : 0);
    }
    if (ScenarioRole == TEXT("Observer")) { return; }
    ASFPlayerController* PC = Cast<ASFPlayerController>(Controller);
    if (!PC) { return; }
    if (AutoAttackScenarioStage == 0)
    {
        int32 PresentPlayers = 0;
        for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It) { ++PresentPlayers; }
        if (PresentPlayers < 2) { return; }
        if (AutoAttackScenarioAt == 0.0) { AutoAttackScenarioAt = Now + 2.0; }
        if (Now < AutoAttackScenarioAt) { return; }
        Controller->SetControlRotation((Boar->GetActorLocation() - GetActorLocation()).Rotation());
        PC->SelectNextCombatTarget();
        AutoAttackScenarioAt = Now;
        AutoAttackScenarioStage = 1;
    }
    else if (AutoAttackScenarioStage == 1)
    {
        if (Now - AutoAttackScenarioAt < 0.3) { return; }
        PC->SelectNextCombatTarget();
        AutoAttackScenarioAt = Now;
        AutoAttackScenarioStage = 2;
    }
    else if (AutoAttackScenarioStage == 2 && CombatTarget == Boar)
    {
        SetAutoAttack(true);
        AutoAttackScenarioStage = 3;
        UE_LOG(LogTemp, Display, TEXT("AUTO_ATTACK_CLIENT_START Role=Driver"));
    }
    else if (AutoAttackScenarioStage == 2 && Now - AutoAttackScenarioAt >= 1.0)
    { PC->SelectNextCombatTarget(); AutoAttackScenarioAt = Now; }
    else if (AutoAttackScenarioStage == 3 && Boar->Health() <= 40)
    {
        SetAutoAttack(false);
        AutoAttackScenarioAt = Now;
        AutoAttackScenarioStage = 4;
        UE_LOG(LogTemp, Display, TEXT("AUTO_ATTACK_CLIENT_STOP Role=Driver HP=%d"), Boar->Health());
    }
    else if (AutoAttackScenarioStage == 4 && Now - AutoAttackScenarioAt >= 2.5)
    {
        if (Boar->Health() != 40)
        { UE_LOG(LogTemp, Error, TEXT("AUTO_ATTACK_CLIENT_FAILED StopHealth=%d"), Boar->Health()); AutoAttackScenarioStage = 6; return; }
        SetAutoAttack(true);
        AutoAttackScenarioStage = 5;
        UE_LOG(LogTemp, Display, TEXT("AUTO_ATTACK_CLIENT_RESUME Role=Driver HP=%d"), Boar->Health());
    }
    else if (AutoAttackScenarioStage == 5 && Boar->Health() <= 20)
    {
        AutoAttackScenarioStage = 6;
        UE_LOG(LogTemp, Display, TEXT("AUTO_ATTACK_CLIENT_DONE Role=Driver HP=%d"), Boar->Health());
    }
#endif
}

void ASFE1Character::ServerSetAutoAttack_Implementation(bool bEnabled)
{
    const FString Map = GetWorld()->GetOutermost()->GetName();
    if ((Map != TEXT("/Game/Frontier/Maps/Sandbox/SB_AutoAttack") &&
         Map != TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook")) ||
        (bEnabled && (PilotHealth <= 0 || !Cast<ASFPilotBoar>(CombatTarget)))) { return; }
    bAutoAttacking = bEnabled;
    if (bEnabled) { NextAutoSwingAt = GetWorld()->GetTimeSeconds(); }
    ForceNetUpdate();
    UE_LOG(LogTemp, Display, TEXT("AUTO_ATTACK_SERVER_TOGGLE Enabled=%d T=%.2f"),
        bEnabled ? 1 : 0, GetWorld()->GetTimeSeconds());
}

void ASFE1Character::Interact()
{
    if (ASFPlayerController* PlayerController = Cast<ASFPlayerController>(Controller))
    {
        PlayerController->InteractWithSelectedTarget();
    }
}
