# QA Magic RPG: Akademia Latajacych Miotel (test mozliwosci silnika)

RPG zbudowane w 100% z danych (0 zmian kodu C++). Kamera TPP to tez
dane: encja `camera Kamera_TPP` (follow: player, offset znad ramienia).

## Pliki (dane)
- `assets/maps/qa_magic_rpg.labmap` — dziedziniec: mury z gzymsami,
  4 wieze z diamentowymi czapami (`rotation 0 45 0`), luk Bramy Polnocnej,
  choragwie, fontanna (tafla `cryo_glass.bmp`), lawki, 4 miotly CSG
  (3 hover + treningowa, tokeny `name` + `script`), 3 ogniska magii,
  4 pochodnie z plomieniami-billboardami, 2 znaczniki `!`, kamera TPP
- `assets/scripts/qa_rpg_rules.lua` — jedyny plik ladowany przez silnik
  (Mission 5 zwojow / 600 s + bronie-rozdzki + funkcje)
- `assets/scripts/qa_rpg_quests.lua` — 5 zwojow, stage, hinty (czyste funkcje)
- `assets/scripts/qa_rpg_dialogs.lua` — 3 NPC x 3 kwestie (czyste funkcje)
- `assets/scripts/qa_rpg_brooms.lua` — 4 miotly + tuning lotu (czyste funkcje)
- `assets/scripts/qa_rpg_wiring.lua` — manifest: element -> skrypt -> hooki
  (od nowej wersji natywne tokeny `name`/`script`/`rotation` w .labmap)
- `assets/hud_projects/magic_rpg.labhud` — preset RPG: ZWOJE, HP, MANA,
  rozdzka, dialog, timer (tylko skladnia Card/Label/Crosshair)
- `games/magic_rpg/game.config` — scene + hud + rules (auto-detect silnika)

## Start
```powershell
.\build\Release\LabPlayer.exe --config games/magic_rpg/game.config
# albo: --scene assets/maps/qa_magic_rpg.labmap (auto-detect po games/)
```

## Petla testowa (jak czlowiek)
1. Start (0,1.8,12) -> stojak (4,1.5,10) [NPC Wozny, zwój 1].
2. Miotla A (-7,3.6,-3) -> B (7,5.2,-8) -> C (0,7.5,3).
3. Brama Polnocna (0,2,-21.4) otwiera sie w 5 m.
4. HUD: ZWOJE mission.cards, HP player.health, timer match.timer.
