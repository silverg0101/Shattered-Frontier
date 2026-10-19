# Gameplay reuse checkpoint

2026-10-18. The user started without gameplay packs and requested free Fab research beyond Epic defaults, with game design governing selection. The [design candidate register](Game-Design/Legends_Shattered_Frontier_Gameplay_Reuse_Candidate_Register_v0.1.md) records eight listings showing Free at review, sources, implementation types, conflicts and later uses.

First inventory candidate: Minimal Inventory Runtime, with advertised UE 5.8 support but Windows-only platform support; Linux and single-writer integration remain unresolved. Advanced Interaction is a selective prompt/input candidate; Easy Building's gathering components merit inspection without replacing canonical construction. Future dialogue, locomotion and UI references are recorded separately from adoption decisions. Blueprint graphs are editable; Blueprint-callable C++ may require C++ changes.

Research follow-up: the free Minimal Inventory Runtime is saved in the Fab library, and version 2.0.0 source was found already installed locally. The [gathering pilot](gameplay-gathering-pilot.md) now integrates its inventory transaction API in the isolated gameplay worktree. Vendor source/binaries stay outside Git; project-authored item definitions are included. This is a Windows session-only pilot, not production Itemization or Linux acceptance. Other listed assets remain candidates.
