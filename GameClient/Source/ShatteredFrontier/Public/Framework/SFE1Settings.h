#pragma once

#include "Engine/DeveloperSettings.h"
#include "SFE1Settings.generated.h"

// Release-controlled fixture settings, not config-service/GM live controls.
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="E1 Fixture (Release Only)"))
class SHATTEREDFRONTIER_API USFE1Settings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere, Category="Movement", meta=(ClampMin="0.5", ClampMax="12.0", Units="m/s"))
    float RunSpeedMetresPerSecond = 7.0f;

    UPROPERTY(Config, EditAnywhere, Category="Movement", meta=(ClampMin="0.1", ClampMax="1.0", ToolTip="Backpedal speed as a fraction of the active grounded mode; release-controlled, next startup."))
    float BackpedalFactor = 0.6f;

    UPROPERTY(Config, EditAnywhere, Category="Movement", meta=(ClampMin="0.5", ClampMax="1.2", Units="x", ToolTip="Deep snow terrain speed multiplier; release-controlled, effective on next server and client startup."))
    float DeepSnowSpeedMultiplier = 0.8f;

    UPROPERTY(Config, EditAnywhere, Category="Movement", meta=(ClampMin="0.5", ClampMax="7.0", Units="m/s"))
    float CrouchSpeedMetresPerSecond = 2.0f;

    UPROPERTY(Config, EditAnywhere, Category="Movement", meta=(ClampMin="0.5", ClampMax="12.0", Units="m/s"))
    float SwimSpeedMetresPerSecond = 3.0f;

    UPROPERTY(Config, EditAnywhere, Category="Movement", meta=(ClampMin="0.5", ClampMax="3.0", Units="m"))
    float SwimDepthMetres = 1.3f;

    UPROPERTY(Config, EditAnywhere, Category="Movement", meta=(ClampMin="0.1", ClampMax="3.0", Units="m"))
    float JumpHeightMetres = 1.2f;

    UPROPERTY(Config, EditAnywhere, Category="Movement", meta=(ClampMin="1.0", ClampMax="20.0", ToolTip="Downward gravity magnitude in metres per second squared; applied at startup."))
    float GravityMetresPerSecondSquared = 9.81f;

    UPROPERTY(Config, EditAnywhere, Category="Movement", meta=(ClampMin="0.0", ClampMax="1.0", Units="m"))
    float StepHeightMetres = 0.45f;

    UPROPERTY(Config, EditAnywhere, Category="Movement", meta=(ClampMin="90.0", ClampMax="1080.0", Units="deg/s"))
    float TurnRateDegreesPerSecond = 540.0f;

    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="100.0", ClampMax="1000.0", Units="cm"))
    float CameraBoomLengthCm = 350.0f;

    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="0.1", ClampMax="4.0"))
    float LookSensitivity = 1.0f;

    // CTL-01 player camera (docs/decisions/CTL-01-player-camera-controls.md). Client-only presentation; next startup.
    // CameraBoomLengthCm above is the starting zoom and must lie within [CameraZoomMinCm, CameraZoomMaxCm].
    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="0.02", ClampMax="2.0", Units="deg", ToolTip="Camera rotation per mouse count while RMB/LMB is held."))
    float MouseLookDegreesPerCount = 0.25f;

    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="30.0", ClampMax="720.0", Units="deg/s", ToolTip="Right-stick camera rate at full deflection."))
    float GamepadLookDegreesPerSecond = 180.0f;

    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="30.0", ClampMax="200.0", Units="cm", ToolTip="Closest third-person zoom; scrolling in past it switches to first person."))
    float CameraZoomMinCm = 60.0f;

    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="300.0", ClampMax="2000.0", Units="cm"))
    float CameraZoomMaxCm = 1200.0f;

    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="10.0", ClampMax="500.0", Units="cm", ToolTip="Zoom change per mouse-wheel notch."))
    float CameraZoomStepCm = 100.0f;

    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="1.0", ClampMax="30.0", ToolTip="Zoom interpolation speed (FInterpTo, per second)."))
    float CameraZoomInterpSpeed = 10.0f;

    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="0.0", ClampMax="30.0", ToolTip="Spring-arm position lag speed; 0 turns lag off."))
    float CameraLagSpeed = 12.0f;

    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="0.0", ClampMax="100.0", Units="cm", ToolTip="Camera pivot above the capsule centre (eye height in first person)."))
    float CameraPivotHeightCm = 65.0f;

    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="-89.0", ClampMax="0.0", Units="deg"))
    float CameraPitchMinDegrees = -80.0f;

    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="0.0", ClampMax="89.0", Units="deg"))
    float CameraPitchMaxDegrees = 60.0f;

    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="1.0", ClampMax="20.0", ToolTip="Cursor travel in screen pixels that turns an LMB/RMB press into a camera drag instead of a click."))
    float ClickDragThresholdPixels = 4.0f;

    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="0.05", ClampMax="1.0", Units="s", ToolTip="Longest press that still counts as a click."))
    float ClickMaxSeconds = 0.2f;

    UPROPERTY(Config, EditAnywhere, Category="Camera")
    bool bInvertLookY = false;

    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ToolTip="Auto-swing the camera behind the character while moving and not orbiting."))
    bool bCameraFollowWhileMoving = false;

    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="0.5", ClampMax="10.0", ToolTip="Follow swing speed (FInterpTo, per second)."))
    float CameraFollowInterpSpeed = 3.0f;

    // CTL-02 client presentation. Release authors only; validated at process startup.
    UPROPERTY(Config, EditAnywhere, Category="Camera|Combat Framing", meta=(ToolTip="Shipped master switch; player preference can also disable framing."))
    bool bCombatFramingEnabled = true;
    UPROPERTY(Config, EditAnywhere, Category="Camera|Combat Framing", meta=(ClampMin="100", ClampMax="1200", Units="cm"))
    float CombatFramingMaxZoomCm = 450.0f;
    UPROPERTY(Config, EditAnywhere, Category="Camera|Combat Framing", meta=(ClampMin="0.1", ClampMax="3", Units="s", ToolTip="Blend in duration; blend out takes twice as long."))
    float CombatFramingBlendSeconds = 0.6f;
    UPROPERTY(Config, EditAnywhere, Category="Camera|Combat Framing", meta=(ClampMin="0", ClampMax="10", Units="s"))
    float CombatFramingSteerHoldSeconds = 1.5f;
    UPROPERTY(Config, EditAnywhere, Category="Camera|Combat Framing", meta=(ClampMin="0", ClampMax="120", Units="cm"))
    float CombatShoulderOffsetCm = 55.0f;
    UPROPERTY(Config, EditAnywhere, Category="Camera|Combat Framing", meta=(ClampMin="-60", ClampMax="60", Units="cm"))
    float CombatShoulderHeightCm = -15.0f;
    UPROPERTY(Config, EditAnywhere, Category="Camera|Combat Framing", meta=(ClampMin="0", ClampMax="45", Units="deg"))
    float CombatFramingDeadzoneDegrees = 8.0f;
    UPROPERTY(Config, EditAnywhere, Category="Camera|Combat Framing", meta=(ClampMin="0.5", ClampMax="10", ToolTip="Yaw and pitch interpolation speed in inverse seconds."))
    float CombatFramingYawSpeed = 2.5f;
    UPROPERTY(Config, EditAnywhere, Category="Camera|Combat Framing", meta=(ClampMin="-20", ClampMax="20", Units="deg"))
    float CombatPitchBiasDegrees = -5.0f;
    UPROPERTY(Config, EditAnywhere, Category="Camera|Combat Framing", meta=(ClampMin="0", ClampMax="60", Units="deg", ToolTip="Dialogue: camera yaw past the speaker, so the speaker stands left of the board."))
    float DialogueFramingYawDegrees = 30.0f;
    UPROPERTY(Config, EditAnywhere, Category="Camera|Combat Framing", meta=(ClampMin="0", ClampMax="25", Units="deg", ToolTip="Dialogue: how far above the screen centre the speaker's head is held; 18 puts it near y 30 % at the dialogue arm (r33 measured), as the concept."))
    float DialogueHeadAboveCentreDegrees = 18.0f;
    UPROPERTY(Config, EditAnywhere, Category="Camera|Combat Framing", meta=(ClampMin="150", ClampMax="600", Units="cm", ToolTip="Dialogue: camera distance while the board is open (close and near eye level, as the concept); the player's zoom returns on close."))
    float DialogueArmCm = 230.0f;
    UPROPERTY(Config, EditAnywhere, Category="Camera|Combat Framing", meta=(ClampMin="0", ClampMax="3", Units="s"))
    float CombatOcclusionGraceSeconds = 0.5f;
    UPROPERTY(Config, EditAnywhere, Category="Camera|Combat Framing", meta=(ClampMin="0.02", ClampMax="0.5", Units="s"))
    float CombatOcclusionProbeSeconds = 0.1f;
    UPROPERTY(Config, EditAnywhere, Category="Camera|Combat Framing", meta=(ClampMin="1.2", ClampMax="16", ToolTip="Lens f-number during combat framing."))
    float CombatDofFstop = 2.8f;
    UPROPERTY(Config, EditAnywhere, Category="Camera|Combat Framing", meta=(ClampMin="0", ClampMax="1", ToolTip="Maximum depth-of-field post-process blend weight."))
    float CombatDofStrength = 1.0f;
    UPROPERTY(Config, EditAnywhere, Category="Camera|Combat Framing", meta=(ClampMin="18", ClampMax="50", Units="mm"))
    float CombatDofSensorWidthMm = 36.0f;

    UPROPERTY(Config, EditAnywhere, Category="Movement", meta=(ToolTip="A/D strafe instead of turning when RMB is not held."))
    bool bStrafeWithAD = false;

    UPROPERTY(Config, EditAnywhere, Category="Movement", meta=(ClampMin="0.1", ClampMax="0.9", ToolTip="Walk speed as a fraction of run speed (analog input scale, so it replicates with movement)."))
    float WalkInputScale = 0.35f;

    // E1 fixture declarations: reviewed release authors only; next process startup;
    // no GM/config-service keys. Invalid values fail startup (no silent fallback).
    UPROPERTY(Config, EditAnywhere, Category="Interaction (Release Only)", meta=(ClampMin="0.5", ClampMax="5.0", Units="m", ToolTip="Maximum pawn-origin to target-anchor range. Shipped default 3 m; release review required; next startup only."))
    float InteractionRangeMetres = 3.0f;

    UPROPERTY(Config, EditAnywhere, Category="Interaction (Release Only)", meta=(ClampMin="0.1", ClampMax="10.0", Units="s", ToolTip="Minimum server time between accepted non-economic probes. Default 0.5 s; release review required; next startup only."))
    float InteractionAcceptedCooldownSeconds = 0.5f;

    // Player MetaHuman faces a character may wear (ASFE1Character::FaceId). Each id names a face built by
    // tools/Build-PlayerMetaHumans.py at /Game/Frontier/Characters/Player/Built/MHC_<id>/BP_MHC_<id>; the server rejects any other id.
    // Shipped list in DefaultGame.ini; must include Orlando (the default face), never a *Bare authoring body (OrlandoBare, HildaBare). Next startup.
    UPROPERTY(Config, EditAnywhere, Category="Appearance (Release Only)")
    TArray<FName> PlayerFaceIds = { TEXT("Orlando") };

    // Faces in PlayerFaceIds with a female body: they wear the outfit built on the female bare body (ASFE1Character::OutfitFemale).
    // Every id must be in PlayerFaceIds. Shipped list in DefaultGame.ini. Next startup.
    UPROPERTY(Config, EditAnywhere, Category="Appearance (Release Only)")
    TArray<FName> FemaleFaceIds;

    bool Validate(FString& OutError) const;
};
