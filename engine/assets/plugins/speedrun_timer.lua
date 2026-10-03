-- =========================================================================
-- Plugin: Speedrun Splits & Live Telemetry Overlay
-- 1 kod 1 plugin: Standalone Lua Plugin File
-- =========================================================================

Plugin = {
    id = "com.lab.speedrun-timer",
    displayName = "Speedrun & Telemetry Splits",
    version = "1.1.0",
    author = "Lab Competitive Team",
    category = "Gameplay & HUD",
    description = "Live speedrun split timer, velocity speedometer (m/s), sector segment comparisons, and delta (+/-) visual HUD overlays.",
    dependencies = {},
    state = "Enabled"
}

local timerRunning = false
local elapsedTime = 0.0
local splits = {}

function Plugin.onLoad()
    print("[Plugin: Speedrun & Telemetry Splits] Initialized. HUD telemetry overlay ready.")
    return true
end

function Plugin.onUpdate(dt)
    if timerRunning then
        elapsedTime = elapsedTime + dt
        if HUD and HUD.setText then
            HUD.setText("SpeedrunTimer", string.format("%02d:%05.2f", math.floor(elapsedTime / 60), elapsedTime % 60))
        end
    end
end

function Plugin.onUnload()
    timerRunning = false
    print("[Plugin: Speedrun & Telemetry Splits] Unloaded.")
end

function Plugin.start()
    timerRunning = true
end

function Plugin.pause()
    timerRunning = false
end

function Plugin.reset()
    timerRunning = false
    elapsedTime = 0.0
    splits = {}
end

function Plugin.recordSplit(name)
    table.insert(splits, { name = name, time = elapsedTime })
end

return Plugin
