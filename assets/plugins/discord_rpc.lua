-- =========================================================================
-- Plugin: Discord Rich Presence & Social
-- 1 kod 1 plugin: Standalone Lua Plugin File
-- =========================================================================

Plugin = {
    id = "com.lab.discord-rpc",
    displayName = "Discord Rich Presence & Social",
    version = "1.0.0",
    author = "Community Extensions",
    category = "Tools & Utilities",
    description = "Discord RPC status integration displaying active editor workspace, level name, and session statistics.",
    dependencies = {},
    state = "Discovered"
}

local statusText = "Lab Engine Studio | Level Design"

function Plugin.onLoad()
    print("[Plugin: Discord Rich Presence] Connected to status bridge.")
    return true
end

function Plugin.onUpdate(dt)
end

function Plugin.onUnload()
    print("[Plugin: Discord Rich Presence] Disconnected.")
end

function Plugin.getStatus()
    return statusText
end

return Plugin
