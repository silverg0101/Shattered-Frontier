# Legends of the Shattered Frontier — Combat Part 6: Starter Stats, Weapons, Ability Coefficients, Mitigation and Greenlands NPCs (levels 1–20)
### Version 0.1 — design draft (2026-10-25)

Scope: the values the shared server resolution path (`Combat/SFCombatResolution.h`, `ASFGameMode::ResolveCombatAttack`, commit 2427fada) still lacks: authoritative character stats per archetype and level, weapon damage lines consistent with `content/items/gear_greenlands_starter.json` (9dafb6ed), damage and heal coefficients for the 38 Part 5 abilities, spread, armor and resistance curves, shield block, absorbs, and stat lines for the six Silverbrook creatures. Every number here is derived from Parts 1–5, the Itemization Stat Budget and the Character spec; nothing is new design except the choices listed in §10. Formulas stay in Parts 1–2 and in code; this part only authors inputs and declares their controls. `L` is the synced level; all tables stop at 20 because `zone/greenlands_silverbrook` is levels 1–8 and the starter kits end at 20.

## 1. Level constants (Parts 1–2, computed)

`s(L) = B(4L)/B(400)`, `K_stat(L) = K_stat_cap × s(L)`, `skill_ref(L) = round(900 × (L/100)^0.85)`, `σ(L) ≈ 0.04 × (400 + 44.5L)/(38.0 + 5.882L)`, `H_ref(L) = 400 + 44.5L`, `attr_ref(L) = round(120 + 460 × ((L − 1)/99)^1.1)`. Implementations compute these; the table is for checking.

| L | s(L) | K_crit = K_dodge = K_parry | K_haste | K_block | K_pen | K_crit_dmg | skill_ref | σ(L) | H_ref | attr_ref |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 0.0419 | 58.7 | 73.4 | 29.4 | 16.1 | 6.1 | 18 | 0.4052 | 444.5 | 120 |
| 2 | 0.0516 | 72.3 | 90.3 | 36.1 | 19.9 | 7.5 | 32 | 0.3931 | 489.0 | 123 |
| 3 | 0.0613 | 85.8 | 107.3 | 42.9 | 23.6 | 8.9 | 46 | 0.3835 | 533.5 | 126 |
| 4 | 0.0710 | 99.4 | 124.2 | 49.7 | 27.3 | 10.4 | 58 | 0.3758 | 578.0 | 130 |
| 5 | 0.0806 | 112.9 | 141.1 | 56.5 | 31.0 | 11.8 | 71 | 0.3694 | 622.5 | 133 |
| 6 | 0.0903 | 126.5 | 158.1 | 63.2 | 34.8 | 13.2 | 82 | 0.3640 | 667.0 | 137 |
| 7 | 0.1000 | 140.0 | 175.0 | 70.0 | 38.5 | 14.6 | 94 | 0.3595 | 711.5 | 141 |
| 8 | 0.1097 | 153.5 | 191.9 | 76.8 | 42.2 | 16.0 | 105 | 0.3555 | 756.0 | 145 |
| 9 | 0.1194 | 167.1 | 208.9 | 83.5 | 46.0 | 17.4 | 116 | 0.3521 | 800.5 | 149 |
| 10 | 0.1290 | 180.6 | 225.8 | 90.3 | 49.7 | 18.8 | 127 | 0.3491 | 845.0 | 153 |
| 11 | 0.1387 | 194.2 | 242.7 | 97.1 | 53.4 | 20.3 | 138 | 0.3464 | 889.5 | 157 |
| 12 | 0.1484 | 207.7 | 259.7 | 103.9 | 57.1 | 21.7 | 148 | 0.3441 | 934.0 | 161 |
| 13 | 0.1581 | 221.3 | 276.6 | 110.6 | 60.9 | 23.1 | 159 | 0.3419 | 978.5 | 165 |
| 14 | 0.1677 | 234.8 | 293.5 | 117.4 | 64.6 | 24.5 | 169 | 0.3400 | 1023.0 | 169 |
| 15 | 0.1774 | 248.4 | 310.5 | 124.2 | 68.3 | 25.9 | 179 | 0.3383 | 1067.5 | 173 |
| 16 | 0.1871 | 261.9 | 327.4 | 131.0 | 72.0 | 27.3 | 190 | 0.3367 | 1112.0 | 178 |
| 17 | 0.1968 | 275.5 | 344.4 | 137.7 | 75.8 | 28.7 | 200 | 0.3352 | 1156.5 | 182 |
| 18 | 0.2065 | 289.0 | 361.3 | 144.5 | 79.5 | 30.1 | 210 | 0.3339 | 1201.0 | 186 |
| 19 | 0.2161 | 302.6 | 378.2 | 151.3 | 83.2 | 31.6 | 219 | 0.3327 | 1245.5 | 191 |
| 20 | 0.2258 | 316.1 | 395.2 | 158.1 | 86.9 | 33.0 | 229 | 0.3315 | 1290.0 | 195 |

## 2. Character stats per archetype and level

### 2.1 Natural attributes

The six starter kits of Part 5 are the six archetypes. Each starts at Part 5 §1.1's creation allocation (10 base + 40 creation points, at most +30 on one attribute) and grows along the Character spec's median natural total (§4.4: 120 at creation, 150 at L10, 210 at L25), interpolated linearly:

```text
N_ref(L) = 120 + 30 × (L − 1)/9            L ≤ 10
N_ref(L) = 150 + 60 × (L − 10)/15          10 < L ≤ 25
A_i(L)   = 10 + round((N_ref(L) − 80) × share_i)      share_i = the kit's creation share (sums to 1)
```

Level points (2 per level) and use-based gains are folded into one line because this part needs a deterministic reference, not an allocation UI; the live character keeps the Character spec's rules and this table is the server's **fallback snapshot** when no Progression record exists (§9).

| Kit (Part 5) | Creation allocation | Shares |
|---|---|---|
| Weapon ("Fighter") | str 30, con 25, dex 15 | str .50, con .375, dex .125 |
| Guardian | str 25, con 30, wil 15 | con .50, str .375, wil .125 |
| Archery ("Archer") | dex 40, con 20 | dex .75, con .25 |
| Magic ("Mage") | int 40, wis 20 | int .75, wis .25 |
| Healing ("Healer") | wis 40, int 20 | wis .75, int .25 |
| Shadow | dex 40, str 20 | dex .75, str .25 |

