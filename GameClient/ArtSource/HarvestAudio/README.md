# Harvest audio source candidates

Four CC0 Kenney Impact Sounds candidates are preserved as original OGG files and imported-source WAVs. The pack page lists the asset as Creative Commons CC0; the accompanying `License.txt` is copied from the original archive. The original archive SHA-256 is `029D734AF1582474EDF3A694D1B0CEBC97C1C152F2F39FA34D4C2BAFC5DE77F8`. Full source URLs, per-file hashes, sample properties, and conversion measurements are in `manifest.json`.

The WAV files are mono, 44.1 kHz, signed 16-bit PCM. Conversion used installed Blender 5.2.2 audaspace to decode OGG to PCM, then averaged the stereo channels, cropped to the source OGG granule duration, and set each peak to about -3 dBFS. No EQ, compression, limiting, or time stretching was applied. None of these candidates has been auditioned; material labels and waveform duration/transient-energy variation guided the bounded selection. Treat them as prototypes for context audition and mix review, not accepted production audio. The source pack does not include clearly identified bark-splinter, rock-debris, or tree-fall sounds.

`tools/Import-HarvestAudio.py` imports only the four WAVs as SoundWave assets into a new `/Game/Frontier/Gameplay/Harvest/AudioV1` folder. It refuses an existing destination and does not edit project-wide settings or package assets.

AudioV1 was imported successfully on 2026-10-19. Runtime `SFHarvestImpactAudio` selects seeded wood/mining variants with distance attenuation and bounded per-resource concurrency, called only for newly approved extraction cuts. The actual-contact regression passed with `-nosound`; context audition and mix approval remain open.
