# FH2 Anti-Lag

Race turbo upgrades with anti-lag, native exhaust pops and flames for Forza Horizon 2 running through Azimuth and Extra Options.

**v0.6.8 Beta** — community mod, not an official Forza release.

## Requirements

- Windows x64.
- Forza Horizon 2 Xbox One build `1.0.0.64`.
- Azimuth **1.0.4** and Extra Options **0.4.4**.
- A compatible engine with a purchasable single or twin race turbo.

The plugin checks the Extra Options version and game-code signatures. Other versions are unsupported and may be refused. Neither Azimuth, Extra Options nor game files are included.

## Installation

1. Close the game and the Azimuth launcher completely.
2. Back up your existing `mods` folder and `data/titles/1A96545B/saves` folder.
3. Download the **FH2-AntiLag-v0.6.8-beta.zip** release asset. GitHub's automatic source archives do not contain the compiled plugin.
4. Copy the ZIP's `mods/Extra Options - Anti Lag` folder into the `mods` folder beside `Azimuth.exe`. Install exactly one copy of this mod. When updating, retain your existing `plugins/EO_AntiLag.ini` AND `plugins/EO_AntiLag_Profiles.ini` to keep your configuration and per-model tuning. Copy the other release files over the old version.
5. The Extra Options loader discovers new mod folders automatically. If this mod was previously disabled in `mods/loadorder.txt`, uncomment its exact folder entry. Restart the game.

## Usage

In the workshop, select **Engine → Turbo / Twin Turbo** and equip **Race Turbo with Anti-Lag** or **Race Twin Turbo with Anti-Lag**. The normal turbo does not enable anti-lag. After changing from twin to single turbo, equip the corresponding special single-turbo part.

Accelerate to build boost, then release the throttle. Anti-lag retains part of the attained turbo state and requests native exhaust pops and flames. Reapplying throttle cancels the lift-off effect. Anti-lag is automatically available whenever the special turbo is equipped. There is no F7 toggle; legacy Enabled and ToggleVirtualKey INI entries are ignored.

The default cutoff is 1800 RPM and charging requires at least 2500 RPM, sufficient throttle and boost. Effect duration depends on pressure, RPM and pedal input; the 6000 ms decay setting is not a guaranteed six-second effect.

## Tune Car > TURBO

Adjust pressure decay, boost retention, cutoff RPM and pop interval in the native tuning screen. Changes are saved immediately for the **car model**, separately from the game's normal Apply/Cancel flow; two garage copies of the same model share settings. Existing profiles are preserved, including through engine/turbo swaps. Cars without a profile start from the general INI values.

Loading the game's **Default tuning settings** is intended to reset the current equipped anti-lag car profile to **6000 ms / 85% / 1800 RPM / 140 ms**. The Global/This car selector has been removed. The four native-style headers indicate the direction of adjustment. Context help is intended to follow the focused row. This new screen is in English; workshop part translations remain available as below.

**Latest beta validation:** the user confirmed the TURBO tab opens, values display and sliders can be edited in the preceding build. The new description visibility and native defaults-reset integration in 0.6.8 have passed automated/simulated checks but still need confirmation in FH2. Treat this as a testing release.

## Workshop behaviour

- Adds a separate part without replacing existing turbo parts.
- Uses the corresponding race turbo's physical specifications, with a 2000-credit price premium. The PI calculation does not account for the anti-lag advantage.
- Generates parts from eligible race-turbo rows in the merged in-memory database. The original game database is not modified on disk.
- The original database yields 151 single-turbo and 170 twin-turbo part rows; these are engine variants, not a count of cars.
- Part names/descriptions include BR, CHT, CZ, DE, EN, ES, FR, GB, HU, IT, JP, KO, MX, NL, PL and RU text. DEV/LOC use English. These are community translations.

## Save data and removal

The plugin does not directly edit save files. **The game's normal autosave can save purchases and equipped custom parts.** Keep the mod available while those parts are equipped.

Before disabling, uninstalling or downgrading to a version without the workshop catalogue, equip an original game turbo on **every affected car**, then allow the game to save. Close the game and remove only this mod folder. Do not restore older saves automatically.

## Compatibility and beta status

The tester confirmed the workshop part appears and works, including a twin-to-single race-turbo-with-anti-lag change on the RX-7 FD. Catalogue transactions, collision rollback, part gating and native string merging were tested separately. Menu layout in every language, broad vehicle compatibility and all purchase/save/reload scenarios have not been exhaustively verified.

Added cars and engine swaps may work when they supply compatible race-turbo rows before the provider runs. Compatibility with arbitrary car mods is not guaranteed. Existing single/twin level-4 parts, reserved-ID collisions, or modifications to hooked native code can cause this plugin to refuse activation. Report your exact mod versions and load order when requesting support.

Sounds and effects use FH2's native events. This does not include or reproduce FH5/FH6 audio assets. Check `plugins/EO_AntiLag.log` and `FH2_ExtraOptions.log` if a part or effect is missing. Share only relevant log excerpts; do not upload saves or personal backups to issues.

## Building

Source is in `src`. With Python 3 and a compatible Windows x64 GCC/Clang toolchain available, run `python Build.py`. The output is `build/EO_AntiLag.dll`; copy it to the mod's `plugins` folder when packaging. Generated `signatures.h` and `catalog_sql.h` are included, so the game executable is not needed to build this version. `catalog.sql` is the readable catalogue source.

## Credits and licence

Maintained/published by **erikzensounds**. Built against [RokkuDayo's Forza Horizon 2 Extra Options](https://github.com/RokkuDayo/ForzaHorizon2_ExtraOptions), using its plugin SDK and adapted guarded-hook support. The upstream hook source identifies FH2 LiveTune as its origin. See [CREDITS.md](CREDITS.md).

Distributed under **GNU GPL v3.0**; see [LICENSE](LICENSE). Corresponding source and build instructions accompany the release. Preserve upstream notices when redistributing modified versions.
