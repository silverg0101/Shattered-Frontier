# PC graphics baseline candidate — 2026-10-18

Read-only inventory at 23:02 UTC; the user authorized this machine as an initial baseline if necessary. This is a hardware/source record for the [Windows graphics policy](../art-templates/pc-graphics-scalability.md), not a benchmark or minimum/recommended specification.

| Item | Observed value | Read-only source |
| --- | --- | --- |
| OS | Windows 11 Pro; version 10.0.26200, build 26200 | `Get-CimInstance Win32_OperatingSystem` (`Caption`, `Version`, `BuildNumber`) |
| CPU | Intel Core i7-14700K; 20 cores, 28 logical processors | `Win32_Processor` (`Name`, `NumberOfCores`, `NumberOfLogicalProcessors`) |
| System RAM | 63.84 GiB visible physical memory | `Win32_ComputerSystem.TotalPhysicalMemory / 1GB` |
| AMD GPU | Radeon RX 7900 XTX; 25,753,026,560 driver-reported dedicated-memory bytes (~23.984 GiB); driver 32.0.31041.5005 | `Win32_VideoController` name/driver plus display-driver registry `HardwareInformation.qwMemorySize` |
| NVIDIA GPU | GeForce RTX 3060; 12,288 MiB (12 GiB); driver 616.92 / Windows 32.0.16.1692 | `nvidia-smi --query-gpu=name,memory.total,driver_version --format=csv,noheader`, corroborated by CIM/64-bit registry memory |
| Desktop/display mode | AMD controller reports 7680×2160 at 120 Hz; NVIDIA mode fields are null | `Win32_VideoController.CurrentHorizontalResolution`, `CurrentVerticalResolution`, `CurrentRefreshRate` |

Display-driver memory was read through `HKLM:\SYSTEM\CurrentControlSet\Control\Video\<adapter>\0000`, selecting `DriverDesc` and `HardwareInformation.qwMemorySize` only. The legacy 32-bit `MemorySize`/CIM `AdapterRAM` values are unsuitable for these VRAM sizes. No serial numbers, hardware instance IDs, account identifiers or registry key identifiers are retained. The AMD driver value is reported as observed rather than rounded up to an assumed marketing capacity.

The display result describes the controller's current desktop mode; monitor count/physical panel layout and the actual Unreal rendering adapter are unverified. Two GPUs do not supply a combined render-memory budget. Initial Standard-tier validation should record which adapter is selected and measure that adapter independently. The RX 7900 XTX is a candidate because it reports the active desktop mode, not because a UE launch/capture proved selection. The RTX 3060 could supply a separate local comparison only after explicit adapter identity and workload are recorded.

## Repository evidence

- `GameClient/ShatteredFrontier.uproject` associates engine 5.8. [DEC-01](../decisions/DEC-01-engine-evidence.md) records installed UE 5.8.3 CL58210709; that is existing metadata evidence, not a newly measured renderer build/cook.
- `GameClient/Config/DefaultEngine.ini` currently sets `r.DynamicGlobalIlluminationMethod=0`, `r.ReflectionMethod=0`, auto exposure off and motion blur off. Its default maps are `Greenlands_E1`. These explicit fixture settings do not establish a production Lumen path.
- `GameClient/Config/` contains `DefaultEngine.ini`, `DefaultGame.ini`, `DefaultInput.ini`; no project `DefaultScalability.ini` or `DefaultDeviceProfiles.ini` is present at this checkpoint. Engine/inherited/saved defaults and active overrides still need capture; their absence is not evidence that UE scalability is disabled.
- The existing surface-quality JSON is `prototype-authored`, includes 1024–4096 bake bounds with 2048 defaults, and explicitly does not accept engine use. Those are authoring-profile values, not a universal runtime texture limit.

## Unverified acceptance work

No UE session was launched, renderer/project settings changed, assets imported or modified, GPU workload started, worker interrupted, or runtime test run for this record. There is no 1440p performance/memory measurement, selected RHI/SM feature-level confirmation, mature-oak grove scene, minimum-device validation, LOD/HLOD/streaming transition acceptance or production quality pass. The current 7680×2160 desktop mode must not become the acceptance resolution: explicitly use 2560×1440 output and record internal resolution in the future capture.

Choose and pin workload/density, intended mature-tree dimensions/views, profile defaults/bounds, memory reserves, capture duration and hitch tolerances; record adapter/RHI/effective settings and measure before ratifying hardware/tier budgets. Hardware changes replace this baseline record prospectively without rewriting historical captures or turning it into permanent asset limits.