Fighter, Archer, Mage and Healer are Part 5's four starters; Guardian and Shadow allocations are chosen here (§10). Derived values use the Part 1 formulas unchanged: `Health = 100 + 15 con + 25 L`, `Mana = 100 + 20 int + 10 L`, `Stamina = 100 + 5 con + 3 dex`, `PhysicalPower = 2 str`, `SpellPower = 2 int`, `crit = 0.05 + dex/5000` (no ratings on starter gear), `heal_wis_mult = 1 + wis/1000`. Other attributes stay at 10.

| Kit | L | str | dex | con | int | wis | wil | Health | Mana | Stamina | PP | SP | crit | mana regen /s (combat) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Weapon | 1 | 30 | 15 | 25 | 10 | 10 | 10 | 500 | 310 | 270 | 60 | 20 | 5.3 % | 1.0 |
| Weapon | 5 | 37 | 17 | 30 | 10 | 10 | 10 | 675 | 350 | 301 | 74 | 20 | 5.3 % | 1.1 |
| Weapon | 10 | 45 | 19 | 36 | 10 | 10 | 10 | 890 | 400 | 337 | 90 | 20 | 5.4 % | 1.3 |
| Weapon | 15 | 55 | 21 | 44 | 10 | 10 | 10 | 1,135 | 450 | 383 | 110 | 20 | 5.4 % | 1.4 |
| Weapon | 20 | 65 | 24 | 51 | 10 | 10 | 10 | 1,365 | 500 | 427 | 130 | 20 | 5.5 % | 1.6 |
| Guardian | 1 | 25 | 10 | 30 | 10 | 10 | 15 | 575 | 310 | 280 | 50 | 20 | 5.2 % | 1.0 |
| Guardian | 5 | 30 | 10 | 37 | 10 | 10 | 17 | 780 | 350 | 315 | 60 | 20 | 5.2 % | 1.1 |
| Guardian | 10 | 36 | 10 | 45 | 10 | 10 | 19 | 1,025 | 400 | 355 | 72 | 20 | 5.2 % | 1.3 |
| Guardian | 15 | 44 | 10 | 55 | 10 | 10 | 21 | 1,300 | 450 | 405 | 88 | 20 | 5.2 % | 1.4 |
| Guardian | 20 | 51 | 10 | 65 | 10 | 10 | 24 | 1,575 | 500 | 455 | 102 | 20 | 5.2 % | 1.6 |
| Archery | 1 | 10 | 40 | 20 | 10 | 10 | 10 | 425 | 310 | 320 | 20 | 20 | 5.8 % | 1.0 |
| Archery | 5 | 10 | 50 | 23 | 10 | 10 | 10 | 570 | 350 | 365 | 20 | 20 | 6.0 % | 1.1 |
| Archery | 10 | 10 | 63 | 28 | 10 | 10 | 10 | 770 | 400 | 429 | 20 | 20 | 6.3 % | 1.3 |
| Archery | 15 | 10 | 78 | 33 | 10 | 10 | 10 | 970 | 450 | 499 | 20 | 20 | 6.6 % | 1.4 |
| Archery | 20 | 10 | 93 | 38 | 10 | 10 | 10 | 1,170 | 500 | 569 | 20 | 20 | 6.9 % | 1.6 |
| Magic | 1 | 10 | 10 | 10 | 40 | 20 | 10 | 275 | 910 | 180 | 20 | 80 | 5.2 % | 3.1 |
| Magic | 5 | 10 | 10 | 10 | 50 | 23 | 10 | 375 | 1,150 | 180 | 20 | 100 | 5.2 % | 4.0 |
| Magic | 10 | 10 | 10 | 10 | 63 | 28 | 10 | 500 | 1,460 | 180 | 20 | 126 | 5.2 % | 5.2 |
| Magic | 15 | 10 | 10 | 10 | 78 | 33 | 10 | 625 | 1,810 | 180 | 20 | 156 | 5.2 % | 6.6 |
| Magic | 20 | 10 | 10 | 10 | 93 | 38 | 10 | 750 | 2,160 | 180 | 20 | 186 | 5.2 % | 8.1 |
| Healing | 1 | 10 | 10 | 10 | 20 | 40 | 10 | 275 | 510 | 180 | 20 | 40 | 5.2 % | 1.9 |
| Healing | 5 | 10 | 10 | 10 | 23 | 50 | 10 | 375 | 610 | 180 | 20 | 46 | 5.2 % | 2.4 |
| Healing | 10 | 10 | 10 | 10 | 28 | 63 | 10 | 500 | 760 | 180 | 20 | 56 | 5.2 % | 3.2 |
| Healing | 15 | 10 | 10 | 10 | 33 | 78 | 10 | 625 | 910 | 180 | 20 | 66 | 5.2 % | 4.1 |
| Healing | 20 | 10 | 10 | 10 | 38 | 93 | 10 | 750 | 1,060 | 180 | 20 | 76 | 5.2 % | 5.2 |
| Shadow | 1 | 20 | 40 | 10 | 10 | 10 | 10 | 275 | 310 | 270 | 40 | 20 | 5.8 % | 1.0 |
| Shadow | 5 | 23 | 50 | 10 | 10 | 10 | 10 | 375 | 350 | 300 | 46 | 20 | 6.0 % | 1.1 |
| Shadow | 10 | 28 | 63 | 10 | 10 | 10 | 10 | 500 | 400 | 339 | 56 | 20 | 6.3 % | 1.3 |
| Shadow | 15 | 33 | 78 | 10 | 10 | 10 | 10 | 625 | 450 | 384 | 66 | 20 | 6.6 % | 1.4 |
| Shadow | 20 | 38 | 93 | 10 | 10 | 10 | 10 | 750 | 500 | 429 | 76 | 20 | 6.9 % | 1.6 |

Stamina regeneration in combat is `10 + dex/50` (10.2–11.9 /s) for every kit. Part 5 §1.1 listed health at L10/L20 without attribute growth (Fighter 725/975); this table supersedes those two columns (Fighter 890/1,365).

### 2.2 Reference skill line

Creation gives up to 150 skill points, 100 maximum per skill (Character spec §7.1). The reference starter puts 100 on the kit's governing skill and 50 on `tactics` (weapon kits) or on the kit's second skill, then keeps pace with `skill_ref(L)` once that passes the creation value:

