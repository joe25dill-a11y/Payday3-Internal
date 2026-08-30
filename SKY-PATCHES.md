# Sky patches on Omega172 Payday3-Internal v2

Base: Omega172 `v2` (UE 5.5.4 PD3). This tree is **Sky’s working fork**, not upstream.

## pSilent / Silent aim
Upstream hooks called originals and never wrote overrides.

Sky:
- Hook `APlayerController::GetPlayerViewPoint` @ **0x106** (Omega v2 index)
- Learn fire-path return addresses while shooting, then gate overrides to those only (camera stays put)
- Before READY: overrides apply so bullets work (camera may twitch while learning)
- After READY: fire-path only = client silent / pSilent
- Bone location + look-at rotation on fire path
- Aimbot hotkey defaults to **Always On** when unbound
- On-screen status: `LEARNING` / `READY` + hit counter
- Eyes viewpoint left alone (no crashy GetViewPoint probing)

## Wallbang
- Aimbot + Player → Weapon Mods → **Wallbang (Through Walls)**
- Raises `MaximumPenetrationCount` / penetrate flags on current FireData
- Auto-disables aimbot visibility check while wallbang is on

## Noclip / ClientMove
New: `Source/Internal/Features/Misc/ClientMove.*`

- Sidebar **Noclip**
- Hold hotkey + Enabled → collision off, `MOVE_Flying`, WASD fly
- Faster / Sync hotkeys + Auto Sync On Release (`Server_StartTraversal`)

## Build
```
xmake config -m release -y
xmake build
```
DLL: `Build/Release/Payday-Internal-v2/Payday-Internal-v2.dll`
