# MKT-01 — Fixed-price market for basic goods (DEC-06)

Status: DEC-06 accepted by the owner 2026-10-22 ([owner decisions](OWNER-2026-10-22-decisions.md)): a restricted fixed-price market, basic goods only (raw materials and simple crafted goods: logs, ore, ingots, planks), seller-set price, sold in a settlement market such as Silverbrook; gear and rare items later. Not implemented as a player feature. One engineering session. Backlog row [MKT-01](../Game-Design/Legends_Shattered_Frontier_Gameplay_Feature_Backlog_v1.0.md); design [Markets, Trade and Vendors](../Game-Design/Economy/Legends_Shattered_Frontier_Economy_Markets_Trade_and_Vendors_v1.0.md) (scopes, fees, price cap).

## What exists (build on, do not fork)

| Piece | Today | This spec |
| --- | --- | --- |
| [`BackendServices/Exchange`](../../BackendServices/Exchange/README.md) + `Exchange.Postgres`: wallets, whole-stack fixed-price listings, escrow through `InventoryLedger`, `ExchangeControls` fees/caps/limits, expiry, durable listing intents ([evidence](../evidence/gameplay-host-exchange-followup.md), [durable](../evidence/exchange-durable.md)) | Eligibility facts are caller-supplied; `market_id` defaults to `market/global`; min level 10 | Eligibility comes from a market content file; one settlement market |
| `Gameplay.Host` Exchange gRPC (`ListItem`, `BuyListing`, `CancelListing`, `ListListings`, `Wallet`) | Passes `MarketId` through | Validates the market and the item against content |
| UE `Exchange/SFExchangeComponent` (server RPC intents, backpack FIFO, client results), `SFExchangeModel` view-models, `USFMarketBoard`/`USFTradeWindow` widgets on the kit, `-ExchangeUiDemo` (F7) with `SFFakeExchange` | Demo data only | Real service, opened at a market board in the world |
| `tools/Test-ExchangeTrade.ps1` + `Tests/SFExchangeNetworkScenario.cpp` (two clients, exactly-once DB check) | Trade + listing wire | Adds the market leg |
| Market Square: `silverbrook-town-layout.json` key 5 plaza at `[0,-30]` 60x50, stalls and vendors from `market_vendors` | Dressing | One market board actor |

## Player-facing behaviour

1. **Selling.** Tam has 12 Rough Timber. At the market board on Silverbrook's Market Square he presses E, opens Sell, picks the timber stack, types quantity 10 and price 8 copper each. The panel shows "Listing fee 1 copper, you receive 76 copper if sold (5% sale fee)". He confirms; the 10 timber leave his bag and the listing shows under My Listings for 48 hours.
2. **Buying.** Mara opens the same board, filters "timber", sees "Rough Timber × 10, 8c each, 80c total, Tam, 47h". She buys; 80 copper leave her purse, the timber lands in her bag, and Tam's purse gains 76 copper whether he is online or not.
3. **Cancel and expiry.** Tam cancels an unsold listing; the timber returns to his bag. A listing nobody bought after 48 hours returns the same way, minus the expiry fee.
4. **Refused goods.** An Iron Axe or a quest item cannot be listed: the Sell list simply does not offer them, and the server refuses if a client tries.
5. **Full bag.** A buy with no bag space is refused before any coin moves ("Make room for 10 Rough Timber").
6. **Price guard.** A price far above the item's reference value is refused with the cap shown; a price so low the fee would eat it is refused too.

## Scope and conflicts resolved

- **Basic goods** = the allowlist in `content/markets/silverbrook.json` (below): `item/oak_log`, `item/iron_ore`, `item/rough_stone`, `item/boar_hide`, `item/rough_timber`, `item/iron_ingot`, `item/iron_nails`, `item/hide_strips`, `item/worked_timber`, `item/iron_brackets`, recipe scrolls, and every CRF-01 grade of the crafted ones. Bundles, tools, gear, quest and bound items are out until a later decision.
- **Settlement market vs design scopes.** The design routes commodities to one realm-wide `global_exchange` book reachable from any hub; the owner chose a settlement market. MVP: every listing carries `market_id = market/<settlement>`, is visible only at that market's board, and the Exchange keeps `MarketScope.GlobalExchange` rules for commodities (fees, whole-stack). One open market (Silverbrook) makes the two identical; a second settlement reopens the question (Owner question 2). No new scope enum.
- **Whole-stack purchase** stays (existing implementation); partial fills are a later change.
- **`market_min_level` default 10 → 1** (bounds unchanged). Alpha characters have no level gate worth enforcing.
- **Fair value** for the price cap comes from the content file, not a sales index.
- **Wallets.** Gold exists only in the Exchange wallet; it reaches players through the DEC-02 Alpha Starter Kit coin line, quest gold (QST-02 job 8) and sales. Owner question 1 fixes the first.

