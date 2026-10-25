# Native desktop building and road UI

This guide covers the native economy sandbox (`--native-economy` and native checkpoints). The UI submits versioned commands through the EconomyApi; its placement overlay is a preview and the economy remains authoritative.

## Building placement

Choose a type in the building dropdown. Its tooltip lists the authored footprint, recipe inputs and outputs, and any declared OpenRepublic construction materials/work requirements. Hover while placement is active to see a cell-snapped translucent footprint. Green means the current snapshot passes the client-side footprint/access preview; red means it does not. The queued command still performs final validation when stepped. The preview mirrors current bounds, footprint/access reservations, roads and rails; pending commands can make its color stale until the next snapshot.

## Roads and removal

Select **Build road** or press **R**, click a start cell and hover an endpoint for a straight rubber-band preview. The line shows segment length, capped at 64. Road cost is unavailable because the native economy has no road-cost field. Click the endpoint to queue each unit segment independently; step the world to apply them. A partial submission is reported and the unsent remainder is retained as a new draft.

Press **X** to remove a building under the pointer. Building removal uses the existing command and discards its inventory. The current EconomyApi has no road-removal command, so bulldozing roads is not supported; road enable/disable is a separate existing operation.

## Shortcuts

- **B**: select the current building type and start placement
- **R**: start road placement
- **X**: click a building to remove it
- **Esc**: cancel an active tool
- **Space**: pause/run the native economy
- **1–4**: choose 1–4 economy ticks per clock step

A hint line is shown in the native economy status. Shortcuts are ignored while a toolbar widget owns input.

All footprints, recipes, construction requirements, time pacing and removal semantics here are OpenRepublic behavior. WRSR parity is **UNKNOWN**. Client preview feedback is not a guarantee of command acceptance, and no GUI session was run for this change.
