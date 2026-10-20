#pragma once

#include "Engine/DeveloperSettings.h"
#include "Framework/SFE1GameMode.h"
#include "GameFramework/Character.h"
#include "SFCombatPilot.generated.h"

class ASFE1Character;
class UStaticMeshComponent;
class UTextRenderComponent;

namespace SFCombatPilotRules
{
    SHATTEREDFRONTIER_API bool CanMelee(double DistanceCm, double FacingDot, double Now,
        double NextAllowed, double RangeCm);
    SHATTEREDFRONTIER_API int32 RemainingHealth(int32 Current, int32 Damage);
}

// Release-authored defaults. Both switches are adopted at the next zone start.
// The command-line flag admits this development pilot; it is not a tuning override.
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Silverbrook Combat Pilot"))
class SHATTEREDFRONTIER_API USFCombatPilotSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Combat", meta=(DisplayName="Combat available (realm)", ToolTip="Master combat availability. Suppresses new offensive actions at next zone start."))
    bool bCombatAvailable = true;
    UPROPERTY(Config, EditAnywhere, Category="Combat", meta=(DisplayName="Hostile spawning (Silverbrook)", ToolTip="Hostile NPC spawning in Silverbrook at next zone start."))
    bool bSilverbrookHostileSpawning = true;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ClampMin="20", ClampMax="500"))
    int32 PlayerHealth = 80;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ClampMin="10", ClampMax="500"))
    int32 BoarHealth = 60;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ClampMin="1", ClampMax="100"))
    int32 PlayerMeleeDamage = 20;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ClampMin="1", ClampMax="100"))
    int32 BoarMeleeDamage = 14;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ClampMin="0.3", ClampMax="5.0", Units="s"))
    float PlayerMeleeCooldownSeconds = 0.8f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ClampMin="0.5", ClampMax="5.0", Units="s"))
    float BoarMeleeCooldownSeconds = 1.6f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ClampMin="1.0", ClampMax="5.0", Units="m"))
    float MeleeRangeMetres = 2.8f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ClampMin="1.0", ClampMax="15.0", Units="s"))
    float PlayerRespawnDelaySeconds = 4.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar AI", meta=(ClampMin="4.0", ClampMax="18.0", Units="m"))
    float BoarAggroRadiusMetres = 12.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar AI", meta=(ClampMin="1.0", ClampMax="10.0", Units="m"))
    float BoarNearNoticeRadiusMetres = 4.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar AI", meta=(ClampMin="20.0", ClampMax="100.0", Units="m"))
    float BoarLeashRadiusMetres = 60.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar AI", meta=(ClampMin="60.0", ClampMax="360.0", Units="deg"))
    float BoarFieldOfViewDegrees = 160.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar AI", meta=(ClampMin="2.0", ClampMax="40.0", Units="m"))
    float BoarWanderRadiusMetres = 18.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar AI", meta=(ClampMin="0.5", ClampMax="5.0", Units="m/s"))
    float BoarWanderSpeedMetresPerSecond = 1.3f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar AI", meta=(ClampMin="1.0", ClampMax="10.0", Units="m/s"))
    float BoarChaseSpeedMetresPerSecond = 3.5f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar AI", meta=(ClampMin="1.0", ClampMax="10.0", Units="s"))
    float BoarWanderMinSeconds = 3.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar AI", meta=(ClampMin="1.0", ClampMax="20.0", Units="s"))
    float BoarWanderMaxSeconds = 6.0f;
    bool Validate(FString& Error) const;
};

UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API ASFPilotBoar : public ACharacter
{
    GENERATED_BODY()
public:
    ASFPilotBoar();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void Initialize(class ASFCombatPilotGameMode* OwnerMode, const FString& Group, int32 Slot,
        const TArray<FVector2D>& Area, int32 Health);
    void ReceivePilotDamage(ASFE1Character* Attacker, int32 Damage);
    const FString& GroupId() const { return SpawnGroup; }
    int32 SlotIndex() const { return Slot; }
    int32 Health() const { return CurrentHealth; }
private:
    UFUNCTION() void OnRep_Health();
    UPROPERTY(ReplicatedUsing=OnRep_Health) int32 CurrentHealth = 0;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
    TWeakObjectPtr<class ASFCombatPilotGameMode> PilotMode;
    TWeakObjectPtr<ASFE1Character> Target;
    FString SpawnGroup;
    int32 Slot = INDEX_NONE;
    TArray<FVector2D> Territory;
    FVector Anchor = FVector::ZeroVector;
    FVector WanderDestination = FVector::ZeroVector;
    double NextWanderAt = 0;
    double NextSwingAt = 0;
    bool bReturning = false;
};

UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API ASFCombatPilotGameMode : public ASFE1GameMode
{
    GENERATED_BODY()
public:
    ASFCombatPilotGameMode();
    virtual void StartPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void RestartPlayer(AController* NewPlayer) override;
    virtual void Tick(float DeltaSeconds) override;
    void TryPlayerAttack(ASFE1Character* Attacker);
    void TryBoarAttack(ASFPilotBoar* Boar, ASFE1Character* Victim);
    void BoarDied(ASFPilotBoar* Boar);
    bool IsSafe(const FVector& Location) const;
    bool IsInsideTerritory(const FVector& Location, const TArray<FVector2D>& Polygon) const;
    bool IsActive() const { return bPilotActive; }
    const USFCombatPilotSettings* Settings() const { return CapturedSettings; }
private:
    bool LoadZoneRecord();
    bool RunWorldSlots(const FString& Command, const FString& Group = FString(), int32 Slot = INDEX_NONE);
    void SpawnSlot(const FString& Group, int32 Slot, int32 Cycle, const FString& PointId,
        double X, double Y, double Facing);
    void RespawnPlayer(TWeakObjectPtr<ASFE1Character> Player);
    struct FBoarGroup { TArray<FVector2D> Polygon; };
    TMap<FString,FBoarGroup> Groups;
    TArray<TArray<FVector2D>> ProtectedAreas;
    TArray<FVector> RespawnPoints;
    TMap<FString,TWeakObjectPtr<ASFPilotBoar>> ActiveBoars;
    TMap<TWeakObjectPtr<ASFE1Character>,double> PlayerNextSwing;
    FString SlotStatePath;
    const USFCombatPilotSettings* CapturedSettings = nullptr;
    bool bPilotActive = false;
    bool bSlotBridgeHealthy = false;
    double NextSlotTick = 0;
    bool bScenario = false;
    bool bScenarioFirstBoarDied = false;
};
