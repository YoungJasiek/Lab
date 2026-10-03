-- =========================================================================
-- Plugin: Branching Dialogue & Quest System
-- Standalone Modular Lua Plugin
-- =========================================================================

Plugin = {
    id = "com.lab.dialogue-quest",
    displayName = "Dialogue & Quest System",
    version = "2.1.0",
    author = "Lab Narrative Systems",
    category = "Scripting & Dev",
    description = "Interactive NPC dialogue branches, quest objective trackers, event triggers, and Lua script hook integration.",
    dependencies = {},
    state = "Enabled"
}

local activeQuests = { "Infiltrate Facility Beta", "Restore Emergency Power" }

function Plugin.onLoad()
    print("[Plugin: Dialogue & Quest System] Initialized. 2 active quests registered.")
    return true
end

function Plugin.onUpdate(dt)
    -- Tick dialogue and objective timers
end

function Plugin.onUnload()
    print("[Plugin: Dialogue & Quest System] Unloaded.")
end

function Plugin.getQuests()
    return activeQuests
end

return Plugin
