# MSZ Modding API (C++ / Unity IL2CPP)

# ⚠️ This project is still conisdered WIP and alot has to be added. It works but its limited.

A lightweight **C++ modding API** for a Unity (IL2CPP) game, built for quickly making mods.

## Highlights
- **No manual offsets**: address/offset scanning is **automatic** (auto-updating), It will always work unless the game developer changes method names.
- **Simple API surface** for UI + common unity interface
- Works great for internal tools, prototyping, and sandbox mods.

## Features
### UI (wrapper)
- Register your own menus
- Toggle / open / close menu state
- Basic widgets (text, buttons, checkbox, slider, input)

### Unity helpers
- Main camera access
- Ray / raycast helpers
- Transform & Rigidbody helpers
- Time scale control
- Scene loading
& more to come

### Safety / convenience
- Main-thread task queue (`RunOnMainThread`)

## Notes
- Offsets are handled automatically — **no updating them manually**.
- Project is still in progress; expect changes.
