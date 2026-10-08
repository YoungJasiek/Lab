# 📋 Lab Engine — Architektoniczny Rejestr TODO i Plan De-Hardcodingu

> **Status:** Aktywny rejestr architektoniczny  
> **Wizja Silnika:** W pełni uniwersalny, modularny silnik 2D i 3D (C++20, OpenGL 4.5+ DSA, DOD/ECS, Lua 5.4), w którym gry (takie jak *Frozen-Life*, platformówki 2D, wyścigi, symulatory czy RPG) są wymiennymi pakietami danych/skryptów, a nie zaszytym na stałe kodem w rdzeniu.

---

## 🧭 Spis Treści
1. [Manifest Architektoniczny](#-manifest-architektoniczny)
2. [Priorytet 1: Uniwersalny System Kamer (Koniec z Wymuszonym FPS)](#-priorytet-1-uniwersalny-system-kamer-koniec-z-wymuszonym-fps)
3. [Priorytet 2: De-Coupling Broni i Mechanik Walki z Rdzenia Silnika](#-priorytet-2-de-coupling-broni-i-mechanik-walki-z-rdzenia-silnika)
4. [Priorytet 3: System Prefabów Data-Driven (Likwidacja Hardcodingu w Edytorze)](#-priorytet-3-system-prefabów-data-driven-likwidacja-hardcodingu-w-edytorze)
5. [Priorytet 4: Czysty Komponentowy ECS (Likwidacja God-Struct MapProp)](#-priorytet-4-czysty-komponentowy-ecs-likwidacja-god-struct-mapprop)
6. [Priorytet 5: Dźwięk Zdarzeniowy (Sound Events zamiast SoundID Enum)](#-priorytet-5-dźwięk-zdarzeniowy-sound-events-zamiast-soundid-enum)
7. [Priorytet 6: Warstwa Wejścia Action-Mapping (Koniec z Hardcoded Keys)](#-priorytet-6-warstwa-wejścia-action-mapping-koniec-z-hardcoded-keys)
8. [Harmonogram Wdrożenia (Etapy M1 - M5)](#-harmonogram-wdrożenia-etapy-m1---m5)

---

## 🏛️ Manifest Architektoniczny

1. **Zero specyfiki gatunkowej w rdzeniu silnika:**
   - Silnik **nie zakłada**, że tworzona gra posiada broń, amunicję, zdrowie, apteczki, headshoty ani widok pierwszej osoby (FPS).
   - Twórca może zbudować w Lab Engine platformówkę 2D, grę logiczną, wyścigi, symulator chodzenia czy strategię RTS bez ani jednej linii kodu związanej ze strzelaniem.
2. **Dane ponad kodem (Data-Driven):**
   - Dodanie nowego obiektu, skrzynki, mebla, wroga czy dekoracji **nigdy** nie może wymagać dopisywania `case X:` w C++ ani rekompilacji `LabEditor.exe`.
3. **Kompozycja ponad dziedziczeniem (ECS):**
   - Obiekty w świecie składają się z elastycznych komponentów (`Transform`, `MeshRenderer`, `Collider`, `Script`), a nie potężnych struktur ze sztywnymi polami `bool hasAudio`, `bool hasCloth`.

---

## 🎥 Priorytet 1: Uniwersalny System Kamer (Koniec z Wymuszonym FPS)

### Diagnoza obecnego problemu:
- W `player_main.cpp` i `LabCamera.h` kamera domyślnie zakłada pozycję oczu gracza FPS (`_playerPos + Vec3(0, 0.2f, 0)`).
- Obsługa TPP (widok trzeciej osoby) ma zacinającą się pętlę kolizji iterującą po wszystkich pędzlach mapy, a kamera gubi kąty przy manipulacji myszą.
- Brak ujednoliconego kontrolera kamery dla innych gatunków gier.

### Zadania do wykonania:
- [ ] **Modułowe Kontrolery Kamer (`CameraController` / `CameraMode`):**
  - [ ] **Mode 1: First-Person (FPS):** Czysta kamera pierwszoosobowa z płynnym ruchem głowy (head bobbing jako opcja), clampem pitch (-89°..+89°) i izolacją od modelu gracza.
  - [ ] **Mode 2: Third-Person Orbit (TPP):**
    - Orbitowanie myszą wokół dowolnego celu (`targetEntity` / `targetTransform`).
    - Regulacja odległości (Distance / Zoom) za pomocą kółka myszy.
    - **Camera Boom / Spring-Arm z raycastem:** Jeśli ściana zasłania postać, kamera płynnie przybliża się przed przeszkodę (sphere sweep / raycast), zapobiegając przenikaniu przez geometrię.
  - [ ] **Mode 3: Top-Down / Rzut z góry:** Sztywny lub amortyzowany kąt nachylenia (np. 60°), śledzenie gracza w płaszczyźnie XZ (dla strategii, hack'n'slash, gier taktycznych).
  - [ ] **Mode 4: 2D Orthographic / Platformer:** Pixel-perfect śledzenie postaci w osiach XY z deadzone (strefą martwą kamery) i ograniczeniem granic mapy (Camera Bounds).
  - [ ] **Mode 5: Free-Fly / Cinematic:** Kamera swobodnego lotu (jak w edytorze) do przerywników filmowych lub podglądu sceny.
- [ ] **Płynne przejścia kamer (Camera Blend / Cinemachine-style):**
  - [ ] Funkcja interpolacji `blendToCamera(targetCam, duration, easeMode)` pozwalająca na kinowe zmiany ujęcia między kamerami w scenie.
- [ ] **Dynamiczny przełącznik w konfiguracji sceny i skryptach Lua:**
  - [ ] Eksport do Lua: `Camera.SetMode("TPP")`, `Camera.SetTarget(entity)`, `Camera.SetDistance(4.5)`, `Camera.SetFov(75.0)`.

---

## 🔫 Priorytet 2: De-Coupling Broni i Mechanik Walki z Rdzenia Silnika

### Diagnoza obecnego problemu:
- `include/LabWeapon.h` w bibliotece `LabWorld.dll` zawiera sztywny enum `WeaponID` (Pipe, Pistol, Shotgun, M4A4S, SG553, Minigun...).
- `include/LabArms.h` zawiera sztywny kod rąk FPP ze specyficznym cięciem rurą hydrauliczną (`melee ribbon during pipe slash`).
- `include/LabCombat.h` zawiera pole `bool isHeadshot` w ogólnej strukturze `RaycastHit`.
- `include/LabAI.h` zawiera `CombatBot` ze sztywną logiką strzelania FPS i drużynami FFA/Red/Blue.

### Zadania do wykonania:
- [ ] **Wydzielenie logiki FPS do showcase'u *Frozen-Life*:**
  - [ ] Przenieść `LabWeapon.h/.cpp`, `LabArms.h/.cpp` oraz `CombatBot` z bibliotek silnika (`LabWorld.dll`, `LabAnimation.dll`, `LabAI.dll`) do dedykowanego modułu gry: `src/games/frozen_life/` lub opcjonalnego pluginu `plugins/FPSGameplay/`.
  - [ ] Rdzeń silnika ma zerową wiedzę o broniach palnych, magazynkach czy przeładowaniu.
- [ ] **Oczyszczenie Fizyki i Raycastingu:**
  - [ ] Usunąć pole `isHeadshot` ze struktury `RaycastHit`.
  - [ ] Wprowadzić generyczne dane trafienia: `hitEntity`, `hitCollider`, `hitPoint`, `hitNormal`, `hitDistance`, `subShapeId` (np. ID kości/collidera).
  - [ ] Sprawdzanie trafienia w głowę (headshot) realizowane jako logika komponentu `HealthComponent` / `HitboxComponent` w skrypcie gry.
- [ ] **Generyczny System Ekwipunku / Interakcji (Data-Driven / Lua):**
  - [ ] Jeśli gra potrzebuje przedmiotów/broni, są one definiowane w plikach danych (np. `assets/items/*.json` lub skryptach Lua), a nie w enumach C++.

---

## 📦 Priorytet 3: System Prefabów Data-Driven (Likwidacja Hardcodingu w Edytorze)

### Diagnoza obecnego problemu:
- W `editor_main.cpp` funkcja `placePrebuilt(int prebuiltId)` posiada ogromny `switch (prebuiltId)` z 18 na sztywno wpisanymi obiektami (skrzynka ze sztywną ścieżką `crate_wood.png`, beczka z `barrel_hazard.png`, itp.).
- Dodanie nowego obiektu wymaga edycji kodu C++ i rekompilacji całego edytora.

### Zadania do wykonania:
- [ ] **Format Pliku Prefaba (`.prefab` w formacie JSON):**
  - Przykładowy format:
    ```json
    {
      "name": "Wooden Crate",
      "category": "Props",
      "components": [
        {
          "type": "Transform",
          "scale": [1.0, 1.0, 1.0]
        },
        {
          "type": "MeshRenderer",
          "model": "models/props/crate.obj",
          "texture": "textures/crate_wood.png"
        },
        {
          "type": "RigidBody",
          "mass": 15.0,
          "friction": 0.6
        }
      ]
    }
    ```
- [ ] **Katalog Prefabów (`assets/prefabs/`):**
  - [ ] Stworzyć podfoldery kategoryzujące: `assets/prefabs/environment/`, `assets/prefabs/lights/`, `assets/prefabs/physics/`, `assets/prefabs/gameplay/`.
- [ ] **Dynamiczny PrefabManager w Edytorze:**
  - [ ] Edytor przy uruchomieniu oraz przy odświeżeniu skanuje folder `assets/prefabs/`.
  - [ ] Zakładka *Prebuilts / Add Entity* automatycznie generuje przyciski i drzewo kategorii na podstawie znalezionych plików.
  - [ ] Kliknięcie przycisku instancjonuje encję bezpośrednio z pliku `.prefab`.
  - [ ] Likwidacja funkcji `placePrebuilt(int id)` i usunięcie twardych referencji do zasobów z `editor_main.cpp`.

---

## 🧩 Priorytet 4: Czysty Komponentowy ECS (Likwidacja God-Struct MapProp)

### Diagnoza obecnego problemu:
- `include/LabMap.h` zawiera strukturę `MapProp` z ponad 30 polami: `bool hasAudio`, `bool hasCloth`, `bool hasWaterVolume`, `bool hasForceField`, `bool hasVehicle`, `bool hasDecal`.
- Brak możliwości swobodnego łączenia komponentów i rozszerzania encji bez modyfikacji struktury bazowej.

### Zadania do wykonania:
- [ ] **Ujednolicenie na bazie `Entity` + `Component` ([`LabEntity.h`](file:///c:/Users/jancz/Desktop/Lab/include/LabEntity.h)):**
  - [ ] Każdy obiekt w scenie staje się instancją `Entity` z unikalnym GUID/ID i transformacją.
  - [ ] Dołączenie zachowań przez dedykowane komponenty:
    - `LightComponent`
    - `AudioSourceComponent`
    - `RigidBodyComponent`
    - `ClothComponent`
    - `WaterVolumeComponent`
    - `DecalComponent`
    - `LuaScriptComponent`
- [ ] **Uniwersalna Serializacja Sceny (`.labscene` JSON):**
  - [ ] Zastąpienie archaicznego tekstowego formatu `.labmap` czystym, rozszerzalnym formatem JSON.
  - [ ] Automatyczna serializacja/deserializacja komponentów bez twardych flag `hasXyz`.

---

## 🔊 Priorytet 5: Dźwięk Zdarzeniowy (Sound Events zamiast SoundID Enum)

### Diagnoza obecnego problemu:
- `include/LabAudio.h` zawiera enum `SoundID` z nazwami gier: `PistolShot`, `M4A4SShot`, `FootstepIce`, `FlashlightToggle`.

### Zadania do wykonania:
- [ ] **Przebudowa API `AudioEngine`:**
  - [ ] `AudioEngine::play(const std::string& assetOrEventPath, const Vec3& pos, float volume, float pitch)`
  - [ ] Zastąpienie sztywnego enumu odwołaniami do zasobów audio lub manifestu zdarzeń (`assets/audio/audio_manifest.json`).
- [ ] **Kolejki Audio i Szyny Dźwiękowe (Audio Buses):**
  - [ ] Szyny dźwięku: `Master`, `Music`, `SFX`, `Voice`, `UI` z osobną kontrolą głośności.

---

## 🎮 Priorytet 6: Warstwa Wejścia Action-Mapping (Koniec z Hardcoded Keys)

### Diagnoza obecnego problemu:
- Bezpośrednie odwołania do `Input::isKeyPressed('W')`, `'E'`, `'G'`, `'R'`, `'S'` w wielu miejscach kodu.
- Kolizje między sterowaniem kamerą, narzędziami edytora i skrótami klawiaturowymi.

### Zadania do wykonania:
- [ ] **Wirtualne Mapowanie Akcji (`InputManager`):**
  - [ ] Zdefiniowanie warstw kontekstowych wejścia:
    - **Context: Editor:** `Editor.GizmoMove` (G), `Editor.GizmoRotate` (R), `Editor.GizmoScale` (S), `Editor.FlyCam` (RMB).
    - **Context: Game:** `Game.MoveForward`, `Game.MoveRight`, `Game.Jump`, `Game.Interact`.
  - [ ] Plik konfiguracyjny powiązań klawiszy: `assets/configs/input_bindings.json`.
  - [ ] Możliwość pełnej zmiany przypisania klawiszy w locie (Rebinding) przez użytkownika lub grę.

---

## 📅 Harmonogram Wdrożenia (Etapy M1 - M5)

| Etap | Zadanie | Efekt dla Użytkownika / Architektury |
|---|---|---|
| **M1** | **System Prefabów Data-Driven (`assets/prefabs/`)** | Likwidacja `placePrebuilt switch` w edytorze. Dodawanie dowolnych obiektów przez pliki JSON bez rekompilacji silnika. |
| **M2** | **Uniwersalny System Kamer (`CameraModes`)** | Działający, bezbłędny TPP z orbitowaniem i ochroną przed kolizjami ze ścianami, czysty FPS, Top-Down i 2D bez wymuszania widoku pierwszej osoby. |
| **M3** | **De-Coupling Broni i Walki z Core DLL** | Oczyszczenie `LabWorld`, `LabAudio`, `LabCombat`. Bronie i mechaniki strzelanki istnieją wyłącznie w grze *Frozen-Life*, nie w silniku ogólnym. |
| **M4** | **Likwidacja God-Struct `MapProp` na rzecz ECS** | Czysta kompozycja encji z komponentów. Dowolne łączenie audio, światła, fizyki i skryptów w jednej encji. |
| **M5** | **Action-Based Input Mapping** | Brak konfliktów klawiszy, pełna możliwość rekonfiguracji sterowania w edytorze i w tworzonych grach. |

---

*Rejestr utworzony na podstawie ustaleń architektonicznych Lab Engine. Każdy etap będzie weryfikowany kompilacją C++20 bez ostrzeżeń i regresji.*
