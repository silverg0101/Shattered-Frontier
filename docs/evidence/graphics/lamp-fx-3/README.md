# Lamp FX pass 3 evidence

Map: `/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook`. All live frames came from a 1920×1080 `-game` process after the editor closed. `capture_game.py` applies review-only sun and skylight values; no review lighting was saved to the map. `make_pairs.py` places the untouched Street Lamp-1 concept at left and the live frame at right.

## Attachment and colour diagnosis

`five-lamp-offsets.json` records world offsets from the authored glass centre for StreetLamp_001–005. The two flame and two halo card origins are `(0,0,0)` cm for each lamp; smoke is `(0,0,+45)` cm. StreetLamp_003 and 004 fireflies are 1.3–2.6 m from their lamp; the other three have none. StreetLamp_004 has a second lamp socket 112.8 cm away, not a detached effect. The UE 5.8 `transform_location` fix is applied once and HISM instances are placed in world space once. Ember cards were removed.

The actual cyan source was the point-light colour: positional `unreal.Color(255,150,65,255)` produced `r=65,g=150,b=255` in UE Python. The saved light now reports `r=255,g=150,b=65`. The close frames show no flame or ember effect outside the glass; the visible interior flame remains hard to separate from the glass glow. Fireflies are small crossed dot cards, with no ribbon or velocity alignment and a material sun-elevation gate. Street-lamp glass is translucent amber; those mesh components disable Nanite so UE 5.8 can render it.

## Captures

- `day-close-compare.jpg`, `dusk-close-compare.jpg`, `night-close-compare.jpg`: concept beside unblocked lantern.
- `dusk-street-compare.jpg`, `night-street-compare.jpg`: road-facing views with several lamp points.
- `flicker-00.png` through `flicker-04.png`: five close frames. `capture-times.json` gives 0.203 s intervals.

## GPU

`capture_gpu.py` captured the same road-facing view at native 2560×1440, alternating FX on/off twice. The off state hides only `FX_StreetLamp_*` point lights and card components; the source glass remains. `gpu-profile-excerpt.txt` preserves the four UE `profilegpu` frame and graphics-root lines, and `gpu-profile.json` calculates the results. Frame times were 9.03/8.71/8.68/8.76 ms (on/off/on/off); the mean frame delta was 0.120 ms and mean graphics-root delta was **0.313 ms**. Four single frames at one view do not establish a route-wide distribution.

## Open visual differences

The glass remains too bright to read the flame clearly at 3 m, the warm spill overexposes the post compared with the concept, and the simulated night street is still difficult to read between the small lamp pools. Smoke is too subtle to judge from these stills. These views have not passed the required independent concept review; no match is claimed.
