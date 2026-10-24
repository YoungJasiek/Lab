-- ============================================================================
-- Addon: CoD Tactical Entrance Intro (addons/cod_intro/scripts/startup.lua)
-- Demonstrates Lua Startup & Session Hooks in Lab Engine
-- ============================================================================

function OnGameStartup()
    if HUD and HUD.SetText then
        HUD.SetText("cod_intro_line1", "OPERATION: FROZEN-LIFE // TACTICAL INSERTION")
        HUD.SetText("cod_intro_line2", "DAY 4 - 04:18:22 - SECTOR 14 CRYO-CONTAINMENT LAB")
        HUD.SetText("cod_intro_line3", "SGT. VANCE // SITE-14 INTERNAL SECURITY FORCE")
        HUD.SetText("cod_intro_status", "ACTIVE")
    end
    if HUD and HUD.SetValue then
        HUD.SetValue("cod_intro_duration", 4.5)
    end
end

-- Execute immediately when loaded as a startup script
OnGameStartup()
