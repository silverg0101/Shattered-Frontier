# Character appearance schema and validator

`content/appearance/human.json` is the shipped, versioned human allowlist. `body` is the Gender step's body type (0 male, 1 female); `heritage` is 0 unset, 1 Valefolk, 2 Northkin, or 3 Seaborn. Heritage does not restrict parts or colours. `height` and `build` are independent quantized u8 body controls; 128 is the neutral creator default. The eight ordered `morph_slots` are face controls and must match the packet count exactly. Palette and part numbers are zero-based indices, not asset IDs. `faces` is the ordered MetaHuman catalogue: indices 0–4 are male (Orlando, Godric, Edwin, Wulfric, Osric), 5–8 are female (Hilda, Aelfwyn, Eadgyth, Ebba). OrlandoBare is not selectable.

V1 byte layout: offset 0 version=1; 1 race; 2 body; 3 heritage; 4 height; 5 build; 6 morph count; 7 onward morph bytes; four little-endian u16 part indices; four palette bytes; cosmetic count; then up to eight `(slot u8, id u16 little-endian)` overrides. V2 has the same layout with version=2 and one trailing face index u8. Maximum sizes are 60 and 61 bytes respectively. V1 still decodes and validates as a legacy packet without a chosen face. V2 requires a face index within `faces` whose body matches the packet body. Slot 0–17 allows future wardrobe mapping; a server-owned cosmetic catalogue and account inventory are passed separately to `AppearanceValidator`. Zero cosmetic ID means no override and is omitted from the sparse list. Validation returns `APPEARANCE_INVALID` for malformed packets and invalid selections. The host must check content-pack compatibility, provide the catalogue and inventory, save the validated bytes through `PostgresCharacterStateStore.SaveAppearance`, and use the stored face index when restoring the character. RPC/gameplay host wiring remains outstanding.

From the repository root, run the focused package-free test with the pinned SDK:

```powershell
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe run --project BackendServices/CharacterAppearance.Tests -- content/appearance/human.json
```