```text
S_kit(L)     = max(100, skill_ref(L))        S_tactics(L) = max(50, round(0.75 × skill_ref(L)))
skill_mult   = 0.80 + 0.20 × min(1.10, S_kit / max(1, skill_ref(L)))         (Part 1)
S_hit        = S_kit + 0.10 × S_tactics      miss = clamp((D_def − S_hit)/2000, 0, 0.30)
```

| L | skill_ref | S_kit | S_tactics | skill_mult | S_hit | miss vs common NPC of level L (D_def = skill_ref) | `shield_defense` block bonus |
|---|---|---|---|---|---|---|---|
| 1–6 | 18–82 | 100 | 50–62 | 1.020 | 105.0–106.2 | 0 | 10.0 % |
| 7 | 94 | 100 | 71 | 1.013 | 107.1 | 0 | 10.0 % |
| 8 | 105 | 105 | 79 | 1.000 | 112.9 | 0 | 10.0 % |
| 10 | 127 | 127 | 95 | 1.000 | 136.5 | 0 | 10.0 % |
| 15 | 179 | 179 | 134 | 1.000 | 192.4 | 0 | 10.0 % |
| 20 | 229 | 229 | 172 | 1.000 | 246.2 | 0 | 10.0 % |

A starter at or above `skill_ref` never misses a common NPC of its level; elites (+50) and mini bosses (+100) add at most 2.5 % and 5 % miss (D_def − S_hit over 2,000). A live character uses its real skills; this line is the fallback snapshot only.

## 3. Weapon damage (Itemization §3.1, levels 1–20)

```text
weapon_dps = 4.0 × B(ilvl) × rarity_mult × hand_factor      B(ilvl) = 1 + 0.075 × ilvl,  levelling ilvl = 4 × L
avg_hit    = weapon_dps × speed_s;  min_hit = round(0.85 × avg_hit);  max_hit = round(1.15 × avg_hit)
```

`rarity_mult` common 0.70 / uncommon 0.85 / rare 1.00; `hand_factor` main_hand 1.00, two_hand 1.35 (bows and staves are two-hand), off_hand 0.50; `unarmed_dps(L) = 0.40 × 4.0 × B(4L)` at `speed_s` 2.0.

### 3.1 Authored speeds (content, `speed_s` 1.4–3.8)

| Weapon type | `speed_s` | Source |
|---|---|---|
| Shortsword, longsword, cudgel/mace, brigand blade (one-hand) | 2.0 | implied by 9dafb6ed's pilot damage (`captains_longsword` 20, `brigand_blade` 16, `cudgel` 12 reproduce only at 2.0) |
| Dagger | 1.4 | implied by `fangtooth_dagger` 14 |
| Bow (two-hand, `ranged` tag, 30 m) | 2.8 | chosen (§10) |
| Staff (two-hand implement) | 3.4 | Part 1 reference two-hand |
| Two-hand axe, sword, polearm | 3.4 | Part 1 reference two-hand |

### 3.2 The five weapons in `gear_greenlands_starter.json`

9dafb6ed scaled design values onto the pilot's flat units through the `iron_shortsword` anchor: **1 pilot damage = 1.2133 avg_hit** (7.28/6) and, from `hide_jerkin`, **1 pilot armor = 3.64 armor** (10.92/3). These two ratios are the development conversion (`tune.combat.dev.pilot_damage_unit`, `tune.combat.dev.pilot_armor_unit`, §8) until `SFGear` carries design stats.

| Item | ilvl (4 × content level, +1 elite, +2 mini boss) | Rarity | `speed_s` | `weapon_dps` | `avg_hit` | min–max | pilot damage (avg / 1.2133) |
|---|---|---|---|---|---|---|---|
| `item/iron_shortsword` | 4 | common | 2.0 | 3.64 | 7.28 | 6–8 | 6 |
| `item/cudgel` | 20 | common | 2.0 | 7.00 | 14.00 | 12–16 | 12 (11.5) |
| `item/brigand_blade` | 20 | rare | 2.0 | 10.00 | 20.00 | 17–23 | 16 (16.5) |
| `item/fangtooth_dagger` | 28 | rare | 1.4 | 12.40 | 17.36 | 15–20 | 14 (14.3) |
| `item/captains_longsword` | 28 | rare | 2.0 | 12.40 | 24.80 | 21–29 | 20 (20.4) |

### 3.3 Weapon lines by level (ilvl 4L; `weapon_dps` / min–max)

