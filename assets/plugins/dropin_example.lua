-- =========================================================================
-- Plugin: Drop-in Example (single-file template)
-- 1 plik w assets/plugins/ = 1 plugin. Wrzucasz plik -> działa.
-- Usuwasz plik -> plugin znika. Bez rekompilacji silnika.
-- Kazdy plugin dostaje WLASNY stan Lua (izolacja, brak kolizji `Plugin`).
-- Dostępne: pełny stdlib Lua + Lab.log(msg). Haki: onLoad/onUpdate/onUnload.
-- =========================================================================

Plugin = {
    id = "com.lab.dropin-example",
    displayName = "Drop-in Example",
    version = "1.0.0",
    author = "You",
    category = "Scripting & Dev",
    description = "Minimal single-file drop-in template. Copy, rename, edit.",
    dependencies = {},
    state = "Discovered"
}

local ticks = 0

function Plugin.onLoad()
    Lab.log("Drop-in Example loaded. Edit me: assets/plugins/dropin_example.lua")
    return true
end

function Plugin.onUpdate(dt)
    ticks = ticks + 1
end

function Plugin.onUnload()
    Lab.log("Drop-in Example unloaded after " .. tostring(ticks) .. " ticks.")
end

return Plugin
