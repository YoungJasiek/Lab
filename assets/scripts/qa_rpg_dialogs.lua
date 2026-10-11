-- ==========================================================
-- QA Magic RPG — dialogi NPC (DANE, czyste funkcje Lua)
-- NPC nie sa osobnymi encjami formatu .labmap: sa reprezentowani
-- przez ogniska magii / znaczniki; po dolaczeniu NPC do silnika
-- ten plik podepnie sie 1:1 bez zmian tresci.
-- ==========================================================

NPCs = {
    wozny = {
        name = "Wozny Grobik",
        spot = "stojak treningowy (4, 1.5, 10)",
        lines = {
            "Miotly same nie lataja, uczniu. Najpierw zwoj, potem niebo.",
            "Zgubiles juz jedna? Wszystkie wroca o zmierzchu. Prawie wszystkie.",
            "Brama Polnocna puszcza tylko tych z piecioma zwojami."
        }
    },
    mistrzyni = {
        name = "Mistrzyni Lotow Vey",
        spot = "miotla B (7, 5.2, -8)",
        lines = {
            "Wznoszenie kosztuje oddech. Opadanie — tylko odwage.",
            "Trzymaj many powyzej polowy, inaczej wyladujesz w fontannie.",
            "Burza slucha tych, ktorzy nie krzycza."
        }
    },
    bibliotekarz = {
        name = "Bibliotekarz Pył",
        spot = "Brama Polnocna (0, 2, -21.4)",
        lines = {
            "Piec zwojow, zero zaklec bojowych na korytarzu. Regulamin.",
            "Sowa roznosi poczte, nie plotki. Choc plotki szybciej.",
            "Egzamin? To tylko latanie. Latanie to tylko spadanie z gracja."
        }
    }
}

function GetGreeting(npcId, scrollsCollected)
    local npc = NPCs[npcId]
    if npc == nil then
        return "..."
    end
    local idx = (scrollsCollected % #(npc.lines)) + 1
    return npc.name .. ": " .. npc.lines[idx]
end

function GetHintLine(npcId)
    local npc = NPCs[npcId]
    if npc == nil then
        return "..."
    end
    return npc.name .. " wskazuje: " .. npc.spot
end