| L | ilvl | B | common 1H 2.0 s | rare 1H 2.0 s | common dagger 1.4 s | common 2H / staff 3.4 s | common bow 2.8 s | unarmed dps |
|---|---|---|---|---|---|---|---|---|
| 1 | 4 | 1.30 | 3.64 / 6–8 | 5.20 / 9–12 | 3.64 / 4–6 | 4.91 / 14–19 | 4.91 / 12–16 | 2.08 |
| 2 | 8 | 1.60 | 4.48 / 8–10 | 6.40 / 11–15 | 4.48 / 5–7 | 6.05 / 17–24 | 6.05 / 14–19 | 2.56 |
| 3 | 12 | 1.90 | 5.32 / 9–12 | 7.60 / 13–17 | 5.32 / 6–9 | 7.18 / 21–28 | 7.18 / 17–23 | 3.04 |
| 4 | 16 | 2.20 | 6.16 / 10–14 | 8.80 / 15–20 | 6.16 / 7–10 | 8.32 / 24–33 | 8.32 / 20–27 | 3.52 |
| 5 | 20 | 2.50 | 7.00 / 12–16 | 10.00 / 17–23 | 7.00 / 8–11 | 9.45 / 27–37 | 9.45 / 22–30 | 4.00 |
| 6 | 24 | 2.80 | 7.84 / 13–18 | 11.20 / 19–26 | 7.84 / 9–13 | 10.58 / 31–41 | 10.58 / 25–34 | 4.48 |
| 7 | 28 | 3.10 | 8.68 / 15–20 | 12.40 / 21–29 | 8.68 / 10–14 | 11.72 / 34–46 | 11.72 / 28–38 | 4.96 |
| 8 | 32 | 3.40 | 9.52 / 16–22 | 13.60 / 23–31 | 9.52 / 11–15 | 12.85 / 37–50 | 12.85 / 31–41 | 5.44 |
| 9 | 36 | 3.70 | 10.36 / 18–24 | 14.80 / 25–34 | 10.36 / 12–17 | 13.99 / 40–55 | 13.99 / 33–45 | 5.92 |
| 10 | 40 | 4.00 | 11.20 / 19–26 | 16.00 / 27–37 | 11.20 / 13–18 | 15.12 / 44–59 | 15.12 / 36–49 | 6.40 |
| 11 | 44 | 4.30 | 12.04 / 20–28 | 17.20 / 29–40 | 12.04 / 14–19 | 16.25 / 47–64 | 16.25 / 39–52 | 6.88 |
| 12 | 48 | 4.60 | 12.88 / 22–30 | 18.40 / 31–42 | 12.88 / 15–21 | 17.39 / 50–68 | 17.39 / 41–56 | 7.36 |
| 13 | 52 | 4.90 | 13.72 / 23–32 | 19.60 / 33–45 | 13.72 / 16–22 | 18.52 / 54–72 | 18.52 / 44–60 | 7.84 |
| 14 | 56 | 5.20 | 14.56 / 25–33 | 20.80 / 35–48 | 14.56 / 17–23 | 19.66 / 57–77 | 19.66 / 47–63 | 8.32 |
| 15 | 60 | 5.50 | 15.40 / 26–35 | 22.00 / 37–51 | 15.40 / 18–25 | 20.79 / 60–81 | 20.79 / 49–67 | 8.80 |
| 16 | 64 | 5.80 | 16.24 / 28–37 | 23.20 / 39–53 | 16.24 / 19–26 | 21.92 / 63–86 | 21.92 / 52–71 | 9.28 |
| 17 | 68 | 6.10 | 17.08 / 29–39 | 24.40 / 41–56 | 17.08 / 20–27 | 23.06 / 67–90 | 23.06 / 55–74 | 9.76 |
| 18 | 72 | 6.40 | 17.92 / 30–41 | 25.60 / 44–59 | 17.92 / 21–29 | 24.19 / 70–95 | 24.19 / 58–78 | 10.24 |
| 19 | 76 | 6.70 | 18.76 / 32–43 | 26.80 / 46–62 | 18.76 / 22–30 | 25.33 / 73–99 | 25.33 / 60–82 | 10.72 |
| 20 | 80 | 7.00 | 19.60 / 33–45 | 28.00 / 48–64 | 19.60 / 23–32 | 26.46 / 76–103 | 26.46 / 63–85 | 11.20 |

## 4. Spread

`spread = WeaponDamage / avg_hit`, one sample from `[min_hit, max_hit]` drawn with `d_spread` (uniform, Part 2): `spread = lerp(1 − WEAPON_SPREAD, 1 + WEAPON_SPREAD, d_spread)` = `[0.85, 1.15]`. It multiplies only the weapon term of `Raw` (`weapon_dps × t_ref`), never the Power term, which is exactly what `FAttack.SpreadWeaponDamage` / `SpreadMinimum` / `SpreadMaximum` implement: `SpreadWeaponDamage = weapon_dps × t_ref × AbilityModifier × σ × skill_mult` (the weapon portion of `RawDamage` at spread 1.0), `SpreadMinimum` 0.85, `SpreadMaximum` 1.15, `bHasSpread` true whenever a weapon or implement is equipped. Spells and heals sample the implement's spread the same way; NPC damage has no spread (`bHasSpread` false; NPC variance is the crit roll). Unarmed uses `unarmed_dps(L)` with the same spread. Key: `tune.item.weapon_spread` 0.15, bounds 0–0.30, step 0.01, realm / `restart` (changes min/max on every weapon tooltip).

## 5. Ability damage, heal and absorb coefficients (38 abilities)

The coefficient of every Part 5 ability is its `power.modifier` with its `t_ref` (Part 2: instant 1.5 s, `short` GCD 1.0 s, cast `max(cast_s, 1.5)`, off-GCD 0.5 s, DoT/HoT the authored `t_ref` per tick); the server computes

```text
RawDamage = (weapon_dps × t_ref × spread + Power × t_ref / 2.4) × modifier × σ(L) × skill_mult × Π(1 + Σ category_mods)
Heal      = RawDamage form with SpellPower, × heal_wis_mult (1 + wis/1000) × target healing-taken; never mitigated
Absorb    = Heal form with amount_modifier; can_crit false
```

Power is `PhysicalPower` for `physical` and for the Archery/Shadow poison DoTs (weapon-delivered), `SpellPower` for every spell school and heal. The columns below are the expected pre-mitigation, non-crit, spread-1.0 values for the kit's reference starter (§2) with a **common ilvl 4L** implement (§3: one-hand 2.0 s for Weapon/Guardian, dagger 1.4 s for Shadow, bow 2.8 s for Archery, staff 3.4 s for Magic/Healing) and the §2.2 `skill_mult`. DoT/HoT rows are totals over all ticks; per-target rows are after the AoE soft cap of 1.0 (≤ 5 targets). Utility abilities (`defensive_stance`, `shield_block`, `challenge`, `shield_wall`, `bulwark`, `disengage`, `steady_focus`, `counterspell`, `entangle`, `blink`, `purify`, `revive`, `hide`, `poisoned_blade` trigger, `vanish`) carry no coefficient; their values are complete in Part 5.

