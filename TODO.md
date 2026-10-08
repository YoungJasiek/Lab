# 📋 Lab Engine — Kompletny Rejestr Architektoniczny TODO i Plan Totalnego De-Hardcodingu

> **Status:** Główny dokument architektoniczny (Master Roadmap)  
> **Cel:** Przekształcenie Lab Engine z monolitycznego silnika strzelanki FPS w **w pełni uniwersalny, data-driven silnik 2D & 3D** (C++20, OpenGL 4.5+ DSA, DOD/ECS, Lua 5.4), w którym dowolny gatunek gry (platformówka, RPG, wyścigi, symulator, horror, RTS, czy showcase FPS *Frozen-Life*) jest tworzony bez modyfikacji i rekompilacji kodu silnika.

---

## 🧭 Spis Treści
1. [Brutalna Prawda: Gdzie jeszcze tkwi hardcoding?](#-brutalna-prawda-gdzie-jeszcze-tkwi-hardcoding)
2. [Obszar 1: Kamery (Koniec z wymuszonym FPS i zacinającym się TPP)](#-obszar-1-kamery-koniec-z-wymuszonym-fps-i-zacinającym-się-tpp)
3. [Obszar 2: Bronie i Mechaniki Walki (Ekstrakcja z Rdzenia DLL do Gry)](#-obszar-2-bronie-i-mechaniki-walki-ekstrakcja-z-rdzenia-dll-do-gry)
4. [Obszar 3: Kontroler Gracza i Ruch (Koniec ze sztywnym humanoidem)](#-obszar-3-kontroler-gracza-i-ruch-koniec-ze-sztywnym-humanoidem)
5. [Obszar 4: HUD i Interfejs (Likwidacja sztywnego paska pancerza i amunicji)](#-obszar-4-hud-i-interfejs-likwidacja-sztywnego-paska-pancerza-i-amunicji)
6. [Obszar 5: Sieć i Multiplayer (Likwidacja frLimit i deathmatchu w protokole)](#-obszar-5-sieć-i-multiplayer-likwidacja-fraglimit-i-deathmatchu-w-protokole)
7. [Obszar 6: Tryby Gry i Sesja (Likwidacja sztywnego FFA / DM / TDM)](#-obszar-6-tryby-gry-i-sesja-likwidacja-sztywnego-ffa--dm--tdm)
8. [Obszar 7: Shadery i Materiały (Koniec ze stringami GLSL w plikach C++)](#-obszar-7-shadery-i-materiały-koniec-ze-stringami-glsl-w-plikach-c)
9. [Obszar 8: Edytor i Prefaby (Likwidacja switcha i string-search "barrel")](#-obszar-8-edytor-i-prefaby-likwidacja-switcha-i-string-search-barrel)
10. [Obszar 9: Parser Mapy i Sceny (Likwidacja sztywnych tokenów w LabMap)](#-obszar-9-parser-mapy-i-sceny-likwidacja-sztywnych-tokenów-w-labmap)
11. [Obszar 10: Skrypty Lua (Pełna ekspozycja silnika dla skryptów)](#-obszar-10-skrypty-lua-pełna-ekspozycja-silnika-dla-skryptów)
12. [Harmonogram Wdrożenia: Etapy od M1 do M8](#-harmonogram-wdrożenia-etapy-od-m1-do-m8)

---

## 🔎 Brutalna Prawda: Gdzie jeszcze tkwi hardcoding?

Podczas głębokiego audytu kodu wyszło na jaw, że hardcoding to nie tylko bronie i przyciski w edytorze. **Praktycznie każdy podsystem silnika został napisany z założeniem, że jedyną istniejącą grą jest sieciowy FPS deathmatch:**

| Podsystem | Gdzie jest zaszyty w kodzie? | Co jest zaszyte na sztywno? |
|---|---|---|
| **Kamery** | `player_main.cpp:391`, `LabCamera.h` | Kamera na sztywno przyklejona do oczu gracza (`_playerPos + Vec3(0, 0.2f, 0)`), zacinająca się pętla TPP po wszystkich pędzlach mapy, brak normalnego Spring-Arma. |
| **Ruch Gracza** | `player_main.cpp:350`, `main.cpp` | Na sztywno zaszyty biegający człowiek: grawitacja `25.0f`, skok `8.5f`, prędkości `6.0f` i `12.0f`. Brak możliwości sterowania statkiem, kulą, autem czy kamerą RTS bez pisania nowego playera. |
| **HUD / UI** | `LabHUD.h:14-36` | Pancerz kombinezonu (`suitArmor`), amunicja w magazynku (`ammoClip`), rezerwa (`ammoReserve`), trafienia w głowę (`hitmarkerHeadshot`), nazwa broni (`PISTOLET`). |
| **Multiplayer** | `LabNetwork.h:33-263` | Pakiety sieciowe mają pola: `NetMsgKillEvent`, `killerId`, `killerName`, `kills`, `deaths`, `fragLimit = 25`. |
| **Tryby Gry** | `LabSession.h:6-48` | Enum `GameMode` zawiera tylko: `FFA`, `DM`, `TDM`. Brak wsparcia dla kampanii, poziomów logicznych, sandboxu czy wyścigów. |
| **Shadery** | `LabRenderer.cpp:21`, `LabDecals.cpp:11`, `LabParticles.cpp` | Wszystkie shadery OpenGL GLSL to `const char*` jako raw stringi w kodzie C++. Nie można zmodyfikować shadera bez rekompilacji silnika! |
| **Materiały** | `LabRenderer.h:42` | Brak klasy `Material`. Tekstury przekazywane jako pojedyncze ID. W edytorze silnik dosłownie szuka słowa: `if (modelPath.find("barrel")) tex = getTexture("barrel_hazard.png")`! |
| **Format Mapy** | `LabMap.cpp:73-200` | Sztywne tokeny tekstowe: `brush`, `prop`, `door`, `camera`, `light`. Nie da się dodać nowej encji do mapy bez edycji parsera w C++. |
| **Skrypty Lua** | `LabScript.cpp:124-200` | Lua nie może tworzyć encji, nie może dodawać komponentów, nie może czytać prefabs ani reagować na ogólne kolizje fizyczne. Posiada za to sztywne `lua_UnlockDoor` i `lua_AddChatMessage`. |

---

## 🎥 Obszar 1: Kamery (Koniec z wymuszonym FPS i zacinającym się TPP)

### Aktualny stan:
- W `player_main.cpp` kamera domyślnie przybiera widok oczu gracza.
- TPP w `player_main.cpp` (linie 392–412) to prowizorka: ręczne sprawdzanie `for (const auto& b : _map->brushes)` w pętli bez raycastu, co powoduje zacinanie, brak płynności i przenikanie przez obiekty niebędące pędzlami (np. propsy).
- Kąty pitch/yaw gubią się przy manipulacji myszą lub blokują na sztywno.

### Zadania do wykonania:
- [ ] **Modułowy Kontroler Kamer (`CameraController`):**
  - [ ] **Tryb 1: Third-Person Orbit (TPP):**
    - Orbitowanie myszą wokół dowolnego celu (gracz, pojazd, obiekt).
    - Regulacja dystansu kółkiem myszy (Zoom In/Out).
    - **Spring-Arm / Camera Boom z Raycastem/SphereCastem:** Jeśli ściana, sufit lub prop zasłania postać, kamera natychmiast i płynnie skraca ramię wysięgnika przed przeszkodę, eliminując przenikanie przez ściany.
    - Opcja: Camera Shoulder Offset (widok zza prawego/lewego ramienia).
  - [ ] **Tryb 2: First-Person (FPS):** Opcjonalny kontroler pierwszoosobowy dla gier, które tego wymagają, z niezależnym obrotem głowy i tłumieniem wstrząsów.
  - [ ] **Tryb 3: Top-Down / Rzut z góry / Izometria:** Płynne śledzenie w płaszczyźnie XZ z regulowanym kątem pochylenia i amortyzacją ruchu (dla RPG, strategii, gier taktycznych).
  - [ ] **Tryb 4: 2D Orthographic Platformer:** Śledzenie z deadzone (martwą strefą), wyprzedzaniem ruchu (look-ahead) i granicami mapy (Camera Bounds).
  - [ ] **Tryb 5: Free-Fly / Cinematic:** Swobodna kamera do cutscenek i prezentacji.
- [ ] **Płynne przejścia kamer (Camera Blending):**
  - [ ] Możliwość płynnej interpolacji pozycji, rotacji i FOV między dwiema kamerami w scenie (`blendToCamera(targetCam, duration, easing)`).

---

## 🔫 Obszar 2: Bronie i Mechaniki Walki (Ekstrakcja z Rdzenia DLL do Gry)

### Aktualny stan:
- `include/LabWeapon.h` w `LabWorld.dll` zawiera enum `WeaponID` (Pipe, Pistol, Shotgun, M4A4S, SG553, Minigun, PlasmaGun, Railgun, RPG) oraz sztywne ścieżki do modeli `.stl`.
- `include/LabArms.h` zawiera model rąk FPP ze specyficzną animacją rury hydraulicznej.
- `include/LabCombat.h` zawiera `bool isHeadshot` w ogólnym raycaście fizyki.
- `include/LabAI.h` zawiera `CombatBot` z zasadami gry w deathmatch.

### Zadania do wykonania:
- [ ] **Wydzielenie logiki FPS ze wszystkich bibliotek silnika:**
  - [ ] Przenieść `LabWeapon.*`, `LabArms.*`, `CombatBot` z rdzenia (`LabWorld.dll`, `LabAnimation.dll`, `LabAI.dll`) do dedykowanego projektu gry: `games/FrozenLife/` lub opcjonalnego modułu `plugins/FPSGame/`.
  - [ ] Rdzeń silnika staje się w 100% czysty od jakiejkolwiek wzmianki o broniach i strzelaninach.
- [ ] **Czysty Raycast i Kolizje:**
  - [ ] Usunąć `isHeadshot` i specyficzne tagi (`EntityTag::Medkit`, `AmmoDrop`) z `LabCombat.h`.
  - [ ] Wprowadzić uniwersalną strukturę `HitResult`: obiekt trafiony, punkt trafienia, normalna powierzchni, odległość, materiał fizyczny.

---

## 🏃 Obszar 3: Kontroler Gracza i Ruch (Koniec ze sztywnym humanoidem)

### Aktualny stan:
- W `player_main.cpp` (linie 350–390) gracz to na sztywno zakodowany człowiek biegający po podłodze:
  `_playerVel.y -= 25.0f * dt`, `_playerVel.y = 8.5f`, `speed = 6.0f / 12.0f`.
- Nie da się stworzyć gry ze statkiem kosmicznym, samochodem, toczącą się kulą czy postacią 2D bez pisania od zera całego pliku wykonywalnego.

### Zadania do wykonania:
- [ ] **Komponentowa Architektura Kontrolerów Ruchu:**
  - [ ] `CharacterMovementComponent`: konfigurowalny ruch humanoidalny (prędkość, siła skoku, grawitacja, tarcie, płynne wchodzenie na stopnie / step-height).
  - [ ] `FlyingMovementComponent`: swobodny ruch 6DOF dla statków kosmicznych i dronów.
  - [ ] `VehicleMovementComponent`: fizyczny model pojazdu kołowego.
  - [ ] `TopDownMovementComponent`: poruszanie się postaci w widoku z góry (sterowanie myszką lub WSAD w 8 kierunkach).
  - [ ] `Platformer2DMovementComponent`: fizyka platformowa 2D (skok ze zmienną wysokością, coyote time, jump buffering).

---

## 🖥️ Obszar 4: HUD i Interfejs (Likwidacja sztywnego paska pancerza i amunicji)

### Aktualny stan:
- Klasa `LabHUD` w `include/LabHUD.h` posiada sztywne pola: `suitArmor`, `ammoClip`, `ammoReserve`, `frags`, `weaponName = "PISTOLET"`, `hitmarkerHeadshot`.
- Jeśli tworzysz grę logiczną lub platformówkę, silnik ładuje elementy HUD-u ze strzelanki Quake/Half-Life.

### Zadania do wykonania:
- [ ] **Prawdziwy Płótno GUI / UI Canvas System:**
  - [ ] Przekształcenie systemu HUD w generyczne drzewo widgetów 2D: `Canvas`, `Panel`, `TextLabel`, `Image`, `ProgressBar`, `Button`.
  - [ ] Możliwość tworzenia layoutu HUD w plikach JSON (`assets/ui/hud_layout.json`) lub w edytorze `LabHUDEditor`.
  - [ ] Sterowanie właściwościami elementów UI z poziomu skryptów Lua: `UI.SetText("score_label", "Points: 100")`, `UI.SetProgress("health_bar", 0.75)`.

---

## 🌐 Obszar 5: Sieć i Multiplayer (Likwidacja fragLimit i deathmatchu w protokole)

### Aktualny stan:
- `include/LabNetwork.h` posiada na stałe zaszyte pakiety: `NetMsgKillEvent` (`killerId`, `killerName`), `kills`, `deaths`, `fragLimit = 25`.
- Biblioteka sieciowa silnika narzuca, że multiplayer to deathmatch na zabójstwa.

### Zadania do wykonania:
- [ ] **Generyczna Warstwa Sieciowa (State Replication & RPC):**
  - [ ] Oczyszczenie `LabNetwork.dll` z pojęć zabójstw, fragów i deathmatchu.
  - [ ] Wprowadzenie generycznych pakietów:
    - Replikacja stanu encji (`NetMsgEntityState`: GUID, position, rotation, velocity, user_state).
    - Zdalne wywołania procedur (`NetMsgRPC`: targetEntity, methodId, payload).
    - Dowolne dane gry przesyłane w elastycznych wiadomościach danych.
  - [ ] Tablica wyników i zasady wygranej definiowane w skrypcie/kodzie gry, a nie w protokole sieciowym silnika.

---

## 🎮 Obszar 6: Tryby Gry i Sesja (Likwidacja sztywnego FFA / DM / TDM)

### Aktualny stan:
- `include/LabSession.h` zawiera enum `GameMode { FFA, DM, TDM }` oraz pola `fragLimit = 25`, `botCount = 0`, `botDifficulty = 1`.
- Silnik nie ma pojęcia o trybach dla jednego gracza innych niż strzelanie do botów na arenie.

### Zadania do wykonania:
- [ ] **Abstrakcyjny `GameModeBase`:**
  - [ ] Architektura `GameModeBase` z cyklem życia: `OnGameStart()`, `OnPlayerJoin()`, `OnPlayerLeave()`, `OnGameEnd()`, `CheckWinCondition()`.
  - [ ] Gry definiują własne tryby (np. `PlatformerGameMode`, `RacingGameMode`, `PuzzleGameMode`, `StoryCampaignGameMode`).
  - [ ] Możliwość pisania pełnej logiki trybu gry w Lua.

---

## 🎨 Obszar 7: Shadery i Materiały (Koniec ze stringami GLSL w plikach C++)

### Aktualny stan:
- Wszystkie shadery (`defaultVertexShaderSrc`, shadery cząsteczek, post-processingu i dekali) są wpisane jako `const char*` bezpośrednio w plikach `.cpp`.
- Brak klasy `Material` – parametry i tekstury są rzucane ad-hoc jako shadery bez standaryzacji.
- Brak możliwości modyfikacji wyglądu grafiki bez rekompilacji kodu C++.

### Zadania do wykonania:
- [ ] **Zewnętrzne Pliki Shaderów (`assets/shaders/`):**
  - [ ] Wydzielenie kodu GLSL do plików: `assets/shaders/standard.vert`, `assets/shaders/standard.frag`, `postprocess.frag`, `particles.vert` itd.
  - [ ] System ładowania i kompilacji shaderów z dysku z obsługą **Hot-Reloadingu** (zmiana shadera w notatniku natychmiast odświeża obraz w działającym edytorze).
- [ ] **Wdrożenie Pełnego Systemu Materiałów (`Material`):**
  - [ ] Klasa `Material`: wskaźnik na shader, mapa tekstur (Albedo, Normal, Roughness, Metallic, AO, Emission) oraz parametry (kolor, przezroczystość, tiling).
  - [ ] Pliki materiałów w formacie JSON (`assets/materials/concrete_wall.mat`).

---

## 📦 Obszar 8: Edytor i Prefaby (Likwidacja switcha i string-search "barrel")

### Aktualny stan:
- `editor_main.cpp` (linie 2607–2850): funkcja `placePrebuilt(int prebuiltId)` ma wielki `switch` z 18 na sztywno wpisanymi encjami.
- W linii 5658 edytor dosłownie szuka stringów w nazwie pliku, żeby zgadnąć teksturę:
  `if (p.modelPath.find("barrel")) tex = getTexture("barrel_hazard.png")`.

### Zadania do wykonania:
- [ ] **System Prefabów (`assets/prefabs/*.prefab` w JSON):**
  - [ ] Każdy obiekt (skrzynka, ściana, lampa, wróg, platforma) definiowany jako plik prefabu z listą komponentów.
- [ ] **Dynamiczna Paleta w Edytorze:**
  - [ ] Usunięcie funkcji `placePrebuilt` i całego `switch`.
  - [ ] Edytor skanuje katalog `assets/prefabs/` i automatycznie buduje kategorie i przyciski w zakładce *Add Entity*.
  - [ ] Kliknięcie wczytuje prefab i dodaje go do sceny – 0 linii C++ potrzebnych do dodania nowego obiektu.

---

## 🗺️ Obszar 9: Parser Mapy i Sceny (Likwidacja sztywnych tokenów w LabMap)

### Aktualny stan:
- `LabMap.cpp` używa prymitywnego parsera tekstu ze sztywnymi instrukcjami `if (token == "brush")`, `if (token == "door")`, `if (token == "prop")`.
- Nie da się zapisać żadnego nowego typu obiektu ani komponentu bez ręcznego dopisywania kolejnych `if (token == ...)` w parserze C++.

### Zadania do wykonania:
- [ ] **Nowoczesny Format Sceny (`.labscene` w JSON):**
  - [ ] Każdy obiekt sceny to standardowa encja z listą komponentów zapisanych w JSON.
  - [ ] Dowolny komponent jest automatycznie serializowany i deserializowany.
  - [ ] Przestarzały format `.labmap` traktowany wyłącznie jako format legacy z automatycznym konwerterem do `.labscene`.

---

## 📜 Obszar 10: Skrypty Lua (Pełna ekspozycja silnika dla skryptów)

### Aktualny stan:
- `LabScript.cpp` udostępnia zaledwie kilkanaście funkcji.
- Lua nie ma możliwości:
  - Spawnowania prefabów ani nowych encji.
  - Dodawania komponentów do istniejących obiektów.
  - Odbierania zdarzeń kolizji (`OnCollisionEnter`, `OnTriggerEnter`).
  - Sterowania kamerami (poza statyczną zmianą nazwy).

### Zadania do wykonania:
- [ ] **Rozszerzenie API Lua (`LabScript`):**
  - [ ] `World.SpawnPrefab(prefabPath, x, y, z)`
  - [ ] `World.DestroyEntity(entityId)`
  - [ ] `Entity.AddComponent(entityId, componentType, params)`
  - [ ] `Camera.SetMode("TPP")`, `Camera.SetTarget(entityId)`, `Camera.SetDistance(5.0)`
  - [ ] Obsługa zdarzeń `OnCollision(hitInfo)` i `OnTrigger(otherEntityId)`.

---

## 📅 Harmonogram Wdrożenia: Etapy od M1 do M8

| Etap | Nazwa | Zakres prac |
|---|---|---|
| **M1** | **System Prefabów Data-Driven** | Likwidacja `placePrebuilt switch` i string-search w edytorze. Dynamiczne czytanie `assets/prefabs/*.prefab`. |
| **M2** | **Uniwersalny System Kamer** | Modułowy `CameraController`: bezbłędny TPP Orbit ze Spring-Armem i kolizjami, czysty FPS, Top-Down, 2D Platformer. |
| **M3** | **Uniwersalny Kontroler Postaci** | Likwidacja sztywnych prędkości/grawitacji w playerze na rzecz wymiennych komponentów ruchu (`CharacterMovement`, `Flying`, `TopDown`). |
| **M4** | **Ekstrakcja Broni i Walki z Core** | Przeniesienie `LabWeapon`, `LabArms`, `CombatBot` do projektu *Frozen-Life*. Oczyszczenie fizyki z `isHeadshot`. |
| **M5** | **Zewnętrzne Shadery i Materiały** | Wydzielenie kodu GLSL do `assets/shaders/` z hot-reloadingiem oraz wprowadzenie klasy `Material`. |
| **M6** | **Generyczny Płótno UI / HUD** | Likwidacja `LabHUD` (pancerz HEV, amunicja) na rzecz drzewa widgetów 2D sterowanych z JSON i Lua. |
| **M7** | **Generyczna Sieć i Tryby Gry** | Usunięcie `NetMsgKillEvent` i `GameMode::FFA/DM/TDM` na rzecz replikacji stanu encji i klasy `GameModeBase`. |
| **M8** | **Pełne API Skryptowe Lua** | Umożliwienie tworzenia pełnych gier w Lua (spawnowanie encji, prefaby, kolizje, logika wygranej). |

---

*Rejestr jest oficjalnym dokumentem architektonicznym projektu Lab Engine. Wszelkie przyszłe prace programistyczne muszą być zgodne z zasadą pełnej uniwersalności i decouplingiem rdzenia od konkretnych gier.*
