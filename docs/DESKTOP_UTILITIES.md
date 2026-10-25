# Desktop utility connections

**Status:** OpenRepublic-authored native economy UI. **WRSR parity: UNKNOWN.**

In native economy and app-owned imported-region modes, select a building and
choose **Set haul source** to store the first endpoint. Select a different
building, choose a declared utility resource in the utility selector, then use
**Connect utility** or **Disconnect utility**. These controls submit the
existing revision-bound `ConnectUtility` / `DisconnectUtility` API commands;
the endpoints must be distinct live buildings. The source selector is shared
with haul tools, while utility links move no cargo.

Connected links are drawn between building centers as elevated colored wires or
pipes. The color distinguishes resource IDs and indicates connectivity only;
it does not represent flow, pressure, capacity, or direction. The synthetic
utility model has idealized shared-grid connectivity and no transmission loss
or span limit. See [Native utility networks](UTILITY_NETWORKS.md) and
[Synthetic heating and water](HEATING_WATER.md) for simulation behavior.

After a simulation tick, the selected-building inspector reports the latest
utility network's generated supply, consumed amount, eligible demand, unused
supply, and short recipes for the selected utility. A building with no measured
utility component has no such row. Before the first measurement, after restore,
or after catalog replacement, advance one tick; unavailable measurements are
reported as such. These are component totals from the OpenRepublic synthetic
simulation, not per-building physical readings.

Startup sets the process working directory before engine initialization. The
pure resolver selects the nearest executable ancestor containing a recognized
Wicked Engine `shaders` resource directory, then falls back to the executable
directory when no resource tree is found. Unit coverage exercises ancestor
selection and fallback; launching from an unrelated directory remains a
separate desktop runtime check.