| Kit | Ability | School | `modifier` | `t_ref` s | L1 | L5 | L10 | L15 | L20 |
|---|---|---|---|---|---|---|---|---|---|
| Weapon | auto-attack (one-hand 2.0 s) | physical | 1.0 | 2.0 | 23.7 | 28.5 | 34.0 | 41.4 | 48.9 |
| Weapon | `strike` | physical | 0.7 | 1.5 | 12.4 | 15.0 | 17.9 | 21.7 | 25.7 |
| Weapon | `heavy_blow` | physical | 1.5 | 1.5 | 26.6 | 32.1 | 38.3 | 46.6 | 55.0 |
| Weapon | `cleave` (per target, ≤ 3) | physical | 0.42 | 1.5 | 7.5 | 9.0 | 10.7 | 13.0 | 15.4 |
| Weapon | `pommel_strike` (off-GCD) | physical | 0.3 | 0.5 | 1.8 | 2.1 | 2.6 | 3.1 | 3.7 |
| Weapon | `second_wind` (total, 5 ticks) | heal, 15 % max health | — | — | 75 | 101 | 134 | 170 | 205 |
| Weapon | `rend` (total, 6 ticks over 12 s) | physical DoT | 1.8 | 1.5 | 32.0 | 38.5 | 45.9 | 55.9 | 66.0 |
| Weapon | `charge` (off-GCD) | physical | 0.5 | 0.5 | 3.0 | 3.6 | 4.3 | 5.2 | 6.1 |
| Guardian | auto-attack (one-hand 2.0 s) | physical | 1.0 | 2.0 | 20.2 | 24.1 | 28.8 | 35.2 | 41.2 |
| Guardian | `shield_bash` (`threat.mult` 1.5) | physical | 1.0 | 1.5 | 15.2 | 18.1 | 21.6 | 26.4 | 30.9 |
| Archery | auto-attack (bow 2.8 s) | physical | 1.0 | 2.8 | 15.3 | 18.8 | 22.9 | 27.6 | 32.3 |
| Archery | `quick_shot` | physical | 0.7 | 1.5 | 5.7 | 7.0 | 8.6 | 10.3 | 12.1 |
| Archery | `aimed_shot` (2.0 s cast) | physical | 1.3 | 2.0 | 14.2 | 17.4 | 21.3 | 25.6 | 30.0 |
| Archery | `crippling_shot` | physical | 0.6 | 1.5 | 4.9 | 6.0 | 7.4 | 8.9 | 10.4 |
| Archery | `multi_shot` (per target, ≤ 3) | physical | 0.42 | 1.5 | 3.4 | 4.2 | 5.2 | 6.2 | 7.3 |
| Archery | `serpent_sting` (total, 5 ticks over 15 s) | poison DoT | 1.6 | 1.5 | 13.1 | 16.1 | 19.7 | 23.6 | 27.7 |
| Magic | `fire_bolt` (2.0 s cast) | fire | 1.4 | 2.0 | 44.3 | 53.9 | 66.1 | 81.3 | 96.5 |
| Magic | `frost_shard` | frost | 0.7 | 1.5 | 16.6 | 20.2 | 24.8 | 30.5 | 36.2 |
| Magic | `arcane_ward` (absorb, 20 s) | arcane, all schools | 2.0 | 1.5 | 48.4 | 59.1 | 72.8 | 89.9 | 107.3 |
| Magic | `flame_burst` (per target, ≤ 8) | fire | 0.9 | 1.5 | 21.3 | 26.0 | 31.9 | 39.2 | 46.5 |
| Healing | `mend` (2.0 s cast) | holy heal | 1.5 | 2.0 | 27.8 | 34.0 | 42.8 | 52.8 | 63.2 |
| Healing | `smite` (1.5 s cast) | holy | 1.0 | 1.5 | 13.4 | 16.2 | 20.1 | 24.5 | 28.9 |
| Healing | `renew` (total, 4 ticks over 12 s) | holy HoT | 1.2 | 1.5 | 16.7 | 20.4 | 25.7 | 31.7 | 37.9 |
| Healing | `flash_mend` (1.0 s cast) | holy heal | 1.0 | 1.5 | 13.9 | 17.0 | 21.4 | 26.4 | 31.6 |
| Healing | `healing_circle` (per target, ≤ 5; 2.5 s cast) | holy heal | 0.9 | 2.5 | 20.9 | 25.5 | 32.1 | 39.6 | 47.4 |
| Shadow | auto-attack (dagger 1.4 s) | physical | 1.0 | 1.4 | 11.7 | 13.8 | 16.9 | 20.3 | 23.8 |
| Shadow | `backstab` (from stealth, behind) | physical | 2.4 | 1.5 | 30.2 | 35.5 | 43.4 | 52.2 | 61.2 |
| Shadow | `effect/weak_poison` (total per stack, 6 s, ≤ 3 stacks) | poison DoT | 0.6 | 1.5 | 7.6 | 8.9 | 10.9 | 13.1 | 15.3 |

Crit: players `crit_damage` 2.0 (no `crit_damage_rating` on starter gear); heals crit at the same chance; `arcane_ward` and `second_wind` never crit. Bounds are Part 3's authoring validator (filler 0.6–1.0, cooldown ≤ 1 + cd/6, AoE per target ≤ 0.6 × single, DoT ≤ 1.2 × instant); every Part 5 modifier is inside them.

## 6. Mitigation curves

### 6.1 Armor (Part 2)

`DamageReduction = EffectiveArmor / (EffectiveArmor + 100 × AttackerLevel)`, cap 0.85; `EffectiveArmor = Armor × (1 − ArmorPen%)`, `ArmorPen%` ≤ 0.60 (0 on starter gear). The divisor grows with the **attacker's** level, so at low levels one level of difference moves reduction a lot (a L1 attacker against a L2 boar's 108 armor meets 52 %, a L2 attacker 35 %).

| Armor | attacker L1 | L2 | L5 | L7 | L10 | L15 | L20 |
|---|---|---|---|---|---|---|---|
| 10 | 9.1 % | 4.8 % | 2.0 % | 1.4 % | 1.0 % | 0.7 % | 0.5 % |
| 25 | 20.0 % | 11.1 % | 4.8 % | 3.4 % | 2.4 % | 1.6 % | 1.2 % |
| 50 | 33.3 % | 20.0 % | 9.1 % | 6.7 % | 4.8 % | 3.2 % | 2.4 % |
| 100 | 50.0 % | 33.3 % | 16.7 % | 12.5 % | 9.1 % | 6.2 % | 4.8 % |
| 150 | 60.0 % | 42.9 % | 23.1 % | 17.6 % | 13.0 % | 9.1 % | 7.0 % |
| 200 | 66.7 % | 50.0 % | 28.6 % | 22.2 % | 16.7 % | 11.8 % | 9.1 % |
| 300 | 75.0 % | 60.0 % | 37.5 % | 30.0 % | 23.1 % | 16.7 % | 13.0 % |
| 500 | 83.3 % | 71.4 % | 50.0 % | 41.7 % | 33.3 % | 25.0 % | 20.0 % |
| 1,000 | 85.0 % (cap) | 83.3 % | 66.7 % | 58.8 % | 50.0 % | 40.0 % | 33.3 % |

Player armor by level (Itemization §3.2, `armor = round(30 × B(ilvl) × slot_weight × material × rarity)`), common pieces at ilvl 4L in the pilot's six armor slots (head 1.0, chest 1.0, hands 0.75, legs 1.0, feet 0.75, back 0.55 always cloth) and the buckler (`off_hand` 0.6 × 2.0, plate):

