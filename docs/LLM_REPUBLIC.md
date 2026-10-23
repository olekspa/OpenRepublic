# LLM republic controller

`tools/llm_republic.py` connects a bounded language-model planner to the standalone typed economy host. OpenRepublic C++ remains authoritative: the model receives a compact observation and proposes JSON actions; Python validates exact fields, catalog names, IDs and coordinate limits, submits commands, steps the host, polls outcomes, and provides rejection reasons in the next round. The model has no shell or host tool access.

## Run

```powershell
python tools/llm_republic.py --host build/headless/src/economy_host/Release/OpenRepublicEconomyHost.exe --backend codex --rounds 8 --ticks 500 --transcript C:\temp\llm-rounds.jsonl -- --region <region> --content-root <content-root> --radius 450
```

The Codex backend runs `codex exec -s read-only -m gpt-5.6-luna --skip-git-repo-check --ephemeral -` in a temporary directory outside the repository. `openai-compatible` uses `OPENAI_COMPATIBLE_BASE_URL`, `OPENAI_COMPATIBLE_MODEL`, and optional `OPENAI_COMPATIBLE_API_KEY` with Python `urllib`; defaults target an LM Studio-style localhost endpoint. `claude` uses `claude -p` when installed. Model errors or invalid JSON produce a logged empty round; `--fallback-advise` enables deterministic haul suggestions for that failed round.

Allowed commands are place_building, remove_building, build_road, set_building_enabled, set_road_enabled, create_haul_order (batch submitted through the host's `create_haul_orders`), cancel_haul_order, retarget_haul_order, connect_utility, and disconnect_utility. At most 24 actions are considered per round. The host independently checks its revision and command semantics. Coordinates are limited to ±100000 integer cells; roads must be a single orthogonal segment. The transcript records an observation SHA-256, validated actions, results/reasons, score, round notes and tick. Keep region transcripts outside Git because observations can include private map information.

## Self-test and replay

```powershell
python tools/llm_republic.py --self-test
python tools/llm_republic.py --host <host.exe> --replay C:\temp\llm-rounds.jsonl --rounds 8 --ticks 500 -- --region <same-region> --content-root <same-root> --radius 450
```

Replay applies recorded validated actions against a fresh host started with matching initial inputs. It is deterministic only to the extent that the host's starting world, catalog, command ordering and step sizes match; compare per-round hashes/scores to inspect divergence. Source/API behavior is VERIFIED by local checks; no WRSR formula, parity, or original-game outcome follows from this controller. The score is the autopilot-compatible `stored_non_raw_units` metric (electricity excluded).

## First real run (2026-10-23, N22 region, radius 450, 8 rounds x 500 ticks)
Model: Codex CLI with `gpt-5.6-luna`, read-only sandbox, one call per round (about 1-3 minutes each), `--fallback-advise` on, no backend errors. Score metric = stored non-raw units (electricity excluded), same as the autopilot `--json` benchmark. Result at tick 4012: **632.3** after rounds scoring 0, 0, 264, 347, 432, 480, 579, 632 (the model queued 16, 8 and 19 actions in rounds 1-3, then nothing). The rule-based `tools/republic_autopilot.py` on the same world at tick 4017 scored **730.4**, an idle world scores 0. So the LLM controller reached 87% of the rule-based baseline: it placed power and wired utilities correctly but stalled early (rounds 4-8 issued no actions). One run each, nondeterministic model, not a significant comparison. Practical notes: the host executable must be rebuilt after protocol changes (an old build rejected `connect_utility`), and long runs must be started detached (a child of a short-lived shell was killed).
