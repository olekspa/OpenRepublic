# New-game setup facts (24 October 2026)

Static review of the original's new-game setup routine and the header import loader (build context 23935965). Everything here is **VERIFIED (static review)** unless marked otherwise; no decompiled code or proprietary data is recorded. The setup routine initialises game-setting globals from menu option variables, then the header loader (see [MAP_SURVEY.md](MAP_SURVEY.md)) overwrites them from `header.bin` slots.

## Start date

- **Start year** comes from an option value: 0 -> 1960, 1 -> 1970, 2 -> 1980, 3 -> 1930, 4 -> 1940, 5 -> 1950, 6 -> 1920; a value above 1000 is used directly as the year. Options 3 to 6 additionally set a flag byte (meaning UNKNOWN). `StartYearFromOption` in `src/content/map_header.h` encodes this.
- **Start day** (day of the year) depends on the map's terrain type: type 3 -> 129, type 1 -> 26, type 2 -> 51, any other type -> 53. The tutorial map "tutorial_map7" uses 285. (Day is counted from the setup's value as given; whether it is zero-based is UNKNOWN.) Terrain type names are UNKNOWN beyond these numeric ids.
- The header slots at +0x19c (year) and +0x198 (day) hold the same two values in a saved game.
- A campaign/tutorial mode flag overrides several settings (e.g. day 77, a fixed settings pattern), values recorded only in the private notes.

## Option variables copied into settings

The setup routine copies twelve option variables and three boolean options into settings globals; the header loader restores them from these header slots. The option names come from the new-game menu and are **UNKNOWN** here (they are probably the difficulty and rule toggles), so only the slot-to-option mapping is recorded, in private notes. Slots: +0x18c, +0x190, +0x194, +0x1bc, +0x1c0, +0x1c4, +0x1c8, +0x1cc, +0x1d0, +0x1d4, +0x1e8, +0x1ec, +0x1fc, +0x200 (settings values) and +0x1f0, +0x1f4, +0x1f8 (booleans).

## Open items

- Name the option variables from the new-game menu's UI strings and the consumers of each settings global (the most-referenced ones have more than 100 consumers).
- The 0x54-byte extended block carries further settings (defaults such as 70 and 20 appear in the setup routine); contents UNKNOWN.

## Option slots behave as 0 to 2 levels (added 24 October 2026; real time 2026-10-23)

**VERIFIED (static review of consumers, build context 23935965):** the option global restored from header slot +0x194 takes only the values 0, 1 and 2, and its consumers choose between three constants by that value (several routines branch on 0, 1 and 2 explicitly). The same pattern holds for the globals restored from +0x1c4, +0x1c8, +0x1cc, +0x1d0, +0x1d4 and +0x1e8, which a gating routine compares with small constants. **OBSERVED:** in the sample package (version 121) these slots hold +0x194 = 2, +0x1c4 = 1, +0x1c8 = 2, +0x1cc = 1, +0x1d0 = 2, +0x1d4 = 0, +0x1e8 = 1, +0x190 = 2, +0x1bc = 2, +0x1c0 = 1, +0x1d8 = 1 (an extended-block flag), all within 0 to 2. A gating routine tests exactly (+0x194 == 2, +0x1c8 == 2, +0x1cc == 1, +0x1d4 == 0, +0x1e8 == 1, with +0x1c4 and a further value above 0) and the sample satisfies it, so that combination looks like one named preset (**INFERRED:** the unmodified or default rule set; the routine itself is an achievement or hint check). **INFERRED, not established:** these slots are the new-game rule/difficulty options (three levels each). Their individual names (for example resource prices, disasters or pollution settings) are **UNKNOWN**; naming each needs the new-game menu strings tied to its option variable.
