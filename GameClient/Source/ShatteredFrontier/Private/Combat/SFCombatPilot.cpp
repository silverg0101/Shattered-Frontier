#include "Combat/SFCombatPilot.h"
#include "Characters/SFE1Character.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
TArray<FVector2D> ReadPolygon(const TArray<TSharedPtr<FJsonValue>>& Values)
{
    TArray<FVector2D> Result;
    for (const auto& Value : Values)
    {
        const auto& Pair = Value->AsArray();
        if (Pair.Num() >= 2) { Result.Emplace(Pair[0]->AsNumber(), Pair[1]->AsNumber()); }
    }
    return Result;
}

FVector ZonePoint(const TArray<TSharedPtr<FJsonValue>>& Values)
{
    return FVector(Values[0]->AsNumber() * 100.0, Values[1]->AsNumber() * 100.0,
        Values.Num() > 2 ? Values[2]->AsNumber() * 100.0 : 0.0);
}

FVector Grounded(UWorld* World, const FVector& XY, float HalfHeight)
{
    FHitResult Hit;
    const FVector Start(XY.X, XY.Y, 20000);
    const FVector End(XY.X, XY.Y, -20000);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CombatPilotGround), false);
    if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
    { return FVector(XY.X, XY.Y, Hit.ImpactPoint.Z + HalfHeight + 4); }
    return FVector(XY.X, XY.Y, XY.Z + HalfHeight + 4);
}
}

bool SFCombatPilotRules::CanMelee(double DistanceCm, double FacingDot, double Now,
    double NextAllowed, double RangeCm)
{
    return FMath::IsFinite(DistanceCm) && FMath::IsFinite(FacingDot) &&
        DistanceCm <= RangeCm && FacingDot >= 0.35 && Now >= NextAllowed;
}

int32 SFCombatPilotRules::RemainingHealth(int32 Current, int32 Damage)
{
    return Damage > 0 ? FMath::Max(0, Current - Damage) : Current;
}

bool USFCombatPilotSettings::Validate(FString& Error) const
{
    if (PlayerHealth < 20 || PlayerHealth > 500 || BoarHealth < 10 || BoarHealth > 500 ||
        PlayerMeleeDamage < 1 || PlayerMeleeDamage > 100 || BoarMeleeDamage < 1 || BoarMeleeDamage > 100 ||
        !FMath::IsWithinInclusive(PlayerMeleeCooldownSeconds, 0.3f, 5.0f) ||
        !FMath::IsWithinInclusive(BoarMeleeCooldownSeconds, 0.5f, 5.0f) ||
        !FMath::IsWithinInclusive(MeleeRangeMetres, 1.0f, 5.0f) ||
        !FMath::IsWithinInclusive(PlayerRespawnDelaySeconds, 1.0f, 15.0f) ||
        !FMath::IsWithinInclusive(BoarAggroRadiusMetres, 4.0f, 18.0f) ||
        !FMath::IsWithinInclusive(BoarNearNoticeRadiusMetres, 1.0f, 10.0f) ||
        BoarNearNoticeRadiusMetres > BoarAggroRadiusMetres ||
        !FMath::IsWithinInclusive(BoarLeashRadiusMetres, 20.0f, 100.0f) ||
        !FMath::IsWithinInclusive(BoarFieldOfViewDegrees, 60.0f, 360.0f) ||
        !FMath::IsWithinInclusive(BoarWanderRadiusMetres, 2.0f, 40.0f) ||
        !FMath::IsWithinInclusive(BoarWanderSpeedMetresPerSecond, 0.5f, 5.0f) ||
        !FMath::IsWithinInclusive(BoarChaseSpeedMetresPerSecond, 1.0f, 10.0f) ||
        !FMath::IsWithinInclusive(BoarWanderMinSeconds, 1.0f, 10.0f) ||
        !FMath::IsWithinInclusive(BoarWanderMaxSeconds, BoarWanderMinSeconds, 20.0f))
    { Error = TEXT("Combat pilot balance is outside declared bounds"); return false; }
    return true;
}