| L | head | chest | hands | legs | feet | back | leather kit | cloth kit | buckler | leather + buckler vs same-level attacker |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 11 | 11 | 8 | 11 | 8 | 4 | 53 (14.6 pilot) | 35 | 33 (9.1 pilot) | 46.2 % |
| 2 | 13 | 13 | 10 | 13 | 10 | 5 | 64 | 41 | 40 | 34.2 % |
| 5 | 21 | 21 | 16 | 21 | 16 | 7 | 102 | 66 | 63 | 24.8 % |
| 7 | 26 | 26 | 20 | 26 | 20 | 9 | 127 | 81 | 78 | 22.7 % |
| 10 | 34 | 34 | 25 | 34 | 25 | 12 | 164 (45.1 pilot) | 107 | 101 | 20.9 % |
| 15 | 46 | 46 | 35 | 46 | 35 | 16 | 224 | 147 | 139 | 19.5 % |
| 20 | 59 | 59 | 44 | 59 | 44 | 20 | 285 (78.3 pilot) | 187 | 176 | 18.7 % |

The shipped hide set (`hide_cap` 1, `hide_jerkin` 3, `hide_gloves` 1, `hide_leggings` 2, `hide_boots` 1, `hide_cloak` 1 = 9 pilot units ≈ 33 armor) sits below the formula's 53 (§10). Leather kit alone: 20 % at equal level from L10 on, the "Damage 15–25 %" band of Part 1.

### 6.2 Resistance (Part 2)

`ResistMitigation = EffectiveResistance / (EffectiveResistance + 5 × AttackerLevel)`, cap 0.75. Characters: `Resistance(school) = resist_<school> + resist_all + floor(wis/2) + floor(resist_magic/10) (+ floor(con/4) poison, disease)`; starter gear carries no `resist_*`, and `resist_magic` is 0 unless chosen at creation.

| Resistance | attacker L1 | L2 | L5 | L7 | L10 | L15 | L20 |
|---|---|---|---|---|---|---|---|
| 5 | 50.0 % | 33.3 % | 16.7 % | 12.5 % | 9.1 % | 6.2 % | 4.8 % |
| 10 | 66.7 % | 50.0 % | 28.6 % | 22.2 % | 16.7 % | 11.8 % | 9.1 % |
| 15 | 75.0 % (cap) | 60.0 % | 37.5 % | 30.0 % | 23.1 % | 16.7 % | 13.0 % |
| 20 | 75.0 % | 66.7 % | 44.4 % | 36.4 % | 28.6 % | 21.1 % | 16.7 % |
| 30 | 75.0 % | 75.0 % | 54.5 % | 46.2 % | 37.5 % | 28.6 % | 23.1 % |
| 46 | 75.0 % | 75.0 % | 64.8 % | 56.8 % | 47.9 % | 38.0 % | 31.5 % |

Starter resistances (all schools, from Wisdom): Weapon/Guardian/Archery/Shadow 5 at every level (wis 10); Magic 10 → 19 (wis 20 → 38); Healing 20 → 46 (wis 40 → 93). Poison and disease add `floor(con/4)`: Fighter 6 → 12, Guardian 7 → 16. NPC casters are absent from Silverbrook, so resistances only matter against player spells and the Archery/Shadow poisons on PvP targets.

### 6.3 NPC mitigation

`Armor_npc = armor_coef × 100 × L_npc`, `Resistance_npc = resist_coef × 5 × L_npc`, class `medium` 0.54 for all six Silverbrook creatures (default; 35.1 % at equal level). Values in §7.

## 7. Block and absorbs

### 7.1 Shields (Part 1 Block Formula, Part 2 step 6)

```text
BlockChance     = min(0.75, block_rating/(block_rating + K_block(L)) + 0.10 × min(1, shield_defense / max(1, skill_ref(L))) + Σ block_effects)
block_reduction = min(0.40, 0.30 + min(0.10, max(0, str − 10)/4000))
```

Guardian with `iron_buckler` (or any shield) and `shield_defense` on the §2.2 line: block chance **10.0 %** at every level 1–20 (skill ≥ `skill_ref`), `shield_block` adds +0.30 for 6 s → 40 %; `block_reduction` 0.304 (str 25) → 0.310 (str 51), so a blocked hit loses 30–31 %. Block applies to physical frontal (180°) attacks only and rolls after mitigation with `d_block`; a blocked crit reports `crit_block`. NPCs: `guardian` archetype with a shield blocks 5 % at `block_reduction` 0.30; none of the six Silverbrook creatures has a shield (`bCanBlock` false for them). Shields keep their implicit armor (§6.1 buckler column) outside the budget.

### 7.2 Absorbs (Part 2, Part 3 `absorb` operation)

- Amount: `arcane_ward` `amount_modifier` 2.0 scaled like a heal (§5 row; 48 at L1, 107 at L20), all schools, 20 s, one instance per caster (`refresh`); cannot crit.
- Consumption: after mitigation and block (`FAttack.Absorb` is the remaining pool, applied after `DamageTakenMultiplier`), oldest instance first, ties by effect instance ID; a hit fully absorbed reports `absorb` with damage 0, a partial absorb reports the landing outcome with the remainder (minimum 1 unless fully absorbed).
- Accounting: absorbed damage counts as damage dealt (threat, kill credit) and as healing done by the absorb's caster; overkill counts for nothing. The remaining pool replicates to the owner for the HUD shield overlay.
- Bounds: `amount_modifier` ≤ 3.0 and `duration_s` ≤ 30 for character absorbs (Part 3 validator; the Part 5 ward is 2.0 / 20 s). No other starter ability absorbs.

## 8. Greenlands NPC stat lines

Levels are the zone record's level areas (`greenlands_silverbrook.json`: boar 2, bandits 5, wolves 6) with 9dafb6ed's tier offsets (+1 elite, +2 mini boss). Attributes: `attr_ref(L) × archetype weight`, floor 10 (Character §9.1; ±5 % spawn variance omitted). Health `H_ref(L) × T`; swing damage `H_ref(L) × npc_dps_coef × swing_s × √T` (Part 2 creature-tier rule); crit `min(0.20, 0.05 + dex/5000)` at ×1.5; skills `skill_ref(L) + off(tier)` for offense and defense (single combat track); avoidance fixed 0.03 dodge / 0.03 parry in the front arc; mitigation class `medium`; `can_crit` true; `threat_profile` standard.

