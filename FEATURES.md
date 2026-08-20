# Sky fork — added features (`sky-features` branch)

Notes for what was added on top of [Omega172/Payday3-Internal](https://github.com/Omega172/Payday3-Internal).  
Build the DLL yourself (`xmake`); the `.dll` is not committed (`Build/` is gitignored).

**Menu:** `INSERT` · **Unload:** `END`  
Risky toggles (god, grab, spawners, etc.) **start OFF every launch**.

**Tabs:** Visuals · Aimbot · Stealth · Misc · Teleport · Debug

---

## Visuals — enemy / civilian ESP

- Enemy / special / civilian ESP toggles (box, health, armor, name, flags, skeleton, outline)
- **ESP color pickers** — Box / Health / Armor / Skeleton / Highlight (corner brackets when Outline on)
- **Key Items ESP** — keycards, RFID, phones, meth ingredients, USB, C4, planks (+ color)

## Visuals — Loot ESP

Separate from actor ESP; uses through-wall glow (V2-style outlines):

| Option | Notes |
|--------|--------|
| **Loot ESP** | Master toggle |
| **Loot Outline** | Wallhack-style outlines on loot actors |
| **Strong Glow (F4+)** | Brighter double-pulse glow |
| **Labels** | Optional text labels |
| **Colors** | Keys/tools (green), money/bags (white), chem (yellow) — matches freecam color rules |

Classification logic: `Features/ESP/LootClassify.hpp`

---

## Aimbot

- Targets / FOV / sorting / through walls / loud-only
- **Aimbot Type: Silent** (bullets track, camera stays) or **Snapping** (camera follows, with smoothing)
- Draw FOV circle (optional)

---

## Stealth tab

| Feature | What it does |
|--------|----------------|
| **Ghost Mode** | Invisible / inaudible + AI perception off + camera sight zeroed. Default **F11**. Hotkey bindable. |
| **Vault Codes** (default **F10**) | One-shot scan of `ASBZKeypadBase` / multi-code / `ASBZCodeNote`. Yellow ImGui flash ~4s + status text. No game chat/UMG. |
| **Pager HUD** | Shows answer-pager count from heist GameState (Visuals config, lives on Stealth tab) |
| **Bag Drop Zones** | Highlights secure / van / escape volumes with count vs target (+ color picker) |

Ghost Mode and Vault Codes also appear under Misc actions in code but are drawn on the **Stealth** tab.

---

## Misc — weapon / movement / camera (upstream + fork)

| Group | Options |
|-------|---------|
| **Removals** | No Spread, No Recoil, Instant Minigame, Instant Reload, Instant Melee, Auto Pistol |
| **Camera Modifiers** | Disable Shake, Disable Tilt |
| **Camera FOV** | Slider 0–150 |
| **Buffs** | Speed, Damage, Armor |
| **Rapid Fire** | Disabled / Steady / Rapid |
| **More Bullets** | Extra projectiles per shot (count slider) |
| **Super Toss** | Thrown bag speed multiplier (slider) |

---

## Misc — Client Move (noclip)

| Control | Notes |
|--------|--------|
| **Client Move** | Noclip / fly (default Mouse5). |
| **Teleport** | Online: syncs your noclip position to the server (default **Z**). Not the same as the Teleport **tab**. |
| **Move Faster** | Hold to boost fly speed (default Left Shift). |
| **Speed slider** | Base fly speed 500–5000 |
| **Auto Teleport** | On release of Client Move, sync position (online). |

---

## Misc — toggles (click-to-bind hotkey next to each)

| Feature | What it does |
|--------|----------------|
| **God Mode** | Incoming damage ×0 + HP/armor top-up. Does **not** block tasers. |
| **Infinite Ammo** | True-inf flag + mag refill. Guns only (not placeables). |
| **More Bullet Damage (×1000)** | Patches equipped weapon FireData (Nexus InstantKill-style). |
| **Carry More Bags (you + AI)** | Raises `MaxCarryBagCount` to 50 for you and AI crew. |
| **Carry More Bodies** | Auto-stash bodies you kill on your back (up to 50). **G** drops the whole pile. Works with pager/body carry fixes. |
| **No Civ / Custody Penalty** | Clears civ-kill + custody cash docks on results (solo/host best). |
| **Friendly Fire** | Host PvP path (you → them). Crash-safe hold-LMB style. |
| **Grab All (loot)** | Auto claim / floor bags. Leaves Num7 free for Lua. |
| **Grab Access** | Keycards / RFID / press badge. Leaves Num/ free for Lua. |
| **Insta Drill** | Drills / PCs / thermite / lance + cleaners + minigame. |
| **Silent Despawn Cops** | Buries `CH_BaseCop` only — not Houston / FWB / civs. |
| **3rd Person (self)** | Misc **button** (ON/OFF) + default **F9**. Camera behind your own body. **Q** = left shoulder, **E** = right shoulder, **Center** = middle. Shooting still uses first-person aim. **F10 is Vault Codes.** Don't combine with F7 FreeCam. |

Click the **None** (or key name) button next to a checkbox → press a key to bind → that key toggles the feature. Esc clears the bind. Binds save in config.

---

## Misc — Spawner (buttons + click-to-bind hotkeys)

| Button | Action |
|--------|--------|
| **Meth spawn** | Cook Off-style meth bag spawn path |
| **Van drive-in** | Escape van drive-in |
| **Green exit** | Green escape / exit path |
| **Money / results** | Money / results screen helpers |

Same bind UI as Misc toggles: click **None** → press key → key fires that one-shot.

---

## Always-on (no menu toggle)

These run whenever the DLL is loaded:

| Behavior | Notes |
|----------|--------|
| **Instant hold-F** | Pins interaction hold durations to ~instant (bags, doors, etc.). Skips zip-tie / AI order interactions so civ tie-hands still work. |
| **No Pagers** | Killed / downed guard radios never ring; pager mode forced to Pick Up so body carry still works. |
| **Unmasked jump** | Space jump restored when mask is off (AlwaysOnQoL movement patch). |

---

## Teleport tab (preset JSON)

Separate from Client Move teleport:

- Load Heist Farmer / Scout Freecam style JSON
- Prev / next spot, teleport you, teleport everyone, tour helpers

---

## Debug tab

- Session host/client, in-heist, stealth, solo flags
- Live status strings for active Misc features
- FPS readout

---

## Source files (main additions)

- `Features/ESP/ESP.*` / `LootClassify.hpp` — actor + loot ESP, pager HUD, bag zones
- `Features/Misc/GodAmmo.*`
- `Features/Misc/CarryBags.*` / `CarryBodies.*`
- `Features/Misc/NoCivPenalty.*` / `NoPagers.*`
- `Features/Misc/FriendlyFire.*`
- `Features/Misc/GrabAll.*` / `GrabAccess.*`
- `Features/Misc/InstaDrill.*` / `SilentKill.*`
- `Features/Misc/SpawnerTools.*` / `PresetTeleport.*`
- `Features/Misc/VaultCodes.*` / `GhostMode.*` / `ThirdPerson.*`
- `Features/Misc/AlwaysOnQoL.*` — jump + movement QoL
- `Features/Misc/ClientMove.*`
- `Menu.cpp` / `Menu.hpp` — tabs, UI, config save/load, hotkeys
- `Features/Features.cpp` / `DLLMain.cpp` — wiring, ProcessEvent hooks (body carry, etc.)
- `Dumper-7/SDK/` — refreshed SDK on this branch (large; build against your game patch)

---

## Not included

- Prebuilt `Payday3-Internal.dll` in git — build Release and inject locally
- Sky FreeCam `.pak` / UE4SS Lua — separate repo folder in the Moolah project
