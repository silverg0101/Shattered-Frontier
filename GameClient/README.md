# E1 Unreal foundation

Open `ShatteredFrontier.uproject` with UE 5.8.3. The shared C++ module supplies Game, Editor, Client and Server targets. The initial Greenlands map is a graybox movement fixture, not a finished biome. WASD moves, mouse looks, Space jumps. The cube avatar is a placeholder.

Build the editor from the repository root:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' ShatteredFrontierEditor Win64 Development "-Project=$PWD\GameClient\ShatteredFrontier.uproject" -WaitMutex -NoHotReloadFromIDE
```

Map source generation is `tools/Create-E1Map.py`, invoked through the editor's Python commandlet. It refuses to overwrite an existing map. Edit the saved map in the editor thereafter. Python and Editor Scripting Utilities are editor-only dependencies.

From the repository root, `./tools/Test-E1Connections.ps1` runs the bounded headless loopback server/two-client connection check, writes ignored logs and stops its own processes. It does not verify rendered movement or lifecycle cleanup. See [recorded evidence and remaining checks](../docs/evidence/e1-foundation.md).

After the Linux server is running, `./tools/Test-E1Remote.ps1 -ServerAddress steam-server -Port 17777` checks two independent client welcomes for the expected E1 map/mode and stops its own clients. Inspect server logs separately for distinct authoritative possessions; no rendered movement or account-identity acceptance is implied.

`SFE1GameMode` owns spawn/possession through Unreal's standard authoritative GameMode. `SFE1Character` uses CharacterMovement replication and collision. This local development fixture has no account authentication, persistence, interaction target, production backend or economic actions. Keep its server on a trusted development network.

## Release-controlled fixture settings

The typed `SFE1Settings` object reads the shipped `Config/DefaultGame.ini` section `/Script/ShatteredFrontier.SFE1Settings`. Release authors may change these values within the declared bounds; they take effect at process startup. Missing entries use shipped C++ defaults; non-finite or out-of-range values fail startup. These are prospective fixture settings, not live GM controls. Any future live controls must use the shared configuration workflow and its permissions/audit rules. Algorithms, replication authority and map topology remain versioned code/content.

| Plain-language setting | Property | Default | Bounds | Units |
| --- | --- | --- | --- | --- |
| Run speed | RunSpeedMetresPerSecond | 7 | 0.5–12 | m/s |
| Jump height | JumpHeightMetres | 1.2 | 0.1–3 | m |
| Gravity magnitude | GravityMetresPerSecondSquared | 9.81 | 1–20 | m/s² |
| Step height | StepHeightMetres | 0.45 | 0–1 | m |
| Character turn rate | TurnRateDegreesPerSecond | 540 | 90–1080 | degrees/s |
| Camera distance | CameraBoomLengthCm | 350 | 100–1000 | cm |
| Mouse look sensitivity | LookSensitivity | 1 | 0.1–4 | multiplier |

Run speed, jump height, gravity and step height follow the canonical unmodified baseline. Launch speed derives from height/gravity; the character scales world gravity to the declared magnitude on startup. Air control is disabled. Other defaults are E1 fixture choices, not acceptance of the full movement design (including terminal speed and modifiers). Fixture geometry is described in the generator in centimetres.

The dedicated destination is Linux x86_64 Docker on `steam-server`, selected by the user after initial Unraid assessment. See [the Linux build route](../docs/e1-linux-server-build.md). The Launcher engine can build this editor but cannot package the Server target. A matching source engine and Linux SDK are required before packaging or deployment.