ASFPilotBoar::ASFPilotBoar()
{
    bReplicates = true;
    SetReplicateMovement(true);
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(55, 65);
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoarBody"));
    Body->SetupAttachment(GetCapsuleComponent());
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->SetRelativeScale3D(FVector(1.3, 0.7, 0.7));
    if (!IsRunningDedicatedServer())
    {
        static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
        Body->SetStaticMesh(Cube.Object);
    }
    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("HealthLabel"));
    Label->SetupAttachment(GetCapsuleComponent());
    Label->SetRelativeLocation(FVector(0, 0, 130));
    Label->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    Label->SetWorldSize(34);
    SetNetCullDistanceSquared(FMath::Square(80000.0f));
}

void ASFPilotBoar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFPilotBoar, CurrentHealth);
}

void ASFPilotBoar::Initialize(ASFCombatPilotGameMode* OwnerMode, const FString& Group, int32 InSlot,
    const TArray<FVector2D>& Area, int32 Health)
{
    check(HasAuthority());
    PilotMode = OwnerMode; SpawnGroup = Group; Slot = InSlot; Territory = Area;
    Anchor = GetActorLocation(); CurrentHealth = Health; OnRep_Health();
}

void ASFPilotBoar::OnRep_Health()
{
    Label->SetText(FText::FromString(FString::Printf(TEXT("Boar L2  %d HP"), CurrentHealth)));
    Label->SetTextRenderColor(CurrentHealth > 0 ? FColor::Yellow : FColor::Red);
}

void ASFPilotBoar::ReceivePilotDamage(ASFE1Character* Attacker, int32 Damage)
{
    if (!HasAuthority() || CurrentHealth <= 0 || !IsValid(Attacker) || Damage <= 0) { return; }
    Target = Attacker;
    bReturning = false;
    CurrentHealth = SFCombatPilotRules::RemainingHealth(CurrentHealth, Damage);
    OnRep_Health(); ForceNetUpdate();
    UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_BOAR_HIT Group=%s Slot=%d HP=%d"), *SpawnGroup, Slot, CurrentHealth);
    if (CurrentHealth == 0 && PilotMode.IsValid()) { PilotMode->BoarDied(this); }
}

