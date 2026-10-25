# Native objectives

Native objectives are an OpenRepublic tutorial layer evaluated from the authoritative economy `WorldSnapshot`. They are read-only and do not change economy state or hashes. Every described mechanic is OpenRepublic synthetic behavior; WRSR parity is **UNKNOWN**.

## Objective model

Objective sets are ordered and each item has an identifier, title, hint, metric, optional building type/resource filters, and an unsigned target. `EvaluateNativeObjectives()` is a pure function: it returns each metric value, completed flags, fraction complete, completion state, and the first incomplete hint. The runtime metrics are `inventory_at_least`, `delivered_at_least`, `construction_sites_count`, `construction_sites_complete`, `population_at_least`, and `building_count_at_least`.

`data/objectives/` contains the authored coal demo, construction sites demo, and growth town sets. These definitions parallel the deterministic fixture/scoring approach in `tools/scenario_score.py`; the scorer remains the single place that runs and scores economy scenarios. Objective checks do not duplicate scenario execution or scoring.

Start the desktop objective mode with a built-in set:

```powershell
OpenRepublic.exe --native-objectives builtin:coal_demo
```

The option initializes native economy mode. A JSON file can also be supplied by path, for example `--native-objectives data/objectives/construction_sites_demo.json`. JSON sets use the schema shown in those files; unknown metrics or malformed JSON fail startup with a diagnostic. The objective panel shows ordered completion marks, a progress bar, the first incomplete hint, and a completion banner. **Restart objectives** restores the world to the authored state captured at startup.

`delivered_at_least` is currently measured from inventory held at matching destination buildings. The snapshot does not expose durable delivery history, so this is an observable stock proxy and can decrease if production consumes the delivered resource. Goals that need historical delivery credit remain a known limitation.

## Evidence boundary

**VERIFIED** source/unit tests cover pure evaluation over `WorldSnapshot` and the authored built-in definitions. Completion of scenario goals and rendered checklist behavior require a native desktop run. **UNKNOWN:** whether any objective wording, threshold, or mechanic reproduces WRSR. Scenario scores are authored OpenRepublic quantities, not WRSR outcomes.
