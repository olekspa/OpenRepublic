# LLM republic controller

`tools/llm_republic.py` connects a bounded language-model planner to the standalone typed economy host. OpenRepublic C++ remains authoritative: the model receives a compact observation and proposes JSON actions; Python validates exact fields, catalog names, IDs and coordinate limits, submits commands, steps the host, polls outcomes, and provides rejection reasons in the next round. The model has no shell or host tool access.

## Run

```powershell
python tools/llm_republic.py --host build/headless/src/economy_host/Release/OpenRepublicEconomyHost.exe --backend codex --rounds 8 --ticks 500 --transcript C:\temp\llm-rounds.jsonl -- --region <region> --content-root <content-root> --radius 450
```

The Codex backend runs `codex exec -s read-only -m gpt-5.6-luna --skip-git-repo-check --ephemeral -` in a temporary directory outside the repository. `openai-compatible` uses `OPENAI_COMPATIBLE_BASE_URL`, `OPENAI_COMPATIBLE_MODEL`, and optional `OPENAI_COMPATIBLE_API_KEY` with Python `urllib`; defaults target an LM Studio-style localhost endpoint. `claude` uses `claude -p` when installed. Model errors or invalid JSON produce a logged empty round; `--fallback-advise` enables deterministic haul suggestions for that failed round. Supply `--prompt <file>` to append tournament-specific planning guidance. `--backend scripted --scripted-responses <json-array>` provides a deterministic fake backend for local tests without a model.

Allowed commands are place_building, remove_building, build_road, set_building_enabled, set_road_enabled, create_haul_order (batch submitted through the host's `create_haul_orders`), cancel_haul_order, retarget_haul_order, connect_utility, and disconnect_utility. At most 24 actions are considered per round. Utility links are accepted locally only for catalog resources marked `utility: true`; utilities cannot be hauled. The host independently checks its revision and command semantics. Coordinates are limited to ±100000 integer cells; roads must be a single orthogonal segment.

Each round's bounded observation includes stalled buildings, recipe flow references and per-building `advise` suggestions; inventories; haul-order state counts and resource flow summaries; `advise` unserved-input, surplus and retarget suggestions; last command outcomes; and current score, score change since start and change since the prior round. The system prompt asks the model to act whenever stalls or blocked orders remain, reconsidering an old failure against the refreshed world. The transcript stores an observation SHA-256, validated actions, outcomes/reasons, score, notes and tick. Keep region transcripts outside Git because notes/actions can expose private map information.

## Self-test and replay

```powershell
python tools/llm_republic.py --self-test
python tools/llm_republic.py --host <host.exe> --replay C:\temp\llm-rounds.jsonl --rounds 8 --ticks 500 -- --region <same-region> --content-root <same-root> --radius 450
```

Replay applies recorded validated actions against a fresh host started with matching initial inputs. It is deterministic only to the extent that the host's starting world, catalog, command ordering and step sizes match; compare per-round hashes/scores to inspect divergence. Source/API behavior is VERIFIED by local checks; no WRSR formula, parity, or original-game outcome follows from this controller. The score is the autopilot-compatible `stored_non_raw_units` metric (electricity excluded).

## Multiple republics and tournaments

The current economy host exposes one world per process and has no republic-owner field or owner-scoped commands. Multi-republic operation therefore starts N independent controller processes against N separate fresh hosts. Give each world entry the same region/seed inputs when comparing controllers; do not point concurrent writers at a shared mutable checkpoint.

Create a JSON manifest:

```json
{
  "worlds": [{"name": "N22-r450", "host_args": ["--region", "<region>", "--content-root", "<content-root>", "--radius", "450"]}],
  "controllers": [
    {"name": "autopilot", "kind": "autopilot"},
    {"name": "codex-default", "kind": "llm", "backend": "codex"},
    {"name": "codex-persistent-guidance", "kind": "llm", "backend": "codex", "prompt": "tools/prompts/llm_republic.md"}
  ]
}
```

Run `python tools/republic_tournament.py --host <OpenRepublicEconomyHost.exe> --manifest <matrix.json> --rounds 8 --ticks 500 --parallel 1 --transcript-dir <private scratch dir> --output-json <results.json>`. The script gives each world/controller pair a new standalone host, prints a score/idle/gain table, and emits JSON results. Increase `--parallel` to run separate republics concurrently; use modest parallelism for rate-limited models. LLM runs get an additional fresh-host idle baseline stepped to the same final tick. Autopilot's own `--json` result includes its matched idle baseline.

## First real run (2026-10-23, N22 region, radius 450, 8 rounds x 500 ticks)
## N22 experiment results (2026-10-23)

Transcript inspection of the initial Codex run showed the model stopped because its notes concluded no “safe” actions remained after duplicate-road and cargo-in-transit cancellation rejections, despite continued stalls and waiting orders. It had also proposed utility links for `eletric`, which is not cataloged as a utility. The controller now rejects non-utility links before submission, maps advice to each stalled building, summarizes order states and resource routes, reports score trend, and exposes the economy's exact service-access cells, nearest road endpoints and road components. The prompt directs the model to act while bottlenecks remain and to use refreshed route evidence instead of repeating failed commands.

All runs used the N22 region with radius 450, Codex `gpt-5.6-luna`, eight rounds of 500 ticks, and the same autopilot-compatible stored non-raw score (electricity excluded). Each run started a fresh host over the read-only region input. The two improved runs used the same prompt file and had no backend errors or empty action rounds.

| Run | Final tick | Final score | Idle score | Difference from autopilot |
|---|---:|---:|---:|---:|
| Original Codex run | 4012 | 632.250239 | 0.0 | -98.124761 |
| Improved run A | 4012 | 771.958573 | 0.0 | +41.583334 (+5.7%) |
| Improved run B | 4012 | 782.333575 | 0.0 | +51.958336 (+7.1%) |
| Rule-based autopilot | 4017 | 730.375239 | 0.0 | baseline |

Both improved runs acted in all eight rounds. Run A had three rejected commands (one unavailable placement access cell and two duplicate haul orders); run B had three duplicate haul-order rejections. This small two-run sample suggests the controller can outperform the baseline on this specific world and metric, but model variance and the five-tick difference from autopilot mean it is not a broad performance or WRSR parity claim. Region imports and economy behavior remain experimental OpenRepublic evidence.

## Scenario mode

Pass `--scenario <id|path>` to seed a shipped scenario by exact ID/filename stem
or an explicit existing JSON path. The setup consumes one tick, followed by the
requested `rounds × ticks` controller ticks. A final `scenarioScore` JSON record
uses the same deterministic integer `stored_non_raw_units` metric as
`tools/scenario_score.py`.

Add `--scenario <id|path>` to `republic_tournament.py` for scenario comparisons.
Each manifest controller receives a fresh fixture host and the table includes a
separate rule-based autopilot baseline. Scenario rows share the fixture score
metric; actual final ticks are shown because autopilot placement attempts may
advance extra ticks. Manifest world `host_args` are rejected in this mode.