| NPC (`content/creatures`) | L | Archetype / tier | str dex con int wis wil cha luck | HP | `swing_s` | Swing (pre-mit) | DPS (pre-mit) | Crit | Armor (vs attacker L1 / L5 / L7) | Resist | Offense = defense skill | Range |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `npc/boar` | 2 | beast / common | 31 31 31 10 10 10 10 10 | 489 | 1.6 | 23.5 | 14.7 | 5.6 % | 108 (52 / 18 / 13 %) | 5.4 | 32 | melee 3.0 m |
| `npc/wolf` | 6 | beast / common | 34 34 34 10 11 10 10 11 | 667 | 1.6 | 32.0 | 20.0 | 5.7 % | 324 (76 / 39 / 32 %) | 16.2 | 82 | melee |
| `npc/bandit_thug` | 5 | brute / common | 33 13 33 10 10 20 10 13 | 623 | 2.0 | 37.4 | 18.7 | 5.3 % | 270 (73 / 35 / 28 %) | 13.5 | 71 | melee |
| `npc/bandit_archer` | 5 | skirmisher / common | 13 40 20 10 13 13 10 20 | 623 | 2.0 | 34.9 | 17.4 | 5.8 % | 270 (73 / 35 / 28 %) | 13.5 | 71 | ranged 30 m, physical |
| `npc/wolf_alpha` | 7 | beast / elite | 35 35 35 10 11 10 10 11 | 1,779 | 1.6 | 54.0 | 33.7 | 5.7 % | 378 (79 / 43 / 35 %) | 18.9 | 144 | melee |
| `npc/bandit_captain` | 7 | champion / mini_boss | 35 14 35 10 10 21 10 14 | 3,558 | 2.0 | 95.5 | 47.7 | 5.3 % | 378 (79 / 43 / 35 %) | 18.9 | 194 | melee |

Miss chance of a starter (`S_hit` 105) against them: 0 for the four commons, 1.9 % vs the alpha, 4.5 % vs the captain. NPC miss against a starter's defense skill 100: boar 3.4 %, wolf 0.9 %, bandits 1.5 %, alpha and captain 0. Tier CC immunity: alpha `charm`; captain `stun`, `fear`, `charm`, `incapacitate` (Part 2). Kill XP multiplier 1 / 2.5 / 5. Pack composition and AI stay in the zone record and AI spec.

Fight check (reference starter, common 4L gear, auto + filler, expected crit, NPC frontal avoidance 6 %): Fighter L2 vs boar: 36 s, ends at 27 % health; Guardian L2 vs boar: 36 s, 46 %; Mage L2 kills the boar in 30 s but at 275 health does not survive it; Archer L2 needs 77 s (Power 20) and dies; Fighter L5 vs thug 40 s / 7 %; L6 vs wolf 42 s / 1 %; alpha and captain beat any solo starter (as Part 1 intends for elites) and need 3–5 players. Part 1's target for a same-level common is 20–30 s with 35–45 % left, assuming the reference's rare two-hand kit; see §10.

## 9. Resolution inputs: where each `FAttack` field comes from

| `FAttack` field | Source once snapshots exist | Fallback today |
|---|---|---|
| `RawDamage`, `SpreadWeaponDamage`, `bHasSpread`, `SpreadMinimum/Maximum` | §5 formula from the attacker's equipment snapshot (`weapon_dps`, `speed_s`), §2 Power, §1 `σ(L)`, §2.2 `skill_mult`; 0.85 / 1.15 | pilot 20 / 14 × dev units (§3.2) |
| `bSpell`, `bPhysical` | ability `school` (§5) | physical |
| `bFrontArc` | existing arc test, `front_arc_deg` 180 | same |
| `bCanMiss`, `MissChance` | §2.2 vs §8 defense skill (spells vs `resist_magic`, NPC 0.75 × offense) | off |
| `bCanDodge/Parry`, `DodgeChance`, `ParryChance` | NPC 0.03 / 0.03 (§8); players ratings/(ratings + K) = 0 on starter gear | boar values |
| `bCanCrit`, `CritChance`, `CritDamage` | players §2 crit, 2.0; NPCs §8 crit, 1.5 (`tune.combat.npc_crit_damage`) | off |
| `bHasArmorSnapshot`, `Armor`, `ArmorPenetrationFraction` | §6.1 equipment armor; NPC `0.54 × 100 × L`; penetration 0 | `PilotFlatArmor` |
| `bHasResistanceSnapshot`, `Resistance`, `MagicPenetrationFraction` | §6.2 | off |
| `AttackerLevel` | synced level (players), spawn level (NPCs, §8) | 0 → mitigation skipped |
| `bCanBlock`, `BlockChance`, `BlockReduction` | §7.1 when a shield is equipped | off |
| `DamageTakenMultiplier` | product of `damage_taken_mod` effects, clamp [0.10, 2.0] (`defensive_stance` −10 %, `shield_wall` −30 %, `bulwark` −20 %) | 1 |
| `Absorb` | remaining pool of §7.2 instances | 0 |

Health and resources enter through the Part 1 formulas on the same snapshot (§2); NPC health is §8. The typed caps and divisors already adopted at zone start by `USFCombatPilotSettings` (`AvoidanceCap`, `MissCap`, `CritCap`, `CritDamageCap`, `BlockCap`, `BlockReductionCap`, `ArmorKLevel`, `ArmorReductionCap`, `ResistK`, `ResistReductionCap`, penetration caps) are the Part 1–2 values and need no change.

## 10. Controls

Formulas, curves and the ability/NPC/item content are release-bound (Tuning Architecture §2); the adjustable scalars are declared here. Scope realm, approval tier "balance", client visibility "tooltip" unless noted. Existing Part 1–2 keys are restated only where they gain bounds.

