---
docid: LF01
name: Lua 5.4 Scripting & Dynamic Runtime
related_docs: [LA01, LB01, LE01]
tags: [scripting, lua, vm, runtime, ide, bytecode]
---

# 📜 Lua 5.4 Scripting & Dynamic Runtime

The **Lab Scripting Subsystem** (`LabScript`, `ScriptEngine`) embeds the official **Lua 5.4.6** engine to decouple high-level gameplay rules, user interface states, input handlers, and entity behaviors from the native C++20 engine core.

---

## 1. Architectural Decoupling

Following modern game engine standards:
- **Native C++20 Core:** Handles low-level heavy lifting (OpenGL 4.5+ DSA, mesh skinning, physics integration, memory management, network sockets, file VFS).
- **Lua 5.4 Runtime:** Dictates game logic, state machines, entity callbacks, and balance tables.
- **Zero External DLLs:** Lua 5.4.6 source code is compiled statically into `LabEngineLib` via `external/lua/onelua.c`.

---

## 2. Dynamic Entity Scripting Callbacks

Entities can attach Lua event handlers triggered by the engine lifecycle:

```lua
-- Called once when entity is registered in the scene
function OnInit(entityId)
    print("Entity initialized: " .. tostring(entityId))
end

-- Called every simulation tick with delta time (dt)
function OnUpdate(entityId, dt)
    local x, y, z = Entity.getPosition(entityId)
    -- Process gameplay logic or movement
end

-- Triggered on volume overlaps and physical contacts
function OnTriggerEnter(entityId, otherEntityId)
    print("Collision detected between " .. tostring(entityId) .. " and " .. tostring(otherEntityId))
end
```

---

## 3. Dynamic UI & HUD State Bridge

Lua scripts can dynamically inspect and manipulate in-game UI and scene widgets:

```lua
-- Update HUD widgets in real time from Lua
HUD.setText("TimerWidget", "02:45")
HUD.setValue("PlayerScore", 1337.0)
HUD.setVisible("TimerWidget", true)
```

Engine components and custom game code query HUD properties with `Lab::ScriptEngine::getHUDText()`, `getHUDValue()`, and `isHUDVisible()`.

---

## 4. Unified Input System Integration (`LabInput`)

The engine's unified input action mapping (`LabInput.h`) exposes direct bindings to the Lua runtime:

```lua
-- Bind actions to keys or gamepad buttons
Input.bindAction("Jump", "Space")
Input.bindAction("Jump", "Gamepad_A")
Input.bindAction("Interact", "E")
Input.bindAction("Interact", "Gamepad_X")

function OnActionTriggered(actionName)
    if actionName == "Jump" then
        print("Jump action triggered")
    elseif actionName == "Interact" then
        print("Interact action triggered")
    end
end
```

---

## 5. Hot-Reloading in Development

Designers can adjust script files on disk and trigger instant hot-reloading via `LabScript::reload()` or from the editor. The engine re-executes the environment and updates active gameplay parameters without recompiling the project or restarting the application.

---

## 6. In-Engine Scripting IDE (`LabEditor`)

LabEditor includes a full-viewport, distraction-free **Scripting IDE** (`View -> Workspaces -> Scripting > Code Editor IDE`):
- **Full Viewport Utilization:** Replaces the 3D scene view with a code editing surface.
- **Interactive Text Buffer:** Supports multiline editing, cursor navigation, home/end, backspace line merging, Enter key indentation with auto-indent preservation, and clipboard operations.
- **Live Syntax Highlighting:** Distinguishes Lua keywords (`function`, `local`, `if`, `then`, `end`), numerical constants, string literals, and engine API global bridges.
- **Direct Save & Execution:** Save to disk (`Ctrl+S`) and test live in the active engine state.

---

## 7. Bytecode Compilation & Protection (`.luac`)

For shipping titles, Lab provides an integrated ahead-of-time bytecode compiler:
- **`compileLuaToBytecode(srcPath, dstPath, stripDebug)`:** Compiles `.lua` text files into standard Lua 5.4 binary chunks.
- **Debug Stripping:** Removes all source paths, local symbol tables, and line numbers (`stripDebug = true`) to safeguard intellectual property.
- **Fast Binary Execution:** `Lab::ScriptEngine::executeBytecode()` executes `.luac` chunks with zero runtime parsing overhead.
