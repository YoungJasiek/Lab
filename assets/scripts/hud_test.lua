-- ============================================================================
-- Lab Engine - Example HUD Logic Script (Lua 5.4)
-- File: assets/scripts/hud_test.lua
-- Used for testing dynamic HUD widgets, data bindings, and interactive events.
-- ============================================================================

HUDTest = {}

-- Mock Game State (for testing without a live dedicated server)
HUDTest.State = {
    health = 100,
    armor = 50,
    clip = 30,
    reserve = 180,
    kills = 5,
    timer = 480,
    stamina = 1.0,
    isLowHealth = false
}

-- ----------------------------------------------------------------------------
-- 1. UPDATE TICK (Called every frame for elements with lua_update / luaCustom)
-- ----------------------------------------------------------------------------

--- Update Player Health Display with Color Warning
--- @param elem table The HUD element being updated
function HUDTest.UpdateHealth(elem)
    local hp = HUDTest.State.health
    elem.text = tostring(math.floor(hp))
    
    if hp <= 25 then
        -- Critical HP: Pulse bright red
        elem.color = {1.0, 0.15, 0.15}
        HUDTest.State.isLowHealth = true
    elseif hp <= 50 then
        -- Low HP: Amber / Orange warning
        elem.color = {1.0, 0.65, 0.1}
    else
        -- Healthy: Tactical Gold
        elem.color = {0.98, 0.78, 0.08}
    end
end

--- Update Ammo Counter with "LOW" and "EMPTY" states
--- @param elem table
function HUDTest.UpdateAmmo(elem)
    local clip = HUDTest.State.clip
    local reserve = HUDTest.State.reserve
    
    if clip == 0 then
        elem.text = "EMPTY! [R]"
        elem.color = {1.0, 0.2, 0.2}
    elseif clip <= 5 then
        elem.text = clip .. " / " .. reserve .. " (LOW)"
        elem.color = {1.0, 0.7, 0.1}
    else
        elem.text = clip .. " / " .. reserve
        elem.color = {0.3, 0.85, 1.0}
    end
end

--- Update Match Countdown Timer (MM:SS)
--- @param elem table
function HUDTest.UpdateTimer(elem)
    local t = HUDTest.State.timer
    local minutes = math.floor(t / 60)
    local seconds = math.floor(t % 60)
    elem.text = string.format("%02d:%02d", minutes, seconds)
    
    if t < 60 then
        elem.color = {1.0, 0.3, 0.3} -- Last minute warning
    end
end

--- Update Progress Bar (Stamina / Health bar fill)
--- @param elem table
function HUDTest.UpdateHealthBar(elem)
    local ratio = HUDTest.State.health / 100.0
    elem.progressValue = math.max(0.0, math.min(1.0, ratio))
    
    if ratio < 0.25 then
        elem.color = {1.0, 0.2, 0.2}
    else
        elem.color = {0.2, 0.85, 0.35}
    end
end

-- ----------------------------------------------------------------------------
-- 2. ON-CLICK ACTIONS (Called when button or interactive widget is clicked)
-- ----------------------------------------------------------------------------

--- Respawn / Heal Button Action
function HUDTest.OnRespawnClick()
    print("[HUD Lua] Respawn clicked! Resetting player status...")
    HUDTest.State.health = 100
    HUDTest.State.clip = 30
    HUDTest.State.armor = 50
end

--- Reload Weapon Simulation
function HUDTest.OnReloadClick()
    print("[HUD Lua] Reload clicked!")
    HUDTest.State.clip = 30
    HUDTest.State.reserve = math.max(0, HUDTest.State.reserve - 30)
end

--- Test Damage Simulation (Take 15 dmg)
function HUDTest.OnTakeDamage()
    HUDTest.State.health = math.max(0, HUDTest.State.health - 15)
    print("[HUD Lua] Player took damage! Current HP: " .. HUDTest.State.health)
end

return HUDTest