void ASFPilotBoar::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || CurrentHealth <= 0 || !PilotMode.IsValid() || !PilotMode->IsActive()) { return; }
    auto* Mode = PilotMode.Get();
    const double Now = GetWorld()->GetTimeSeconds();
    ASFE1Character* Victim = Target.Get();
    if (Victim && (Victim->CombatHealth() <= 0 || Mode->IsSafe(Victim->GetActorLocation()) ||
        !Mode->IsInsideTerritory(Victim->GetActorLocation(), Territory) ||
        FVector::Dist2D(Anchor, GetActorLocation()) > Mode->Settings()->BoarLeashRadiusMetres*100))
    { Target.Reset(); Victim = nullptr; WanderDestination = Anchor; bReturning = true; }
    if (bReturning && FVector::Dist2D(GetActorLocation(),Anchor) < 100)
    { bReturning = false; CurrentHealth = Mode->Settings()->BoarHealth; OnRep_Health(); ForceNetUpdate(); }
    if (!Victim && !bReturning && Mode->Settings()->bCombatAvailable)
    {
        for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            auto* Candidate = Cast<ASFE1Character>(It->Get()->GetPawn());
            const FVector Toward = Candidate ? Candidate->GetActorLocation()-GetActorLocation() : FVector::ZeroVector;
            const double Distance = Toward.Size2D();
            if (IsValid(Candidate) && Candidate->CombatHealth() > 0 &&
                !Mode->IsSafe(Candidate->GetActorLocation()) &&
                Mode->IsInsideTerritory(Candidate->GetActorLocation(), Territory) &&
                Distance <= Mode->Settings()->BoarAggroRadiusMetres*100 &&
                (Distance <= Mode->Settings()->BoarNearNoticeRadiusMetres*100 ||
                    FVector::DotProduct(GetActorForwardVector(), Toward.GetSafeNormal2D()) >=
                    FMath::Cos(FMath::DegreesToRadians(Mode->Settings()->BoarFieldOfViewDegrees*0.5f))))
            { Target = Candidate; Victim = Candidate; break; }
        }
    }
    FVector Destination = WanderDestination;
    float Speed = Mode->Settings()->BoarWanderSpeedMetresPerSecond*100;
    if (Victim)
    {
        Destination = Victim->GetActorLocation(); Speed = Mode->Settings()->BoarChaseSpeedMetresPerSecond*100;
        if (FVector::Dist2D(GetActorLocation(), Destination) <= Mode->Settings()->MeleeRangeMetres * 100 && Now >= NextSwingAt)
        { NextSwingAt = Now + Mode->Settings()->BoarMeleeCooldownSeconds; Mode->TryBoarAttack(this, Victim); }
    }
    else if (bReturning)
    { Destination = Anchor; Speed = Mode->Settings()->BoarChaseSpeedMetresPerSecond*100; }
    else if (Now >= NextWanderAt || Destination.IsNearlyZero() || FVector::Dist2D(GetActorLocation(), Destination) < 80)
    {
        NextWanderAt = Now + FMath::FRandRange(double(Mode->Settings()->BoarWanderMinSeconds),
            double(Mode->Settings()->BoarWanderMaxSeconds));
        for (int32 Attempt = 0; Attempt < 12; ++Attempt)
        {
            const double Radius = Mode->Settings()->BoarWanderRadiusMetres*100;
            const FVector Candidate = Anchor + FVector(FMath::FRandRange(-Radius, Radius), FMath::FRandRange(-Radius, Radius), 0);
            if (Mode->IsInsideTerritory(Candidate, Territory) && !Mode->IsSafe(Candidate))
            { WanderDestination = Candidate; break; }
        }
        Destination = WanderDestination;
    }
    FVector Direction = Destination - GetActorLocation(); Direction.Z = 0;
    if (Direction.SizeSquared2D() < 2500) { return; }
    const FVector Candidate = GetActorLocation() + Direction.GetSafeNormal2D() * FMath::Min(Speed * DeltaSeconds, Direction.Size2D());
    if (!Mode->IsInsideTerritory(Candidate, Territory) || Mode->IsSafe(Candidate)) { Target.Reset(); return; }
    const FVector Ground = Grounded(GetWorld(), Candidate, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    SetActorLocation(Ground, true);
    SetActorRotation(Direction.Rotation());
}

bool ASFCombatPilotGameMode::IsInsideTerritory(const FVector& Location, const TArray<FVector2D>& Polygon) const
{
    if (Polygon.Num() < 3) { return false; }
    const FVector2D P(Location.X / 100.0, Location.Y / 100.0);
    bool Inside = false;
    for (int32 I = 0, J = Polygon.Num()-1; I < Polygon.Num(); J = I++)
    {
        const FVector2D& A = Polygon[I]; const FVector2D& B = Polygon[J];
        const double Cross = (P.X-A.X)*(B.Y-A.Y) - (P.Y-A.Y)*(B.X-A.X);
        if (FMath::Abs(Cross) < 1e-7 &&
            P.X >= FMath::Min(A.X,B.X)-1e-7 && P.X <= FMath::Max(A.X,B.X)+1e-7 &&
            P.Y >= FMath::Min(A.Y,B.Y)-1e-7 && P.Y <= FMath::Max(A.Y,B.Y)+1e-7) return true;
        if ((A.Y > P.Y) != (B.Y > P.Y) && P.X < (B.X-A.X) * (P.Y-A.Y) / (B.Y-A.Y) + A.X) { Inside = !Inside; }
    }
    return Inside;
}

