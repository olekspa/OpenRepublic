# Map package scenic survey

## Results table

Indices are anonymized, deterministic labels assigned from the case-insensitively sorted paths discovered under the configured roots. This table does not reveal package names or paths. **OBSERVED** means local bytes were decoded by this survey's bounded reader; it is not a game runtime or compatibility claim.

| Rank | Package index | Score / 100 | World (km) | Relief P95-P05 (m) | 3 km town relief (m) | 250 m building cluster | Below-zero area within 3 km (km²) | Buildings | Trees | Road nodes |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 27 | 95.5 | 20.0 × 20.0 | 333.6 | 521.4 | 150 | 0.964 | 1769 | 1518777 | 1530 |
| 2 | 19 | 92.1 | 20.0 × 20.0 | 165.6 | 401.6 | 148 | 1.477 | 370 | UNKNOWN | UNKNOWN |
| 3 | 23 | 91.4 | 20.0 × 20.0 | 188.3 | 388.6 | 239 | 1.081 | 3284 | 929829 | 2589 |
| 4 | 25 | 84.7 | 20.0 × 20.0 | 385.6 | 441.7 | 122 | — | 1771 | 357298 | 1357 |
| 5 | 21 | 84.1 | 20.0 × 20.0 | 343.8 | 312.7 | 59 | 4.507 | 668 | 1753830 | 1088 |

## Generic methodology

The script searches each supplied root recursively for folders containing `script.ini`, `heightmap.dds`, and `buildings_game.bin`. Provide each authorized input location with the repeatable required `--root PATH` option; also provide `--private-dir PATH` outside the repository. Results are local observations. The script only reads inputs.

- **World dimensions and height:** Read `$WORLDSIZE`, `$WORLDPOSITION`, `$HEIGHTSCALE`, and `$HEIGHTMAPSIZE`. Decode the bounded legacy single-level DDS FourCC 114 R32F payload. Apply `raw × HEIGHTSCALE + WORLDPOSITION.y`. The centered X/Z transform and identity raster orientation are verified in the project ledger for one map only; other packages inherit that uncertainty.
- **Sampling and terrain metrics:** Raster sampling is regular with a default maximum edge of 512 samples. Only sampled source values are checked for finiteness. Height min/max/range, below-zero and above-threshold fractions, and interpolated P95-P05 are sample estimates. Mean slope averages `atan(hypot(dx-height gradient, dz-height gradient))` over adjacent sampled cells in degrees. Sampling stride appears in the private report.
- **Buildings and dense cluster:** Parse the declared count and candidate placement records using the current bounded OpenRepublic reader's observed record shape (survey file-size cap 512 MiB). Coordinates are matrix translations X/Z. For every building, count placements within a 250 m horizontal radius including itself; return the earliest file index on ties, matching `FindDensestPlacement`. The exact placement schema remains inferred from one build's samples.
- **Trees and roads:** Tree counts validate species headers and the declared 17-byte placement payload lengths; positions are not decoded here, and species semantics/yaw/scale remain unknown. Road node counts are accepted after validating the file header and node blocks. Missing or undecodable files show `UNKNOWN`, never zero. Both layouts have build-scoped evidence limits recorded in the project ledger.
- **Town-near relief and below-zero component:** Within a sampled 3 km radius of the densest building, relief is sampled max-minus-min. The largest 4-connected below-zero sampled component is converted to km² using sampled cell spacing. Below zero is only a candidate water/lowland proxy: water extent, sea level, and map mask semantics are unverified. It must not be described as confirmed coastline or lake.
- **Transparent score:** `100 × (0.40 × min(cluster_count/80,1) + 0.35 × min(local_relief/300m,1) + 0.15 × max(0,1-nearest_below_zero/5000m) + 0.10 × min(map_relief/500m,1))`. This favors a dense town, substantial nearby terrain, and a nearby below-zero candidate. It is a heuristic for choosing a visual showcase, not a game metric.
- **Status and limitations:** Height transform has one-map validation; placement record scanning is inferred; tree decoding is partial; road node parsing is build-scoped. Threshold fractions and relief are sampled, and all below-zero area claims are hypotheses. No game launch, save modification, or graphics/hardware acceptance is part of this survey.

The private report and candidate relief images are written to the configured private directory. No exact private paths, package names, or fingerprints are written by this script to this document.
