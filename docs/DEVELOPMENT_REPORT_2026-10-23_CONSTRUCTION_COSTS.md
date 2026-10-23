# Construction cost importer boundary - 23 October 2026

## Status

Partial. Added an opt-in diagnostic switch for retained `$COST_*` declarations. The requested conversion into delivered construction materials and work is not implemented because this checkout has no native construction-site lifecycle to receive it, and the source directive units/selection semantics remain UNKNOWN. No conversion factors were guessed.

## Changes

- `CatalogImportOptions::constructionCosts` enables deterministic `ImportReport::warnings` for every retained `$COST_*` row. It reports the source building, directive, and line.
- Economy host accepts `--construction-costs` with `--region`; the option is rejected with catalog/checkpoint modes. Existing behavior is the default.
- Synthetic compat coverage confirms the option leaves canonical catalog text byte-identical and emits one unsupported warning per synthetic cost row.
- Compatibility docs, project plan and evidence ledger record unknown conversion factors and the native API gap. No `build_a_mill` scenario was added because scenario setup cannot represent a construction site.

## Verification

- `cmake -S . -B build/headless -DOPENREPUBLIC_BUILD_CLIENT=OFF` - passed.
- `cmake --build build/headless --config Release -- /m` - passed.
- `ctest --test-dir build/headless -C Release --output-on-failure` - passed, 52/52.
- `git diff --check` - passed.
- CMake disabled the optional EconomyHost target because the pinned `nlohmann/json.hpp` was unavailable. The host argument parser addition therefore was not compiled in this configuration.
- No game, original files, saves, or app code were touched.

## Remaining work / fidelity blockers

- Add a native construction-site entity/state with material delivery, work progression, completion, save/load, API exposure, and scenario setup/scoring support.
- Resolve `$COST_WORK` to simulation-time units; `$COST_RESOURCE_AUTO` resource/amount rules; `$COST_RESOURCE` argument and amount semantics; and `$COST_ALL_MULTIPLY`, node and keyword multiplier/selection semantics from appropriate evidence.
- Only then convert supported declarations. Unsupported directives must remain raw and produce warnings.