bool ASFCombatPilotGameMode::IsSafe(const FVector& Location) const
{
    for (const auto& Polygon : ProtectedAreas)
    { if (IsInsideTerritory(Location, Polygon)) { return true; } }
    return false;
}

bool ASFCombatPilotGameMode::LoadZoneRecord()
{
    const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../content/world/zones/greenlands_silverbrook.json"));
    FString Source; TSharedPtr<FJsonObject> Zone;
    if (!FFileHelper::LoadFileToString(Source, *Path) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source), Zone) || !Zone.IsValid() ||
        Zone->GetStringField(TEXT("map_id")) != TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook"))
    { return false; }
    for (const auto& Value : Zone->GetArrayField(TEXT("spawn_groups")))
    {
        const auto Group = Value->AsObject();
        const FString Id = Group->GetStringField(TEXT("group_id"));
        if (Id == TEXT("boar_farm_edge") || Id == TEXT("boar_forest_edge"))
        {
            if (Group->GetStringField(TEXT("spawn_kind")) != TEXT("npc") ||
                Group->GetArrayField(TEXT("definitions"))[0]->AsObject()->GetStringField(TEXT("definition_id")) != TEXT("npc/boar")) return false;
            Groups.Add(Id, { ReadPolygon(Group->GetArrayField(TEXT("area"))) });
        }
    }
    for (const auto& Value : Zone->GetArrayField(TEXT("areas")))
    {
        const auto Area = Value->AsObject();
        if (Area->GetStringField(TEXT("safety")) == TEXT("protected"))
        { ProtectedAreas.Add(ReadPolygon(Area->GetArrayField(TEXT("polygon")))); }
    }
    TSet<FString> LevelTwoAreas;
    for (const auto& Value : Zone->GetArrayField(TEXT("level_areas")))
    {
        const auto Area = Value->AsObject();
        if (Area->GetIntegerField(TEXT("content_level")) == 2)
        { LevelTwoAreas.Add(Area->GetStringField(TEXT("area_id"))); }
    }
    for (const auto& Value : Zone->GetArrayField(TEXT("respawn_points")))
    { RespawnPoints.Add(ZonePoint(Value->AsObject()->GetArrayField(TEXT("position")))); }
    return Groups.Num() == 2 && LevelTwoAreas.Contains(TEXT("boar_farm_edge")) &&
        LevelTwoAreas.Contains(TEXT("boar_forest_edge")) && ProtectedAreas.Num() > 0 && RespawnPoints.Num() > 0;
}

ASFCombatPilotGameMode::ASFCombatPilotGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 1.0f;
}

void ASFCombatPilotGameMode::StartPlay()
{
    Super::StartPlay();
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    bPilotActive = GetNetMode() == NM_DedicatedServer &&
        GetWorld()->GetOutermost()->GetName() == TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook") &&
        FParse::Param(FCommandLine::Get(), TEXT("CombatPilot"));
    bScenario = bPilotActive && FParse::Param(FCommandLine::Get(), TEXT("CombatPilotScenario"));
#endif
    if (!bPilotActive) { return; }
    CapturedSettings = GetDefault<USFCombatPilotSettings>();
    FString Error;
    if (!CapturedSettings->Validate(Error) || !LoadZoneRecord())
    { UE_LOG(LogTemp, Error, TEXT("COMBAT_PILOT_DISABLED Invalid settings or zone record: %s"), *Error); bPilotActive = false; return; }
    SlotStatePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CombatPilot/boar-slots.json"));
    IFileManager::Get().Delete(*SlotStatePath);
    bSlotBridgeHealthy = RunWorldSlots(TEXT("tick"));
    if (!bSlotBridgeHealthy) { bPilotActive = false; UE_LOG(LogTemp, Error, TEXT("COMBAT_PILOT_DISABLED WorldContent bridge failed")); return; }
    UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_READY Boars=%d Combat=%d HostileSpawning=%d"), ActiveBoars.Num(), CapturedSettings->bCombatAvailable, CapturedSettings->bSilverbrookHostileSpawning);
}

void ASFCombatPilotGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    if (!bPilotActive || !bScenario || GetNumPlayers() != 1) { return; }
    const TWeakObjectPtr<APlayerController> Weak(NewPlayer);
    FTimerHandle Timer;
    GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, Weak]()
    {
        if (!Weak.IsValid()) { return; }
        auto* Player = Cast<ASFE1Character>(Weak->GetPawn());
        auto* Boar = ActiveBoars.FindRef(TEXT("boar_farm_edge:0")).Get();
        if (!Player || !Boar) { return; }
        const FVector Near = Grounded(GetWorld(), Boar->GetActorLocation()+FVector(180,0,0), Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        Player->SetActorLocation(Near, false, nullptr, ETeleportType::TeleportPhysics);
        UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_SCENARIO_START Pawn=%s"), *Player->GetName());
    }), 2.0f, false);
}

void ASFCombatPilotGameMode::RestartPlayer(AController* NewPlayer)
{
    Super::RestartPlayer(NewPlayer);
    if (bPilotActive)
    { if (auto* Player = Cast<ASFE1Character>(NewPlayer ? NewPlayer->GetPawn() : nullptr)) Player->CombatSetHealth(CapturedSettings->PlayerHealth); }
}

void ASFCombatPilotGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bPilotActive || !CapturedSettings->bSilverbrookHostileSpawning || !bSlotBridgeHealthy) { return; }
    if (GetWorld()->GetTimeSeconds() >= NextSlotTick)
    { NextSlotTick = GetWorld()->GetTimeSeconds() + 2.0; bSlotBridgeHealthy = RunWorldSlots(TEXT("tick")); }
}

bool ASFCombatPilotGameMode::RunWorldSlots(const FString& Command, const FString& Group, int32 Slot)
{
    const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), TEXT(".."));
    const FString Dotnet = FPaths::Combine(Root, TEXT("Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe"));
    const FString Dll = FPaths::Combine(Root, TEXT("BackendServices/WorldContent.Tool/bin/Debug/net10.0/WorldContent.Tool.dll"));
    const FString Zone = FPaths::Combine(Root, TEXT("content/world/zones/greenlands_silverbrook.json"));
    if (!FPaths::FileExists(Dotnet) || !FPaths::FileExists(Dll)) { return false; }
    FString Args = FString::Printf(TEXT("\"%s\" combat-pilot \"%s\" \"%s\" %s"), *Dll, *Zone, *SlotStatePath, *Command);
    if (Command == TEXT("defeat"))
    { Args += FString::Printf(TEXT(" %s %d %d"), *Group, Slot, GetNumPlayers()); }
    Args += CapturedSettings->bSilverbrookHostileSpawning ? TEXT(" true") : TEXT(" false");
    int32 Code = -1; FString Stdout, Stderr;
    FPlatformProcess::ExecProcess(*Dotnet, *Args, &Code, &Stdout, &Stderr);
    if (Code != 0) { UE_LOG(LogTemp, Error, TEXT("COMBAT_PILOT_SLOT_ERROR Code=%d %s"), Code, *Stderr); return false; }
    TArray<FString> Lines; Stdout.ParseIntoArrayLines(Lines);
    for (const FString& Line : Lines)
    {
        TArray<FString> Fields; Line.ParseIntoArray(Fields, TEXT("|"), false);
        if (Fields.Num() != 9 || Fields[0] != TEXT("SLOT") || !Groups.Contains(Fields[1])) { continue; }
        if (Fields[4] == TEXT("Active") && CapturedSettings->bSilverbrookHostileSpawning)
        { SpawnSlot(Fields[1], FCString::Atoi(*Fields[2]), FCString::Atoi(*Fields[3]), Fields[5],
            FCString::Atod(*Fields[6]), FCString::Atod(*Fields[7]), FCString::Atod(*Fields[8])); }
    }
    return true;
}

