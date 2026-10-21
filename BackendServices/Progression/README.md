# Progression receipt model

Package-free in-memory model for accepted gathering harvest, craft conversion, and enemy defeat receipts. It uses registry IDs `skill/logging`, `skill/mining`, and `skill/smithing`. Formula version 1 is fixed in code; an injected clock and roll source make outcomes repeatable. Accepted receipt IDs are retained for idempotency. Rejected receipts have no effect. Practice grants character XP even when a skill roll fails or its unspecialized cap blocks a gain.

`ProgressionSettings` is an immutable effective snapshot with typed bounded defaults. It declares base gain rate, maximum chance, attempt weight, fatigue and repetition windows, thresholds, and hourly gain reference attempts. The shared configuration workflow must publish it; this model has no console editor. The snapshot is captured at character-model creation and applies until that instance ends. No runtime config-service connection exists yet.

Run `& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe run --project BackendServices/Progression.Tests --locked-mode` from the repository root.

`CharacterState.Postgres` persists snapshots, effective settings and source receipts through this model. This slice has no producer authentication, skill locks/donors, specialization, rested/diversity modifiers, timing/risk anti-macro signals, or production service adapter. Enemy defeat uses base eligible kill XP with tier, level difference, and party scaling; trusted producers must validate combat eligibility before acceptance.
