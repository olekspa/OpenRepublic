# Authoritative utility networks - 22 October 2026

Added an independent utility network to the same C++ economy used by human
controls and external agents. Utilities have no warehouse inventory or truck
cargo. Explicit resource-specific building connections determine where finite
per-tick generator output can satisfy demand. This addresses the earlier imported
industrial snapshot's missing utility input without removing consumption.

Generators check fuel and all material output space before committing and offering
supply. Consumers execute in stable building/recipe order, requiring complete
utility demand, material inputs and output space before any consumption. Two-limb
budgets preserve aggregate supply above uint64. Successful generators always burn
declared fuel; surplus power is discarded. Utility production is serial; ordinary
material-only worlds retain the established production path.

Utility-bearing recipes require interval one; generators cannot consume utilities.
Idealized links have no capacity/loss/voltage/distance/battery/startup-power model.
The native catalog's `utility <name>` declaration uses authored integer units.
These rules do not claim physical wattage or original-game utility parity.

Typed/JSONL APIs expose connect/disconnect, discovery, resource kinds, full and
bounded revision-checked connection snapshots. The desktop provides a separate
utility menu and connection actions for exact source/selected building IDs.
Native scenes draw colored elevated connectivity links, without claiming flow.
Imported regions accept repeatable --utility-resource names, retaining declared
rate conversion while omitting utility storage. No supply or topology is invented.
Lua tuning now preserves resource kinds and rejects unsupported utility intervals.

Native v4 persists canonical links, retained utility command identity and stall
receipts. Utility-bearing recipes and utility state/history select v4, including
after the last link is removed. Invalid pending commands restore for normal
tick-boundary rejection; impossible nonzero utility recipe progress rejects.
Legacy material-only v2/v3 bytes are preserved; checkpoint wrapper v1 remains.

VERIFIED - full Windows Release client/headless builds succeed. Final concurrent
CTest passes **64/64 client**, **63/63 headless**. Tests cover fuel and coproduct
gates, disconnected/atomic multiutility supply, deterministic shortage priority,
above-64-bit budgets, storage/haul rejection, catalog kinds and Lua tuning,
permissions/revisions/paging, malformed and pending saves, history and replay.
Independent review repaired version/history/receipt selection, saturation and
impossible utility progress; focused re-review found no remaining blocker.

VERIFIED - separate-process authored fuel -> generator -> powered mine -> powered
processor -> depot chain, three material hauls and two utility links, delivers40
iron by tick65. Disconnection causes a power shortage. Checkpoint restoration
matches16 subsequent ticks. The helper optionally writes a no-overwrite checkpoint
for desktop use. All scenario content is independently authored.

OBSERVED - authorized Windows GUI: restored tick83 world shows five buildings,
three hauls, two links and the utility controls. Pick generatorOR2 as source and
mineOR3 as destination; Disconnect utility then Step1 yields tick84, one applied,
zero rejected, one link and mine missing_input. Reconnect then Step1 yields tick85,
two links and mine output_full. VERIFIED actual pipe vs fresh headless reference:
hash2805840414191079416 at84, hash16698369674331021470 at85. Pipe explanations name
missing power then full ore storage. Task-owned client closed.

Validation exposed shared temporary test paths colliding between build suites;
file tests now create exclusive unique fixture directories. Early argument errors
could print stderr then hang after starting asynchronous engine logging. Source
review identified an INFERRED logger/CRT shutdown race. Pre-engine errors now use
stderr only. The reproduced task process was stopped; final startup tests pass
with the original five-second timeout. No dependency source was changed.

No original game was launched or original input modified. Real-content generation
and delivery, original utility topology, workforce, physical units and gameplay
parity remain UNKNOWN. Next: evidence-backed topology/generator integration and
network shortage diagnostics. The global goal remains active and incomplete.

See [model, API and scenario usage](UTILITY_NETWORKS.md).