void ASFCombatPilotGameMode::SpawnSlot(const FString& Group, int32 Slot, int32 Cycle, const FString& PointId,
    double X, double Y, double Facing)
{
    const FString Key = FString::Printf(TEXT("%s:%d"), *Group, Slot);
    if (ActiveBoars.FindRef(Key).IsValid() || PointId == TEXT("-")) { return; }
    const FVector Where = Grounded(GetWorld(), FVector(X*100,Y*100,0), 65);
    if (IsSafe(Where) || !IsInsideTerritory(Where, Groups[Group].Polygon)) { return; }
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    auto* Boar = GetWorld()->SpawnActor<ASFPilotBoar>(Where, FRotator(0,Facing,0), Params);
    if (!Boar) { return; }
    Boar->Initialize(this, Group, Slot, Groups[Group].Polygon, CapturedSettings->BoarHealth);
    ActiveBoars.Add(Key, Boar);
    UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_BOAR_SPAWN Group=%s Slot=%d Cycle=%d Point=%s"), *Group, Slot, Cycle, *PointId);
}

void ASFCombatPilotGameMode::TryPlayerAttack(ASFE1Character* Attacker)
{
    if (!bPilotActive || !CapturedSettings->bCombatAvailable || !IsValid(Attacker) || Attacker->CombatHealth() <= 0 || IsSafe(Attacker->GetActorLocation())) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now < PlayerNextSwing.FindRef(Attacker)) { return; }
    ASFPilotBoar* Best = nullptr; double BestDistance = CapturedSettings->MeleeRangeMetres * 100;
    const FVector Facing = Attacker->GetControlRotation().Vector().GetSafeNormal2D();
    for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It)
    {
        if (It->Health() <= 0 || IsSafe(It->GetActorLocation())) { continue; }
        const FVector Delta = It->GetActorLocation() - Attacker->GetActorLocation();
        const double Distance = Delta.Size2D();
        if (!SFCombatPilotRules::CanMelee(Distance, FVector::DotProduct(Facing,Delta.GetSafeNormal2D()),
            Now, PlayerNextSwing.FindRef(Attacker), BestDistance)) { continue; }
        FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(CombatPilotMelee), false, Attacker);
        if (GetWorld()->LineTraceSingleByChannel(Hit, Attacker->GetActorLocation()+FVector(0,0,45),
            It->GetActorLocation()+FVector(0,0,45), ECC_Visibility, Query) && Hit.GetActor() != *It) { continue; }
        Best = *It; BestDistance = Distance;
    }
    if (!Best) { return; }
    PlayerNextSwing.Add(Attacker, Now + CapturedSettings->PlayerMeleeCooldownSeconds);
    Best->ReceivePilotDamage(Attacker, CapturedSettings->PlayerMeleeDamage);
}

void ASFCombatPilotGameMode::TryBoarAttack(ASFPilotBoar* Boar, ASFE1Character* Victim)
{
    if (!bPilotActive || !CapturedSettings->bCombatAvailable || !IsValid(Boar) || !IsValid(Victim) ||
        Boar->Health() <= 0 || Victim->CombatHealth() <= 0 || IsSafe(Boar->GetActorLocation()) ||
        IsSafe(Victim->GetActorLocation()) ||
        FVector::Dist2D(Boar->GetActorLocation(), Victim->GetActorLocation()) > CapturedSettings->MeleeRangeMetres*100) return;
    Victim->CombatSetHealth(SFCombatPilotRules::RemainingHealth(Victim->CombatHealth(), CapturedSettings->BoarMeleeDamage));
    UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_PLAYER_HIT HP=%d"), Victim->CombatHealth());
    if (Victim->CombatHealth() == 0)
    {
        Victim->GetCharacterMovement()->DisableMovement();
        UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_PLAYER_DEAD Pawn=%s"), *Victim->GetName());
        FTimerHandle Timer;
        GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this,
            [this, Weak = TWeakObjectPtr<ASFE1Character>(Victim)]() { RespawnPlayer(Weak); }),
            CapturedSettings->PlayerRespawnDelaySeconds, false);
    }
}

