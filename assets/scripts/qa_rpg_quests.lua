-- ==========================================================
-- QA Magic RPG — questy (DANE fabularne, czyste funkcje Lua)
-- Nie wymagane przez silnik; czytane przez skrypty misji /
-- narzedzia. 5 zwojow = 5 "kart" z Mission.cardCount.
-- Lokalizacje odpowiadaja encjom w qa_magic_rpg.labmap.
-- ==========================================================

QuestScrolls = {
    { id = 1, name = "Zwoj Dziedzinca", where = "stojak treningowy (4, 1.5, 10)", hint = "Zacznij przy stojaku z miotla D." },
    { id = 2, name = "Zwoj Wierzby", where = "miotla A, hover (-7, 3.6, -3)", hint = "Spojrzenie w gore, na zachod." },
    { id = 3, name = "Zwoj Burzy", where = "miotla B, hover (7, 5.2, -8)", hint = "Najwyzsza miotla wschodu." },
    { id = 4, name = "Zwoj Sowy", where = "miotla C, patrol (0, 7.5, 3)", hint = "Prosto nad glowa, patrz w niebo." },
    { id = 5, name = "Zwoj Bramy", where = "Brama Polnocna (0, 2, -21.4)", hint = "Podejdz do bramy — otworzy sie sama." }
}

QuestStages = {
    [0] = "Porozmawiaj z Woznym przy stojaku.",
    [1] = "Znajdz Zwoj Wierzby (miotla A).",
    [2] = "Znajdz Zwoj Burzy (miotla B).",
    [3] = "Znajdz Zwoj Sowy (miotla C).",
    [4] = "Odbierz Zwoj Bramy i przejdz przez Brame Polnocna.",
    [5] = "Egzamin zdany!"
}

function QuestStageText(scrollsCollected)
    local s = math.min(scrollsCollected, 5)
    return QuestStages[s]
end

function NextHint(scrollsCollected)
    local nxt = QuestScrolls[scrollsCollected + 1]
    if nxt == nil then
        return "Wszystkie zwoje zebrane. Do bramy!"
    end
    return nxt.hint
end

function QuestProgress(scrollsCollected, timeLeft)
    if scrollsCollected >= 5 then
        return "WIN", "Egzamin zdany! Miotla czeka."
    end
    if timeLeft <= 0.0 then
        return "LOSE", "Czas minal. Wroc o zmierzchu."
    end
    return "RUNNING", QuestStageText(scrollsCollected)
end
