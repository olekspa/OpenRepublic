# Contributing to OpenRepublic

This project is in research and feasibility planning. Small, reviewable contributions with a clear source and reproducible evidence are most useful.

## Evidence and reverse engineering

- Separate **VERIFIED**, **OBSERVED**, **INFERRED**, and **UNKNOWN** statements. Do not turn a plausible format or gameplay interpretation into a recovered contract.
- Tie native addresses and offsets to an exact file hash and game build. A result from one executable does not automatically apply to another build.
- Prefer official documentation, existing text definitions, public format descriptions, and controlled before/after observations. Use Ghidra/REA for a specific unanswered question, not as a reason to decompile the whole game.
- Keep user-owned game files read-only. Use copies of saves for experiments. Do not commit proprietary files, extracted asset payloads, decompiler output, private analysis databases, or credentials.
- Never use an observation from the original game as proof of OpenRepublic behavior until the new implementation has its own regression evidence.

## Code and dependency review

- Keep the simulation independent of the renderer and run it without the game installation.
- Add or update a focused validation when changing a format reader, simulation rule, or save/replay contract.
- Record third-party code, data, and binary dependencies with their exact version and license before adding them.
- Do not copy code from GPL-licensed mod loaders or plugins into this MIT-licensed repository without an explicit licensing decision. Public behavior descriptions and independently implemented ideas are a separate research input.
- Contributions are provided under the repository's MIT license unless a different written agreement is made.

## Pull requests

Describe the behavior changed, evidence and sources, validation performed, game/build scope where relevant, and remaining unknowns. Never claim WRSR compatibility or a performance improvement from a screenshot or synthetic microbenchmark alone.
