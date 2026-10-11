-- ==========================================================
-- QA Magic RPG — MANIFEST PODPIEC (element mapy -> skrypt)
-- Od commita kamery: format .labmap MA tokeny `name`, `script`
-- i `rot/rotation` (LabMap.cpp je czyta) — ten plik jest
-- czytelnym indeksem tego, co mapa podpina natywnie.
-- ==========================================================

ElementScriptWiring = {
    { element = "Miotla_A_Wierzba", locator = "brush (-7, 3.6, -3)", script = "assets/scripts/qa_rpg_brooms.lua", hooks = { "ClimbRate", "CanTakeoff" } },
    { element = "Miotla_B_Burza", locator = "brush (7, 5.2, -8)", script = "assets/scripts/qa_rpg_brooms.lua", hooks = { "ClimbRate", "StaminaAfter" } },
    { element = "Miotla_C_Sowa", locator = "brush (0, 7.5, 3)", script = "assets/scripts/qa_rpg_brooms.lua", hooks = { "ClimbRate" } },
    { element = "Miotla_D_Treningowa", locator = "brush (4, 1.55, 10)", script = "assets/scripts/qa_rpg_brooms.lua", hooks = { "CanTakeoff" } },
    { element = "NPC_Wozny", locator = "stojak (4, 1.5, 10)", script = "assets/scripts/qa_rpg_dialogs.lua", hooks = { "GetGreeting" } },
    { element = "NPC_Mistrzyni", locator = "miotla B (7, 5.2, -8)", script = "assets/scripts/qa_rpg_dialogs.lua", hooks = { "GetGreeting" } },
    { element = "NPC_Bibliotekarz", locator = "brama (0, 2, -21.4)", script = "assets/scripts/qa_rpg_dialogs.lua", hooks = { "GetHintLine" } },
    { element = "Quest_Zwoje", locator = "5x sprite_2d / weapon_spawner", script = "assets/scripts/qa_rpg_quests.lua", hooks = { "QuestProgress", "NextHint" } },
    { element = "Misja_Glowna", locator = "game.config rules", script = "assets/scripts/qa_rpg_rules.lua", hooks = { "MissionProgress", "CalculateDamage" } }
}

HUDPushWiring = {
    { hudKey = "rpg.mana", from = "qa_rpg_brooms.lua:FlightTuning", note = "silnik pokaze pusto dopoki Lua nie ustawi setHUDText" },
    { hudKey = "rpg.xp", from = "qa_rpg_quests.lua:QuestStageText", note = "jak wyzej" },
    { hudKey = "rpg.dialog", from = "qa_rpg_dialogs.lua:GetGreeting", note = "jak wyzej; fallback statyczny w .labhud" }
}