## Content

`content/markets/silverbrook.json` (validator in `Exchange.Tool`, loaded by Gameplay.Host from `GAMEPLAY_CONTENT_ROOT/markets/`):

```json
{ "id": "market/silverbrook", "zone_id": "zone/greenlands_silverbrook", "board": "market_board/silverbrook", "open": true,
  "items": [ { "definition_id": "item/rough_timber", "fair_value_copper": 6, "grades": true },
             { "definition_id": "item/oak_log",      "fair_value_copper": 3 } ] }
```

- `grades: true` admits `item/<slug>/<grade>` ids with the same fair value times the grade multiplier (`crude` 0.5, `common` 1, `fine` 1.5, `superior` 2.5, `masterwork` 4; versioned content constants, not tuning).
- The host builds `MarketEligibility` from it: `Tradeable` = listed, `Bound/QuestItem/Token` false for listed ids, `Category` "material", `HubAttested` = market `open`, `SameRealm` true (one realm), `SellerLevel` from the character snapshot, `FairValueCopper` from the row. Unknown `market_id` or unlisted item → `invalid_listing` before any ledger call.

## Typed configuration

Backend `ExchangeControls` (existing keys, permission `economy.tuning.publish`, effective for new listings): listing fee 1% (0..10%), regional 2%, sale fee 5% (0..20%), expiry fee 1%, minimum fee 100 copper (0..10000), active listings per character 100 (1..100), duration 48 h (12..48), **min level default 1** (1..100), price cap 20× fair value (1..50×) and 100 000 copper absolute floor (10 000..10 000 000). One new control: `tune.economy.market_max_quantity_per_listing` 100 (1..1000) units.

UE `USFGameplayServicesSettings` (release authors, next world start): `bExchangeEnabled` (on for Silverbrook worlds), `MarketReachM` 3.0 (1.0..10.0 m), `MarketPageSize` 50 (10..200 listings), `MarketRefreshSeconds` 5.0 (1.0..60.0 s, board re-fetch while open). All per [tuning architecture](../Game-Design/Legends_Shattered_Frontier_Tuning_and_Configuration_Architecture_v0.1.md) §3; no console price or fee expressions.

## Server authority and durable owner

Per [DEC-02](DEC-02-durable-actions.md): domain-local transactions, replay by operation id, **InventoryLedger is the only item writer**; Exchange owns wallets and listings; the zone server owns intent validation only.

- **Zone server** (`USFExchangeComponent::ServerIntent`): the intent carries `MarketId` from the board actor the player opened; the server checks the actor exists, matches, and is within `MarketReachM`; the client never supplies identity, escrow, fee or fair value (existing rule). Requests go through the backpack FIFO (`SFExchangeRequests` deterministic operation ids).
- **List**: Gameplay.Host `ListItem` → durable listing intent → ledger `Transfer` backpack → `<backpack>:escrow` → listing active; listing fee debited from the wallet. Crash between the two → reconciliation fences or tombstones the transfer (existing, evidenced).
- **Buy**: `BuyListing` with `expected_unit_price` and revision → wallet debit buyer, credit seller net of sale fee, `DeliverEscrowBatch` to the buyer's backpack in one Exchange settlement with the ledger hook; capacity checked first, so a full bag refuses before coin moves. Concurrent buyers: one wins (existing fixture).
- **Cancel / expiry**: escrow returns to the seller's backpack; expiry fee from the wallet; the host sweep ages expired listings (existing recovery loop).
- **Offline seller**: proceeds land in the wallet row; nothing waits on the seller's session.
- Receipts are kept; replay returns the same receipt; a changed payload under the same id is rejected (INT-02).

## UI surfaces

[menus.md](../blueprints/menus.md), [ui-hud.md](../blueprints/ui-hud.md); concept `docs/Game-Design/concept-art/UI/v5/market-board.png` (screen table 4 in `IMPLEMENTATION-FIX.md`).

| Surface | Behaviour |
| --- | --- |
| `USFMarketBoard` (E at the board; F7 demo stays dev-only) | Tabs Buy / Sell / My Listings. Buy: search field, sort (price, name), rows item, grade, quantity, unit and total price, seller, time left, Buy with hold-confirm. Sell: backpack stacks filtered to the allowlist, quantity and unit-price fields (`SFGold` digit-only), fee and net preview from the replicated controls snapshot, List. My Listings: Cancel. Bound to `FSFExchangeClientService` through `FSFMarketViewModel`; refresh every `MarketRefreshSeconds` and after each result |
| Wallet | Purse amount in the board header and the inventory panel footer (`gameplay-hud.png` coin style) |
| Toasts | "Sold: Rough Timber × 10, +76c" on next login or live; refusals with the reason (`price_above_cap`, `bag_full`, `not_a_market_good`) |