| Key | Label | Type / unit | Default | Bounds | Step | Apply | Notes |
|---|---|---|---|---|---|---|---|
| `tune.combat.crit_base` | Base critical chance | fraction | 0.05 | 0–0.15 | 0.005 | immediate | Part 1 |
| `tune.combat.block_reduction` | Block damage removed | fraction | 0.30 | 0.10–0.40 | 0.01 | immediate | ≤ `block_reduction_max` 0.40 |
| `tune.combat.skill_damage_floor` | Damage at skill 0 | fraction | 0.80 | 0.50–1.00 | 0.01 | immediate | Part 1 |
| `tune.combat.miss_divisor` | Skill points per 1 % miss | points | 2,000 | 500–10,000 | 100 | immediate | Part 1 |
| `tune.combat.power_speed_ref_s` | Power reference time | s | 2.4 | 1.5–4.0 | 0.1 | restart | changes every ability tooltip |
| `tune.combat.unarmed_dps_factor` | Unarmed damage | fraction of `WEAPON_DPS_K × B(4L)` | 0.40 | 0.10–1.00 | 0.05 | immediate | Part 2 |
| `tune.combat.throughput_scale` | σ(L) table | per-level table | §1 | each entry 0.10–1.00 | — | restart | republished by `lsf-combatmath` only |
| `tune.combat.npc_dps_coef.<archetype>` | NPC auto-attack damage | fraction of `H_ref` /s | beast/brute/champion 0.030, skirmisher 0.028 | 0.005–0.080 | 0.001 | next spawn | Part 2 |
| `tune.combat.npc_swing_s.<archetype>` (new) | NPC swing time | s | beast 1.6, others 2.0 | 1.0–3.5 | 0.1 | next spawn | definitions may override |
| `tune.combat.npc_crit_damage` (new) | NPC critical multiplier | multiplier | 1.5 | 1.0–2.0 | 0.05 | immediate | Part 2 `NPC_CRIT_DAMAGE` |
| `tune.combat.npc_crit_cap` (new) | NPC critical chance cap | fraction | 0.20 | 0.05–0.40 | 0.01 | immediate | Part 2 |
| `tune.combat.npc_dodge`, `tune.combat.npc_parry` (new) | NPC frontal dodge / parry | fraction | 0.03 / 0.03 | 0–0.15 | 0.005 | next spawn | sum ≤ `avoidance_cap`; `BoarDodgeChance`/`BoarParryChance` adopt these |
| `tune.combat.npc_block_guardian` (new) | Shielded NPC block | fraction | 0.05 | 0–0.30 | 0.01 | next spawn | `guardian` only |
| `tune.combat.npc_armor_coef.<class>`, `npc_resist_coef.<class>` | NPC mitigation class | coefficient | light 0.25, medium 0.54, heavy 1.00 | 0.05–2.00 | 0.01 | next spawn | Part 2 |
| `tune.combat.tier_health_mult.<tier>` | Tier health | multiplier | 1 / 2.5 / 5 / 20 | 1–40 | 0.5 | next spawn | Part 2 |
| `tune.item.weapon_spread` | Weapon damage spread | fraction | 0.15 | 0–0.30 | 0.01 | restart | §4 |
| `tune.item.weapon_dps_k`, `tune.item.armor_k` | Weapon damage / armor scale | points per budget | 4.0 / 30 | 1–10 / 5–100 | 0.1 / 1 | restart | Itemization §3 |
| `tune.progression.npc_attr_curve` | NPC attribute curve | formula parameters (120, 460, 1.1) | — | 100–200, 300–600, 0.8–1.5 | — | next spawn | Character §9.1 |
| `tune.combat.dev.pilot_damage_unit`, `tune.combat.dev.pilot_armor_unit` (new, development fixture) | Pilot unit conversion | avg_hit / armor per pilot point | 1.2133 / 3.64 | 0.5–5 / 1–10 | 0.001 / 0.01 | restart | removed when `SFGear` carries design stats; never a player feature |

Content-bound values and their validator bounds: ability `power.modifier` (Part 3 bounds, §5), `speed_s` 1.4–3.8 (§3.1), creation shares (sum 1, no attribute above +30 at creation, §2.1), archetype attribute weights (sum 1, Character §9.1), NPC `level` inside the zone's level areas ±2, `armor_class`/`resist_class` enum, `npc_tier` enum, absorb `amount_modifier` ≤ 3.0 / `duration_s` ≤ 30 (§7.2).

## 11. Owner checks

1. **Starter gear rarity.** The reference build assumes a rare two-hand kit at ilvl 4L; starters hold common one-hand gear, so a Fighter L2 kills a boar in 36 s and ends at 27 % (target 20–30 s, 35–45 %), and a Fighter L6 barely survives a wolf. Accept the harsher start, give starter kits rare-equivalent weapons, or raise `npc_dps_coef` gaps by level?
2. **Archery and Shadow power.** `PhysicalPower = 2 × str` leaves dex kits with Power 20 (Archer DPS ≈ 45 % of the Fighter, 77 s per boar). Add Dexterity to Power for `ranged` and `daggers` abilities (e.g. `PhysicalPower = 2 × max(str, dex)` for those skills), or re-weight their creation shares toward Strength?
3. **Weapon speeds.** One-hand swords, maces and the captain's longsword at 2.0 s (implied by 9dafb6ed's pilot numbers) conflict with the Item Model example `iron_longsword` 2.6 s; bow 2.8 s and staff 3.4 s are chosen. Confirm or re-derive the pilot damages.
4. **Guardian and Shadow creation allocations** (str 25 / con 30 / wil 15; dex 40 / str 20) are not in Part 5.
5. **Growth folding.** All growth follows creation shares along the Character median line (`N_ref`), so Part 5's L10/L20 health lines (no growth) are superseded. Keep, or model level points and use-based gains separately?
6. **Skill line.** Kit skill 100 + tactics 50 at creation, then `skill_ref(L)`; a starter never misses a same-level common. Agreed as the fallback snapshot?
7. **Low-level armor sensitivity.** The attacker-level divisor makes a L1 attacker meet 52 % boar armor against 35 % at L2. Accept, or floor `AttackerLevel` at the zone's content level for players below it?
8. **Hide starter set** (9 pilot units ≈ 33 armor) is below the formula's 53 for common ilvl 4 leather. Re-grade the six hide pieces to the formula (14.6 pilot units) or keep?
9. **Switch-over.** The pilot fight (80 / 60 health, 20 / 14 damage) lasts about 3 s; the design fight lasts about 36 s. When do the §9 snapshots replace the pilot values: with the Progression stat snapshot, or now with the §2 fallback table?
10. **Archetype mapping** boar/wolf/alpha `beast`, thug `brute`, archer `skirmisher` (ranged 30 m physical bolt at the brute swing time), captain `champion`; elite/mini-boss levels 7 (zone area + offsets). Agreed?

## Change log

- 0.1 (2026-10-25): first draft for the shared combat resolution path: level constants, six archetype stat lines 1–20, weapon lines consistent with 9dafb6ed, 38-ability coefficient table, spread, armor/resistance curves, shield block, absorbs, six Silverbrook NPC lines, `FAttack` input map, controls, owner checks.
