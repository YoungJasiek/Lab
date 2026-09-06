# 📜 Lua 5.4 Scripting & Gameplay Balancing

The **Lab Scripting Subsystem** (`LabScript`) embeds the official **Lua 5.4.6** engine to cleanly decouple gameplay balance, player attributes, weapon parameters, and security credentials from the native C++ engine core.

---

## 1. Architectural Decoupling

Following classic professional game engine standards:
- **Native C++20 Core:** Handles low-level heavy lifting (OpenGL 4.5 DSA, vertex skinning, physics step integration, network sockets, collision detection).
- **Lua 5.4 Environment:** Dictates all high-level gameplay balance and rules.
- **Zero External DLLs:** Lua 5.4.6 source code is compiled statically into `LabEngineLib` via `external/lua/onelua.c`.

---

## 2. Gameplay Script: `game_mechanics.lua`

All gameplay balance parameters are externalized in `assets/scripts/game_mechanics.lua`:

### Player Combat Rules
```lua
PlayerRules = {
    maxHealth = 100.0,
    maxArmor = 100.0,
    startHealth = 100.0,
    startArmor = 25.0,
    armorAbsorption = 0.65,      -- 65% of incoming damage absorbed by armor
    headshotMultiplier = 2.5,
    respawnTimerSeconds = 3.0,
    explosiveBarrelDamageMult = 1.85
}
```

### Complete Arsenal Configuration Table
```lua
Weapons = {
    [0] = { name = "Lead Pipe",         damage = 35.0, fireRate = 0.45, range = 2.5,   clipSize = 1,   spread = 0.000 },
    [1] = { name = "Tactical Pistol",   damage = 22.0, fireRate = 0.20, range = 50.0,  clipSize = 12,  spread = 0.006 },
    [2] = { name = "Combat Shotgun",    damage = 14.0, fireRate = 0.75, range = 30.0,  clipSize = 8,   spread = 0.045 },
    [3] = { name = "M4A4-S Tactical",   damage = 26.0, fireRate = 0.11, range = 65.0,  clipSize = 30,  spread = 0.008 },
    [4] = { name = "SG553 Rifle",       damage = 30.0, fireRate = 0.13, range = 75.0,  clipSize = 30,  spread = 0.007 },
    [5] = { name = "Rotary Minigun",    damage = 16.0, fireRate = 0.065,range = 55.0,  clipSize = 100, spread = 0.035 },
    [6] = { name = "Plasma Repeater",   damage = 32.0, fireRate = 0.16, range = 50.0,  clipSize = 25,  spread = 0.012 },
    [7] = { name = "Kinetic Railgun",   damage = 85.0, fireRate = 1.10, range = 120.0, clipSize = 5,   spread = 0.001 },
    [8] = { name = "RPG Launcher",      damage = 95.0, fireRate = 1.40, range = 80.0,  clipSize = 1,   spread = 0.005 }
}
```

### Dynamic Damage Calculation Function
Damage calculations are executed directly within the Lua VM:
```lua
function CalculateDamage(rawDamage, isHeadshot, currentArmor)
    local finalDamage = rawDamage
    if isHeadshot then
        finalDamage = finalDamage * PlayerRules.headshotMultiplier
    end

    local absorbed = 0
    if currentArmor > 0 then
        absorbed = finalDamage * PlayerRules.armorAbsorption
        if absorbed > currentArmor then
            absorbed = currentArmor
        end
    end

    local healthDmg = finalDamage - absorbed
    local armorDmg = absorbed

    return healthDmg, armorDmg
end
```

---

## 3. Biometric Security & Retinal Scanner Credentials

Security clearances for interactive consoles are authored in Lua:
```lua
BiometricCredentials = {
    ["Dr. Vance"] = { clearance = 5, sector = "Cryo Research", matchThreshold = 0.92 },
    ["Security Chief"] = { clearance = 4, sector = "Armory & Perimeter", matchThreshold = 0.88 },
    ["Maintenance Tech"] = { clearance = 2, sector = "Ventilation & Power", matchThreshold = 0.75 }
}

function VerifyBiometricScan(userLabel, requiredAuth)
    local cred = BiometricCredentials[userLabel]
    if not cred then return false, 0.0 end
    if userLabel == requiredAuth or cred.clearance >= 5 then
        return true, cred.matchThreshold
    end
    return false, cred.matchThreshold * 0.5
end
```

---

## 4. Hot-Reloading in Development

Designers can adjust numbers in `game_mechanics.lua` and trigger hot-reloading via `LabScript::reload()` or pressing the console hotkey. The game instantly re-reads weapon damage, clip sizes, and player rules without recompiling the project or restarting matches.
