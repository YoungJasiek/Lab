# QA LabRunner: Kryo-Ucieczka (gra w 100% z danych LabSDK, 0 zmian kodu)

Tester QA, 10 lat exp. Gra zbudowana WYLACZNIE narzedziami silnika:
LabEditor (3D World) + formaty .labmap / .lua / .labhud + LabPlayer.

## Pliki gry (dane, nie kod C++)
- `assets/maps/qa_labrunner_escape.labmap` — arena: start, schody platformowe, drzwi, 3 spawny broni, 3 swiatla, sprite wyjscia
- `assets/scripts/qa_labrunner_rules.lua` — balans: PlayerRules / Mission(3 karty, 300 s) / Weapons(ID 0-2) + funkcje CalculateDamage, MissionProgress
- `assets/hud_projects/labrunner_escape.labhud` — HUD: KARTY 0/3, HEALTH, bron, timer, crosshair, pasek sterowania
- `games/labrunner_escape/game.config` — ten folder (tytul + scena startowa)

## Uruchomienie (bez kompilacji)
```powershell
# z katalogu Lab/ :
.\build\Release\LabPlayer.exe --scene assets/maps/qa_labrunner_escape.labmap --title "QA LabRunner: Kryo-Ucieczka"
# albo: skopiuj game.config do katalogu z LabPlayer.exe i odpal bez flag
```

## Sterowanie (jak w LabPlayer)
WASD ruch, SPACJA skok, SHIFT sprint, mysz rozgladanie, ESC pauza (ESC + Q wyjscie).

## Cel gry / plan testu
1. Start (0,1.8,8) -> schody (0,0.8,4) -> (0,1.6,0) -> platforma (0,2.4,-4).
2. Drzwi security_gate (0,1.75,-8) maja otworzyc sie w promieniu 4.5 m.
3. Bronie: PISTOLET (2.5,0,8) -> STRZELBA (-3,1,0) -> RURA (0,2.8,-4).
4. HUD: KARTY `mission.cards`, ZDROWIE `player.health`, TIMER `match.timer`.
