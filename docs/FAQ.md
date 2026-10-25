# Frequently asked questions

## Is OpenRepublic a WRSR mod or replacement renderer?

No. OpenRepublic is an independent client and simulation. It does not run inside the WRSR process and is not a mod loader or repackaging.

## Is it a complete playable WRSR replacement?

No. The project has playable OpenRepublic synthetic economy demos and experimental local-content region views. The long-term WRSR gameplay and visual parity goal remains incomplete. Synthetic production, trade, population, construction, utilities, calendar, depots, rail, vehicle behavior, and scores have **UNKNOWN WRSR parity**. See the [parity roadmap](PARITY_ROADMAP.md) and [current handoff](HANDOFF_2026-10-24.md).

## Do I need WRSR installed to try the demos?

No. The native sandbox, coal logistics demo, and construction-sites sandbox use OpenRepublic-authored synthetic content. Region mode optionally reads selected content from a local install for compatibility experiments. It is not required to build or try the synthetic demos.

## Which demo should I start with?

Use `OpenRepublic.exe --native-economy --native-coal-demo` for a prebuilt logistics loop with a depot, trucks, trade, storage, and a heating plant. Use `OpenRepublic.exe --native-economy --native-construction-sites` to inspect construction-site behavior. The plain `--native-economy` option opens the general synthetic sandbox. See [Playing OpenRepublic](PLAYING.md).

## Can OpenRepublic open my WRSR save?

No. OpenRepublic has its own versioned economy saves and checkpoints. It does not load or write WRSR saves. Do not select an original save as an OpenRepublic checkpoint.

## Does region mode reproduce the whole map or its economy?

No. Region mode uses bounded readers and currently supported import paths for selected buildings, terrain, roads, and other documented content. Unsupported objects may be skipped. It does not reconstruct every building's runtime state, save state, native gameplay system, or original economy rule. Import acceptance is an OpenRepublic compatibility experiment, not evidence of complete game behavior.

## Are the screenshots or game assets included?

The README's screenshot area is a placeholder; it does not embed game captures or assets. The repository must not contain WRSR binaries, assets, saves, extracted caches, or decompiled source. Project-generated captures and synthetic presentation details should be labeled clearly; previewing local content does not grant redistribution rights.

## Is OpenRepublic safe to distribute with original assets?

This repository does not establish permission to redistribute WRSR content or a binary that uses it. Keep original content local and read-only. Distribution rights remain unresolved; read [decision D-005](DECISIONS.md#d-005-local-assets-are-an-experiment-not-a-release-permission).

## Can an AI controller change the game directly?

The optional LLM harness proposes bounded JSON actions to the local OpenRepublic economy host. Python validates them, and the C++ simulation validates and applies accepted commands at tick boundaries. The controller does not operate WRSR. The host role flag is not authentication: keep the JSONL process local and trusted. See [AI controllers](AI_CONTROLLERS.md) and [Economy API](ECONOMY_API.md).

## Why does a queued action not immediately change the world?

Submissions are queued and checked by the authoritative economy at a tick boundary. Read the returned handle with `poll` after stepping to distinguish accepted, rejected, or still queued commands. A successful submission response means the host admitted the request, not necessarily that the operation completed.

## Why is a toolbar control missing or the scene blank?

Controls vary by mode and the toolbar can reflow with window size. In native economy mode, try **Frame world** if the view is stale or the camera does not frame the current world; maximize the window if the scene is obscured by the panel. Make sure the app is launched from its build directory so the Wicked Engine `Content` folder is available. Build output verifies compilation, not graphical acceptance; the dated handoff records what has or has not been observed live.

## Where are modding examples?

See [Modding](MODDING.md), the example [economy rules](../data/rules/economy_rules.lua), [scene scripts](../data/scripts), [scenarios](../data/scenarios), and the focused guides for [population](POPULATION.md), [trade](TRADE.md), [construction sites](CONSTRUCTION_SITES.md), [calendar](CALENDAR.md), [depots](DEPOTS.md), and [rail](RAIL.md).
