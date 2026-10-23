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
