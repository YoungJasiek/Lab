-- ==========================================================
-- QA Magic RPG — mechanika miotel (DANE + czyste funkcje Lua)
-- Test mozliwosci silnika: lewitujace brushe + billboardy swiatla
-- + parametry lotu. Bez fizyki lotu w silniku (gap do AI prompta).
-- ==========================================================

Brooms = {
    wierzba = { hoverH = 3.6, climb = 2.0, stamina = 100.0, handling = 0.8, spot = "zachod" },
    burza = { hoverH = 5.2, climb = 3.2, stamina = 80.0, handling = 1.2, spot = "wschod" },
    sowa = { hoverH = 7.5, climb = 2.6, stamina = 120.0, handling = 1.0, spot = "patrol" },
    treningowa = { hoverH = 1.55, climb = 0.8, stamina = 200.0, handling = 0.5, spot = "stojak" }
}

FlightTuning = {
    staminaDrainPerSec = 6.0,
    staminaRegenPerSec = 10.0,
    minStaminaToTakeoff = 50.0,
    manaPerBoostSec = 8.0
}

function ClimbRate(broomId, stamina)
    local b = Brooms[broomId]
    if b == nil then
        return 0.0
    end
    if stamina <= 0.0 then
        return 0.0
    end
    return b.climb * math.min(1.0, stamina / 100.0)
end

function CanTakeoff(broomId, stamina, mana)
    local b = Brooms[broomId]
    if b == nil then
        return false
    end
    return stamina >= FlightTuning.minStaminaToTakeoff and mana > 0.0
end

function StaminaAfter(dt, stamina, boosting)
    local drain = FlightTuning.staminaDrainPerSec
    if boosting then
        drain = drain * 2.0
    end
    local s = stamina - drain * dt
    if s < 0.0 then
        s = 0.0
    end
    return s
end