void ASFCombatPilotGameMode::RespawnPlayer(TWeakObjectPtr<ASFE1Character> Player)
{
    if (!bPilotActive || !Player.IsValid() || Player->CombatHealth() != 0 || RespawnPoints.IsEmpty()) { return; }
    FVector Nearest = RespawnPoints[0]; double Distance = FVector::DistSquared2D(Player->GetActorLocation(), Nearest);
    for (const FVector& Point : RespawnPoints)
    { const double D = FVector::DistSquared2D(Player->GetActorLocation(),Point); if (D < Distance) { Nearest = Point; Distance = D; } }
    const FVector At = Grounded(GetWorld(), Nearest, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    Player->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
    Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    Player->CombatSetHealth(CapturedSettings->PlayerHealth);
    UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_PLAYER_RESPAWN X=%.0f Y=%.0f HP=%d"), At.X,At.Y,Player->CombatHealth());
    if (bScenario)
    {
        const FVector Town = Grounded(GetWorld(), FVector(0,-3000,0), Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* FixturePlayer = GetWorld()->SpawnActor<ASFE1Character>(Town, FRotator::ZeroRotator, Spawn);
        auto* FixtureBoar = GetWorld()->SpawnActor<ASFPilotBoar>(Town+FVector(150,0,0), FRotator::ZeroRotator, Spawn);
        if (FixturePlayer && FixtureBoar)
        {
            FixturePlayer->CombatSetHealth(CapturedSettings->PlayerHealth);
            FixtureBoar->Initialize(this, TEXT("boar_farm_edge"), -1, Groups[TEXT("boar_farm_edge")].Polygon, CapturedSettings->BoarHealth);
            const int32 Before = FixturePlayer->CombatHealth();
            TryBoarAttack(FixtureBoar, FixturePlayer);
            TryPlayerAttack(FixturePlayer);
            UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_SAFE_CHECK Protected=%d PlayerHP=%d BoarHP=%d Passed=%d"),
                IsSafe(Town), FixturePlayer->CombatHealth(), FixtureBoar->Health(),
                IsSafe(Town) && Before == FixturePlayer->CombatHealth() && FixtureBoar->Health() == CapturedSettings->BoarHealth);
        }
        if (FixtureBoar) { FixtureBoar->Destroy(); }
        if (FixturePlayer) { FixturePlayer->Destroy(); }
    }
}

void ASFCombatPilotGameMode::BoarDied(ASFPilotBoar* Boar)
{
    if (!bPilotActive || !IsValid(Boar)) { return; }
    const FString Key = FString::Printf(TEXT("%s:%d"), *Boar->GroupId(), Boar->SlotIndex());
    UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_BOAR_DEAD Group=%s Slot=%d"), *Boar->GroupId(), Boar->SlotIndex());
    bSlotBridgeHealthy = RunWorldSlots(TEXT("defeat"), Boar->GroupId(), Boar->SlotIndex());
    ActiveBoars.Remove(Key);
    Boar->Destroy();
    if (bScenario && !bScenarioFirstBoarDied)
    {
        bScenarioFirstBoarDied = true;
        FTimerHandle Timer;
        GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this]()
        {
            auto* Other = ActiveBoars.FindRef(TEXT("boar_farm_edge:1")).Get();
            auto* PC = GetWorld()->GetFirstPlayerController();
            auto* Player = Cast<ASFE1Character>(PC ? PC->GetPawn() : nullptr);
            if (!Other || !Player) { return; }
            const FVector Near = Grounded(GetWorld(), Other->GetActorLocation()+FVector(180,0,0), Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
            Player->SetActorLocation(Near, false, nullptr, ETeleportType::TeleportPhysics);
            UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_SCENARIO_BOAR_AGGRO"));
        }), 2.0f, false);
    }
}
