-- ==========================================================
-- QA Magic RPG: Akademia Latajacych Miotel — reguly misji
-- Plik DANYCH ladowany przez LabPlayer (wzorcowy format Mission).
-- Quest fabularny w qa_rpg_quests.lua; tu tylko balans wymagany
-- przez silnik: PlayerRules / Mission / Weapons / funkcje.
-- ==========================================================

PlayerRules = {
    maxHealth = 100.0,
    startSuitArmor = 0.0,
    armorAbsorptionRatio = 0.0,
    respawnTime = 3.0,
    barrelDamageMultiplier = 0.5
}

Mission = {
    name = "Akademia Latajacych Miotel",
    cardCount = 5,
    timeLimit = 600.0,
    startText = "Zbierz 5 zwojow i zglos sie do Bramy Polnocnej!",
    winText = "Egzamin zdany! Miotla czeka.",
    loseText = "Czas minal. Wroc o zmierzchu."
}

Pickups = {
    MedkitHeal = 25.0,
    AmmoBoxAmount = 12,
    WeaponPadRespawnTime = 30.0
}

-- Ogniska magii (weapon_spawner ID 0/1/2 z mapy), nazwy fabularne
Weapons = {
    Pipe = { id = 0, name = "KOSTUR WOZNEGO", damage = 30.0, range = 2.6, fireRate = 0.55, isMelee = true, slot = 1 },
    Pistol = { id = 1, name = "ROZDZKA TRENINGOWA", damage = 15.0, fireRate = 0.30, clipSize = 12, reserve = 60, slot = 2 },
    Shotgun = { id = 2, name = "ROZDZKA BURZY", damage = 9.0, fireRate = 0.85, clipSize = 6, reserve = 30, slot = 3 }
}

function CalculateDamage(incomingDamage, currentArmor, currentHealth)
    local finalDmg = incomingDamage
    local isLethal = (currentHealth - finalDmg <= 0.0)
    return finalDmg, 0.0, isLethal
end

function MissionProgress(scrollsCollected, timeLeft)
    if scrollsCollected >= Mission.cardCount then
        return "WIN", Mission.winText
    end
    if timeLeft <= 0.0 then
        return "LOSE", Mission.loseText
    end
    return "RUNNING", Mission.startText
end

function OnUpdate(dt)
    -- Engine lifecycle tick hook for mission logic
end

function OnTriggerEnter(entityId, otherEntityId)
    -- Hook called by engine when triggers fire, doors open or pickups are collected
    if otherEntityId == 1 then
        -- Weapon spawner / card pickup event
        if HUD and HUD.setText then
            HUD.setText("rpg.quest_status", "ZWOJE 1/5")
        end
    end
end
