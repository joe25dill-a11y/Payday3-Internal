# Sky fork — added features (`sky-features` branch)

Notes for what was added on top of [Omega172/Payday3-Internal](https://github.com/Omega172/Payday3-Internal).  
Build the DLL yourself (`xmake`); the `.dll` is not committed (`Build/` is gitignored).

**Menu:** `INSERT` · **Unload:** `END`  
Risky toggles (god, grab, spawners, etc.) **start OFF every launch**.

---

## Misc — toggles (click-to-bind hotkey next to each)

| Feature | What it does |
|--------|----------------|
| **God Mode** | Incoming damage ×0 + HP/armor top-up. Does **not** block tasers. |
| **Infinite Ammo** | True-inf flag + mag refill. Guns only (not placeables). |
| **More Bullet Damage (×1000)** | Patches equipped weapon FireData (Nexus InstantKill-style). |
| **Carry More Bags (you + AI)** | Raises `MaxCarryBagCount` to 50 for you and AI crew. |
| **No Civ / Custody Penalty** | Clears civ-kill + custody cash docks on results (solo/host best). |
| **Friendly Fire** | Host PvP path (you → them). Crash-safe hold-LMB style. |
| **Grab All (loot)** | Auto claim / floor bags. Leaves Num7 free for Lua. |
| **Grab Access** | Keycards / RFID / press badge. Leaves Num/ free for Lua. |
| **Insta Drill** | Drills / PCs / thermite / lance + cleaners + minigame. |
| **Silent Despawn Cops** | Buries `CH_BaseCop` only — not Houston / FWB / civs. |

Click the **None** (or key name) button next to a checkbox → press a key to bind → that key toggles the feature. Esc clears the bind. Binds save in config.

---

## Misc — Client Move (noclip)

| Control | Notes |
|--------|--------|
| **Client Move** | Noclip / fly (default Mouse5). |
| **Teleport** | Online: syncs your noclip position to the server (default **Z**). Not the same as the Teleport **tab**. |
| **Move Faster** | Hold to double fly speed (default Left Shift). |
| **Auto Teleport** | On release of Client Move, sync position (online). |

---

## Teleport tab (preset JSON)

Separate from Client Move teleport:

- Load Heist Farmer / Scout Freecam style JSON
- Prev / next spot, teleport you, teleport everyone, tour helpers

---

## Spawner (buttons + click-to-bind hotkeys)

| Button | Action |
|--------|--------|
| **Meth spawn** | Cook Off-style meth bag spawn path |
| **Van drive-in** | Escape van drive-in |
| **Green exit** | Green escape / exit path |
| **Money / results** | Money / results screen helpers |

Same bind UI as Misc toggles: click **None** → press key → key fires that one-shot.

---

## Source files (main additions)

- `Features/Misc/GodAmmo.*`
- `Features/Misc/CarryBags.*`
- `Features/Misc/NoCivPenalty.*`
- `Features/Misc/FriendlyFire.*`
- `Features/Misc/GrabAll.*` / `GrabAccess.*`
- `Features/Misc/InstaDrill.*` / `SilentKill.*`
- `Features/Misc/SpawnerTools.*` / `PresetTeleport.*`
- `Menu.cpp` / `Menu.hpp` — UI, config save/load, hotkeys
- `Features/Features.cpp` / `DLLMain.cpp` — wiring

---

## Not included in this notes dump

- Full Dumper-7 SDK refresh on disk (local only; not pushed with the feature commit)
- Prebuilt `Payday3-Internal.dll` — build Release and inject that
