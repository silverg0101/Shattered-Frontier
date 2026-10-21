# Housing PostgreSQL durability fixture

Run `tools/Test-HousingDurable.ps1` with a fresh `-EvidenceRoot` child under `Artifacts/Persistence/HousingDurable`. The executable receives its loopback connection through `HOUSING_CONNECTION`; run phases are orchestrated by the wrapper so PostgreSQL can be stopped immediately between them. The wrapper restores with committed package locks and removes only its marked instance.
