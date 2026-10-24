# OpenRepublic contributor and coding-agent rules

- Read `docs/PROJECT_PLAN.md`, `docs/DECISIONS.md`, and the relevant evidence-ledger entry before implementation.
- This project is an independent client and simulation. Keep original game files read-only and never include WRSR binaries, assets, save payloads, extracted caches, or decompiled source in Git or release artifacts.
- Keep public research outside the user's private local analysis data. Exact paths, proprietary file fingerprints, Ghidra databases, and private REA output belong outside this repository.
- Label claims VERIFIED, OBSERVED, INFERRED, or UNKNOWN. Record game version/build/hash for game-specific evidence; do not reuse offsets between builds without revalidation.
- Do not invent WRSR formats, APIs, economy formulas, licensing permissions, or compatibility claims. Unknown remains unknown until the evidence ledger supports a stronger status.
- Preserve the C++ simulation as the authority; rendering, Lua rules, and optional external controllers communicate through explicit versioned interfaces.
- Keep dependencies and licenses explicit. Do not copy GPL mod-loader code into this MIT project without an approved licensing change.
- Make small scoped changes. Do not launch the game, modify saves, write to the Steam installation, publish assets, or change user-level tools/configuration unless the task explicitly requires that action.
- Separate source validation, build results, original-game comparison, and graphics/hardware acceptance in reports.
- Scope, goal, target, architecture map and working method are in CLAUDE.md; read it together with this file.
