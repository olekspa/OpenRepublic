# Synthetic border trade

Border trade is an OpenRepublic-authored economy feature. It is a deterministic
way to exchange catalog-declared resources at one designated trade post and keep
the money balance in the authoritative world. It does not model an original-game
market, price formula, trade route, currency, or border process. WRSR trade parity
is **UNKNOWN**.

## Catalog declarations

Trade prices are optional resource declarations in the catalog text:

```text
resource grain
trade grain buy 5 sell 3
building border_station 1 1
  store grain 100
  tradepost
end
```

`buy` is the price the world treasury pays per unit imported into a station;
`sell` is the price the treasury receives per unit exported from a station.
Prices and resource quantities are positive integer units. Each resource can be
declared once, and utilities cannot be traded. `tradepost` marks a building type
as an eligible station. Catalogs without trade declarations keep their existing
canonical text and state-hash behavior.

## Tick commands

The typed `EconomyApi` and protocol-v0 JSONL host expose `trade_export` and
`trade_import`. Requests include the expected revision, exact building ID,
resource name, requested amount, and a positive per-command `rateCap`. A command
is queued after controller permission and revision checks. At the next tick it
can move at most `min(amount, rateCap)` units, subject to station designation,
storage, stock or treasury funds, and integer overflow checks. It either applies
the bounded exchange atomically or rejects without moving inventory or money.
Several commands in a tick execute by command ID; earlier accepted exchanges
therefore affect later stock, storage, and treasury checks deterministically.

Export removes stock and credits `quantity * sellPrice`; import adds stock and
debits `quantity * buyPrice`. The world treasury starts at zero. Thus an authored
scenario can export a resource to earn funds before importing a priced resource.
No automatic exchange, recurring order, hauling, or conversion between money
and resources occurs. Observer clients may inspect catalog prices and treasury;
only controller clients may queue trade commands.

Trade state is persisted in native OpenRepublic world saves and catalog-bound
checkpoints. Legacy saves remain loadable. The treasury participates in the
state hash only when the active catalog declares at least one trade price, so
existing no-trade hashes remain stable. This persistence is not WRSR save
compatibility.

## Fixture

[`data/scenarios/grain_export_for_power.json`](../data/scenarios/grain_export_for_power.json)
defines an OpenRepublic synthetic grain-producing trade post with priced grain
and coal storage. Use the host to place the building, step once to generate grain,
submit a grain export at the current revision, and step once to inspect the world
treasury and command result. The fixture is a setup description; scenario-score
tooling does not yet automate trade commands.

**VERIFIED** describes only this OpenRepublic implementation and its tests.
**UNKNOWN**: original-game trade rules, prices, station constraints, starting
money, and any parity with WRSR.
