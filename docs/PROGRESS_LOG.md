# OpenRepublic progress log (chronological, timestamped)

Tool-neutral running log for Claude and Codex/Luna sessions. Times are America/New_York (EDT) with UTC in brackets. `docs/DEVELOPMENT_CHECKPOINT.md` remains the detailed handoff; this file is the short, ordered record. Append new entries at the bottom. Evidence statuses follow `AGENTS.md` (VERIFIED / OBSERVED / INFERRED / UNKNOWN).

## 2026-10-21

- **~21:30 EDT [01:30 UTC]** Claude takeover. Local `main` was 11 commits ahead of `origin/main` (D-005 gates publication). Baseline client CTest 15/15.
- **~21:50 [01:50]** Found why NMF previews were dark/"stepped": source triangles are wound opposite to Wicked's front face (418/418 and 3791/3791 triangles agree with stored normals). Index swap in `BuildNmfPreviewGeometry`, ambient fill light for NMF previews. School renders textured. Commit `c846fa1`.
- **~22:00 [02:00]** Second model (`residental_brick_3`) verified: bounds, winding, UV direction. Corpus survey: NMF/MTL name mismatches are common. Commit `49bb13a`.
- **~22:30 [02:30]** Ghidra headless on C3DDLL32.dll: material loader rules (single-section selection, first-exact-name else first, `$TEXTURE_MTL` relative to MTL directory). Implemented and wired. Commit `448eaf3`.
- **~23:00 [03:00]** `buildingtypes.ini` parser + `--content-root/--building-type/--building-lod`; whole real file (489 types) parses and resolves. Commit `4459c07`.
- **22:15 EDT [02:15 UTC, 22 Oct]** `C3D_BUILDING::Import` traced: no comment syntax (unknown tokens skipped), `MODEL_LOD`/`MODEL_LOD2` numbers default to 1000/2000, `MODELEMISSIVE` discarded, `MATERIALEMISSIVE` loads a parallel material, `MIRRORZ_` derived types. Ledger updated; parser exposes `loaderNumber`.

## Operating notes

- Launching the game: `steam.exe -applaunch 784150` works; running the game launcher exe directly or `steam://rungameid` fails (reported by the project owner on 2026-10-21). The game and saves remain read-only research sources per `AGENTS.md`.
- Ghidra projects must live under a path without dot-prefixed folders (e.g. `D:\ghidra_claude_c3d`); trace scripts and outputs are archived in the private research folder `OpenRepublic-Research\ghidra-claude`.
