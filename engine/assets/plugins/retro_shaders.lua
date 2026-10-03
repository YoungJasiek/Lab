-- =========================================================================
-- Plugin: Retro CRT & Post-Process Shaders FX
-- Standalone Modular Lua Plugin
-- =========================================================================

Plugin = {
    id = "com.lab.retro-fx",
    displayName = "Retro CRT & Post-Process FX",
    version = "2.0.1",
    author = "Lab VFX Group",
    category = "Rendering & VFX",
    description = "Retro style shader effects suite: CRT scanlines, chromatic aberration, vignette, pixelation filter, and film grain simulation.",
    dependencies = {},
    state = "Enabled"
}

local scanlinesEnabled = true
local pixelScale = 2

function Plugin.onLoad()
    print("[Plugin: Retro CRT & Post-Process FX] Initialized. Viewport shaders active.")
    return true
end

function Plugin.onUpdate(dt)
    -- Update shader scanline phase
end

function Plugin.onUnload()
    print("[Plugin: Retro CRT & Post-Process FX] Unloaded.")
end

function Plugin.toggleScanlines()
    scanlinesEnabled = not scanlinesEnabled
    print("[Retro VFX] Scanlines toggled: " .. tostring(scanlinesEnabled))
    return scanlinesEnabled
end

return Plugin
