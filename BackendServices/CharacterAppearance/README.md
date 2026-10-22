# Character appearance schema and validator

`content/appearance/human.json` is the shipped, versioned human allowlist. `body` is the Gender step's body type (0 male, 1 female); `heritage` is 0 unset, 1 Valefolk, 2 Northkin, or 3 Seaborn. Heritage does not restrict parts or colours. `height` and `build` are independent quantized u8 body controls; 128 is the neutral creator default. The eight ordered `morph_slots` are face controls and must match the packet count exactly. Palette and part numbers are zero-based indices, not asset IDs.

V1 packs version, race, body, heritage, height, build, morph count, morph bytes, four u16 part indices, four palette bytes, cosmetic count, then up to eight `(slot u8, id u16)` overrides in little-endian order. At the schema maximum of 16 morphs and eight cosmetics this is 60 bytes. Slot 0–17 allows future wardrobe mapping; a server-owned cosmetic catalogue and account inventory are passed separately to `AppearanceValidator`. Zero cosmetic ID means no override and is omitted from the sparse list. Validation returns `APPEARANCE_INVALID` for malformed packets and all invalid selections. Pack/version compatibility must be enforced by the caller before content-pack swaps; there is no character-service or RPC integration in WP1.

From the repository root, run the focused package-free test with the pinned SDK:

```powershell
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe run --project BackendServices/CharacterAppearance.Tests -- content/appearance/human.json
```
