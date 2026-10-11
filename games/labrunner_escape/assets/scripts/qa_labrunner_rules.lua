-- ==========================================================
-- QA LabRunner: Kryo-Ucieczka — zasady gry (Lua 5.4)
-- Plik DANYCH, nie kodu silnika. Czytany przez LabScript.
-- Balans celowo prosty: 1 misja, 3 karty, limit 300 s.
-- ==========================================================

PlayerRules = {
    maxHealth = 100.0,
    startSuitArmor = 0.0,
    armorAbsorptionRatio = 0.0,
    respawnTime = 2.0,
    barrelDamageMultiplier = 0.75
}

Mission = {
    name = "Kryo-Ucieczka",
    cardCount = 3,
    timeLimit = 300.0,
    startText = "Zbierz 3 karty i dotrzyj do wrot!",
    winText = "Ucieczka udana!",
    loseText = "Czas minal. Sprobuj ponownie."
}

Pickups = {
    MedkitHeal = 25.0,
    AmmoBoxAmount = 12,
    WeaponPadRespawnTime = 30.0
}

-- Progresja broni dopasowana do weapon_spawnerow z mapy (ID 0/1/2 istnieja w silniku)
Weapons = {
    Pipe = { id = 0, name = "RURA", damage = 35.0, range = 2.6, fireRate = 0.55, isMelee = true, slot = 1 },
    Pistol = { id = 1, name = "PISTOLET", damage = 18.0, fireRate = 0.25, clipSize = 12, reserve = 60, slot = 2 },
    Shotgun = { id = 2, name = "STRZELBA", damage = 10.0, fireRate = 0.85, clipSize = 6, reserve = 30, slot = 3 }
}

-- Czysta funkcja Lua: bez odwolania do C++, testowalna w izolacji
function CalculateDamage(incomingDamage, currentArmor, currentHealth)
    local finalDmg = incomingDamage
    local isLethal = (currentHealth - finalDmg <= 0.0)
    return finalDmg, 0.0, isLethal
end

function MissionProgress(cardsCollected, timeLeft)
    if cardsCollected >= Mission.cardCount then
        return "WIN", Mission.winText
    end
    if timeLeft <= 0.0 then
        return "LOSE", Mission.loseText
    end
    return "RUNNING", Mission.startText
end

-- Hooki silnika (wywolywane TYLKO jesli silnik je podpina; brak nowych API)
function OnUpdate(dt)
end

function OnTriggerEnter(entityId, otherEntityId)
end
