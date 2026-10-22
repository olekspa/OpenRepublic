# Graphics and performance direction

## Today (VERIFIED on an RTX 5070 Ti, DirectX 12, Wicked Engine pinned revision)
`--graphics "key=value;..."` (also the type `runtime::GraphicsSettings`, intended for the agent interface) controls: `upscaler` (`none`, `fsr2_quality|balanced|performance|ultra_performance`, `dlss` reserved), `ao` (`off|ssao|hbao|msao|rtao`), `rt_reflections`, `rt_diffuse`, `ssr`, `fxaa`, `bloom`, `exposure`, `sharpen`. Unsupported features are logged, never ignored silently. Captures wait 48 frames per view when temporal features are on so FSR2/ray-tracing history matches the view (ghosting otherwise). Example: `--graphics "upscaler=fsr2_quality;ao=rtao;rt_reflections=on;rt_diffuse=on;fxaa=on"`.

## DLSS 4.x / frame generation (planned, not integrated)
Wicked Engine ships FSR2 and hardware ray tracing but no NVIDIA upscaler. DLSS super resolution, ray reconstruction and multi-frame generation require the NVIDIA Streamline SDK (DX12 proxy swap chain, motion vectors, depth, exposure and UI-excluded colour inputs). Plan: keep `runtime::Upscaler` as the neutral interface; add a Streamline-backed implementation behind it once the SDK terms are accepted and the renderer exposes motion vectors/jitter; `upscaler=dlss` already parses and reports "unavailable" until then. Neural-rendering features beyond what Streamline exposes publicly are out of scope until an SDK exists.

## Performance (planned)
- Simulation: the economy core is deterministic and single-threaded. Production is independent per building within a tick; the plan is a two-phase tick (parallel read-only evaluation into per-building deltas, ordered single-thread merge) so multithreading cannot change results, verified by state-hash equality against the serial path.
- Rendering: instancing is already used for placed buildings (one mesh per type). Next: LOD selection from the building definitions' LOD fields (loader defaults 1000/2000), frustum/occlusion handled by Wicked, texture streaming for large maps.
- Compute offload candidates: terrain tessellation/normal generation, vehicle/path queries, road graph rasterization.