## World asset: the market board

[props-and-dressing.md](../blueprints/props-and-dressing.md) §A/§B; [buildings.md](../blueprints/buildings.md) §1 import facts.

- Actor `ASFMarketBoard` (`Exchange/SFMarketBoard.{h,cpp}`): `MarketId`, interact prompt "Trade", replicated; alpha mesh `SM_SilverbrookPropNoticeBoard` with the "MARKET" sign variant (`sheet.add_variants`, same package); a dedicated `PropMarketBoard` sheet is a later polish item.
- Placement is content: `silverbrook-town-layout.json` gains `"market_boards": [ { "id": "market_board/silverbrook", "market": "market/silverbrook", "at": [0, -8], "facing": [0, -30] } ]` (south edge of Market Square, among the stalls, facing the square; the Quest Board of [QST-02](QST-02-durable-quests-and-quest-board.md) stands on the north edge). `build_silverbrook.py` spawns it after the stalls and registers it in `_obstacles`; `place_props.py` excludes it from the decorative board cap. Capture the `market` view; fresh reviewer per `docs/visual-review.md`.

## Acceptance tests

1. **Allowlist.** Listing `item/iron_axe` or a quest item → `invalid_listing` at the host, nothing escrowed; listing `item/rough_timber/fine` accepted with fair value 9.
2. **List–buy–deliver.** Two clients: A lists 10 timber at 8c; B buys; A wallet +76, B wallet −80, B backpack +10, A escrow 0; DB shows one settlement (`tools/Test-ExchangeTrade.ps1 -Market`).
3. **Concurrent buyers.** Two buyers, one listing → one sale, the other `listing_gone`; no double debit (host fixture, existing case re-run with market ids).
4. **Wrong market / reach.** Intent with `market/eastford` or from 6 m → refused by the zone server before any RPC; a client-forged `MarketId` for a board it is not near → refused.
5. **Price guards.** 20× fair value + 1 → `price_above_cap`; 1 copper total → `price_below_fee`; quantity 101 → `quantity_above_limit`.
6. **Full bag.** Buyer with a full backpack → `bag_full`, wallets unchanged.
7. **Cancel / expiry.** Cancel returns escrow; forced expiry returns escrow minus the expiry fee; both idempotent on replay.
8. **Crash safety.** Host kill after listing escrow transfer and after buy settlement, immediate PostgreSQL restart → one listing / one sale (existing `Test-GameplayHosts.ps1` exchange leg with market ids).
9. **Offline seller.** A sells, logs out; B buys; A logs in → wallet shows the proceeds, toast once.
10. **Visual.** Board captures (Buy, Sell, My Listings) vs `market-board.png`, fresh reviewer; `market` town capture with the board placed passes the sight-line assert.

## Jobs (in order)

| # | Job | Size |
| --- | --- | --- |
| 1 | Content: `content/markets/silverbrook.json`, grade multipliers, `Exchange.Tool` validator, scroll/grade ids from CRF-01 | S |
| 2 | Backend: market registry in Gameplay.Host, eligibility from content, `market_min_level` default 1, `max_quantity_per_listing` control, refusal codes | M |
| 3 | UE server: `MarketId` on `FSFExchangeIntent`, board proximity check, `bExchangeEnabled` for Silverbrook | M |
| 4 | World: `ASFMarketBoard`, layout row, `build_silverbrook.py` spawn, sign variant | S |
| 5 | UI: wire `USFMarketBoard` to the real service, Sell tab from the backpack, My Listings, wallet in HUD/inventory, toasts | M |
| 6 | Tests and runners: `ShatteredFrontier.Exchange.*` additions, `Test-ExchangeTrade.ps1 -Market`, GameplayHosts market leg, captures + fresh review | M |
| 7 | `docs/dependency-map.md` rows; DEC-06 register row already points here | S |

## Owner questions

1. **First coins.** How does a new character get coins to buy with? (a) The Alpha Starter Kit includes a purse (for example 5 silver = 500 copper). (b) Only by selling goods on the market or finishing quests. (c) An NPC buys basic goods at a fixed low price as well. Spec assumes (a).
2. **A second town's market.** When another town opens a market: (a) each town has its own list, goods must be carried between them; (b) all towns share one list; (c) raw materials are shared, crafted goods stay local. Spec assumes (a).
3. **Fees in alpha.** The design charges a 1% listing fee and a 5% sale fee. (a) Keep them so the economy is tested as designed. (b) Waive all fees during alpha. Spec assumes (a).
