# LFM_PLAN.md: Lab FilmMaker (LFM) Roadmap & Architecture

Plik architektoniczny i plan rozwoju dedykowanego narzędzia reżyserskiego **Lab FilmMaker (`lfm.exe`)** – pierwszoligowego projektu w ekosystemie **Lab Engine**.

Wzorzec referencyjny: **Valve Source Filmmaker (SFM)** (patrz: `insp/lfm/`).

---

## 1. Wizja i Cel Projektu

`lfm` to profesjonalne, zintegrowane z silnikiem studio reżyserskie, animacyjne i montażowe, służące do tworzenia zwiastunów, przerywników filmowych (cutscenek) oraz demonstracji technologicznych silnika Lab Engine.

### Filozofia architektoniczna:
1. **To nie jest miniaturowy widget w edytorze map:** To pełnoprawne, zaawansowane środowisko pracy (DCC - Digital Content Creation Tool).
2. **Prawdziwy silnik pod maską:** Korzysta z tego samego potoku renderującego (`lrender.dll`), animacji GPU (`lanim.dll`), fizyki (`lphys.dll`) i dźwięku (`laudio.dll`), co gra `game.exe`. To, co widzisz w viewporcie, jest w 100% zgodne z runtime'em.
3. **Deterministyczny render:** Niezależny od wydajności karty graficznej – klatki renderowane są ze stałym $\Delta t = \frac{1}{60}\text{ s}$ bezpośrednio do potoku wideo.
4. **Czysty kod C++20 bez placeholderów:** Modułowa architektura paneli, drzewiasty stan encji (`AnimationSet`), dwu-viewportowy podgląd sceny i trójtrybowa oś czasu.

---

## 2. Układ Interfejsu (SFM Layout Standard)

Zgodnie z materiałami referencyjnymi (`insp/lfm/ss_0ccc...`, `insp/lfm/ss_48d4...`, `insp/lfm/ss_76b5...`):

```
┌──────────────────────────────────────────────────────────────────────────────────────────────────┐
│ File  Edit  Windows  View  Help                                                    [Render Video]│
├──────────────────────────┬───────────────────────────────────┬───────────────────────────────────┤
│ ANIMATION SET EDITOR     │ PRIMARY VIEWPORT                  │ SECONDARY VIEWPORT (Work Cam)     │
│ [ + ] [ Search       ] ⚙ │ 00:00:23.958         mtt_demo     │ 00:00:23.958         Work Camera  │
│ ──────────────────────── │                                   │                                   │
│ ▼ guard_01               │                                   │      /▲\ (Frustum kamery 1)       │
│   ► Upper Face           │        [ Kadr Kamery 1 ]          │     / | \                         │
│   ► Eyes                 │                                   │    /  |  \                        │
│   ▼ Body                 │   (Ostateczny obraz filmowy       │   *───┴───*   [ Postać w 3D ]     │
│     • bip_pelvis         │    z post-processingiem)          │   (Widok roboczy z gizmem 3D,     │
│     • bip_spine_0        │                                   │    trajektorią i oświetleniem)    │
│     • bip_head           │                                   │                                   │
│   ► Arms / Legs          │                                   │                                   │
│ ► camera1                │                                   │                                   │
│ ► spot_light_main        ├───────────────────────────────────┼───────────────────────────────────┤
│                          │ [|<] [ [ ] [Play/Pause] [ ] ] [>|]│ [|<] [ [ ] [Play/Pause] [ ] ] [>|]│
│                          │ Cam: [ camera1        ▼ ] [ ⚙ ]   │ Cam: [ Work Camera    ▼ ] [ ⚙ ]   │
├──────────────────────────┴───────────────────────────────────┴───────────────────────────────────┤
│ SEQUENCER TABS:  [🎞 Clip Editor]  [〰 Motion Editor]  [📈 Graph Editor]           Snap: [Frames] 100%│
├──────────────────────────────────────────────────────────────────────────────────────────────────┤
│ TIME RULER:  00:00      00:05      00:10      00:15      00:20      00:25      00:30      00:35    │
├──────────────────────────────────────────────────────────────────────────────────────────────────┤
│ ▼ SHOTS   │ [=== Shot 01: Cryo Pod ===] [==== Shot 02: Blast Door ====] [=== Shot 03: Hall ===]  │
│ ▼ ACTORS  │ guard_01: [ idle_ready           ] [ walk_patrol        ] [ interact_terminal ]       │
│ ▼ AUDIO   │ voice:    ▂▃▅▇█ [dialogue_01.wav] ▃▂      ▂▃▅▆ [alert.wav]                            │
│           │ sfx/music: ▂▃▅▆▇█▇▆▅▃▂ [ambient_drone.wav] ▂▃▅▆▇█▇▆▅▃▂ [sparks_cable.wav]            │
└──────────────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Moduły i Struktury Danych LFM

### 3.1. `LFMAnimationSet` (Drzewo elementów i kości postaci)
- Odpowiednik SFM Animation Set.
- Każdy załadowany model postaci, kamera czy źródło światła jest reprezentowane jako zestaw kanałów kontrolnych (`ControlChannel`).
- Hierarchia:
  - Głowa, oczy, gałęzie kości ciała (`bip_pelvis`, `bip_spine`, `bip_head`, ręce, nogi).
  - Wskaźniki wag transformacji (Local translation, Rotation Euler/Quat, Scale).
- Integracja z `Skeleton` i `Animator` z biblioteki `lanim.dll`.

### 3.2. `LFMDualViewport` (Podwójny podgląd)
- **Primary Viewport (Kamera reżyserska):**
  - Wyświetla obraz z aktywnej kamery ujęcia (`CameraComponent`).
  - Stosuje docelowy aspect ratio (16:9, 2.39:1 Cinemascope).
  - Wyświetla timecode klatki i identyfikator aktywnego ujęcia (`Shot ID`).
- **Secondary Viewport (Kamera robocza / Work Camera):**
  - Swobodna kamera nawigacyjna (WASD + PPM Orbit/Pan).
  - Rysuje w przestrzeni 3D:
    - Bryłę widzenia (Frustum Pyramid) aktywnej kamery filmowej.
    - Stożki i sfery zasięgu świateł dynamicznych.
    - 3D Motion Path – trajektorię ruchu kości lub kamery z punktami kluczowymi.
    - Gizmo translacji / rotacji (3 osie: czerwona, zielona, niebieska).

### 3.3. `LFMSequencer` (Trójtrybowa Oś Czasu)
1. **Clip Editor (Montaż ujęć i dźwięku):**
   - Dzielenie filmu na ujęcia (`Shots`).
   - Przycinanie początku (`In`) i końca (`Out`) ujęcia.
   - Paski ścieżek audio (`Dialogue`, `Sound Effects`, `Music`) z generowaniem i rysowaniem rzeczywistego kształtu fali dźwiękowej (Waveform).
2. **Motion Editor (Miksowanie i procedury):**
   - Zaznaczanie przedziału czasu głowicą (Green Selection Range z fade-in/fade-out).
   - Suwaki procedur: `Smooth` (wygładzanie drgań), `Jitter` (dodanie trzęsienia kamery), `Ramp`, `Zero`, `Default`.
3. **Graph Editor (Edytor krzywych F-Curves):**
   - Wykres krzywych Béziera dla poszczególnych kanałów (np. `Camera FOV`, `Head Rot Y`).
   - Uchwyty stycznych (Tangents) dla idealnie płynnych przejść akceleracji i wyhamowania.

---

## 4. Harmonogram Prac (TODO Roadmap)

### Etap 1: Fundamenty UI i Architektury (Zrealizowany 100%)
- [x] Utworzenie modułowej struktury `include/LFM/` i `src/LFM/`:
  - `LFMWorkspace` – zarządzanie podziałem ekranu i dokowaniem paneli.
  - `LFMAnimationSet` – struktury danych aktorów, kamer, świateł i kości.
  - `LFMDualViewport` – obsługa renderowania kamery filmowej (Primary) oraz kamery roboczej z frustumem (Secondary).
  - `LFMSequencer` – trójtrybowa dolna belka (Clip / Motion / Graph).
- [x] Przebudowa `lfm_main.cpp` na profesjonalny układ zgodny ze zrzutami z Valve SFM.
- [x] Pełna integracja stylistyki `LabTheme` (ciemny grafit `#1E1F22`, ramki, nagłówki kart, ikonografia).

### Etap 2: Animacja Postaci i Drzewo Kości (Zrealizowany 100%)
- [x] Wczytywanie szkieletu postaci (`guard.glb`) do Animation Set Editora z hierarchią 70 kości.
- [x] Wyświetlanie listy kości w widoku drzewa (Tree View) z przewijaniem, czytelnymi nazwami (`Hips`, `Spine`, `Head`, `LeftArm`), zwijaniem i rozwijaniem.
- [x] Zaznaczanie kości i manipulacja 3D gizmem (X=czerwony, Y=zielony, Z=niebieski) oraz wizualizacja szkieletu ze złotymi/błękitnymi łącznikami w oknie roboczym.
- [x] Odtwarzanie biblioteki 23 klipów FBX (`Idle`, `Walk`, `Talk`, `Firing Rifle`) na osi czasu z animacją ruchu postaci po scenie.

### Etap 3: Reżyseria Ujęć (Multi-Shot Workflow - Zrealizowany 100%)
- [x] System ujęć (`Shot`): ujęcia `shot001`, `shot002`, `shot003` z dedykowanymi kamerami, czasem trwania i kolorystyką.
- [x] Przełączanie cięć montażowych w locie na osi czasu (wide master -> tracking cable -> close-up action breach).
- [x] Rysowanie torów ruchu (Motion Paths) w przestrzeni 3D ze złotymi węzłami i zieloną trajektorią.
- [x] Ładowanie sceny (`.labscene` / `.labmap` z `lhmr.exe`) bezpośrednio do LFM z renderingiem geometrii, drzwi, świateł i kamer.

### Etap 4: Dźwięk i Waveformy (Zrealizowany 100%)
- [x] Ładowanie autentycznych plików `.wav` (`bot_corridor_dialog.wav`, `footstep_metal.wav`, `terminal_beep.wav`, `weapon_pistol.wav`, `reload.wav`).
- [x] Własny parser PCM RIFF WAVE ekstrahujący rzeczywiste amplitudy i rysujący zielone waveformy na osi czasu (styl SFM).
- [x] Synchronizacja audio: odtwarzanie próbek dźwięku przy odtwarzaniu osi czasu przez `AudioEngine`.

### Etap 5: Profesjonalny Render do Wideo i Eksport (Zrealizowany 100%)
- [x] Zintegrowany potok FFmpeg z obsługą rozdzielczości (720p HD, 1080p FHD, 4K UHD) i klatkażu (24, 30, 60 FPS).
- [x] Płynny, nieblokujący proces renderowania (`FilmExportSession`) krok-po-kroku z odczytem klatek z aktywnego bufora OpenGL bez zawieszania pętli aplikacji.
- [x] Dedykowany panel dialogowy eksportu (`ExportSettingsModal`) z wyborem formatu (MP4 H.264 / PNG sekwencja), zakresem (całość, ujęcie, test 5s), paskiem postępu na żywo, licznikiem klatek i możliwością przerwania renderu w dowolnym momencie.
- [x] Pełny test automatyczny w suicie silnika (Test 97 w `TestVerify`) weryfikujący poprawność generowania pliku MP4 H.264.

### Etap 6: Interaktywność Osi Czasu i Własna Tożsamość Lab Director Studio (Zrealizowany 100%)
- [x] W pełni interaktywny scrubbing osi czasu: kliknięcie lub przeciąganie kursorem myszy z LPM po linijce czasu lub ścieżkach natychmiast przesuwa głowicę i aktualizuje podgląd ujęcia 3D w obu viewportach w czasie rzeczywistym.
- [x] Zaznaczanie i manipulacja blokami ujęć, blokami animacji aktora oraz klipami audio z wyróżnieniem obramowaniem aktywnego elementu i usuwaniem klawiszem Delete lub przyciskiem na belce.
- [x] Skróty klawiszowe nawigacji reżyserskiej: Spacja (Start/Pauza), Home (przewinięcie do 0.0s), Strzałki Lewo/Prawo (precyzyjny skok o 1 klatkę do tyłu/przodu).
- [x] Zastąpienie klonowanych elementów SFM nowoczesnym 3-zakładkowym panelem Lab Director Studio:
  - Zakładka **Scena**: hierarchia szkieletu, kości, świateł i kamer wraz z kartą parametrów wybranego elementu oraz przełącznikami manipulatora 3D.
  - Zakładka **Animacje**: pełna biblioteka 23 animacji FBX z podglądem na żywo (`[Podgląd]`) i natychmiastowym wstawianiem na oś czasu pod głowicę (`[+ Na oś]`).
  - Zakładka **Audio**: biblioteka efektów i dialogów WAV z natychmiastowym odsłuchem (`[Odsłuchaj]`) oraz dodawaniem ścieżki na oś z generowaniem fali dźwiękowej (`[+ Na oś]`).
- [x] Wbudowane okno dialogowe wyboru i ładowania map (`MapBrowserModal`) z automatycznym skanowaniem `assets/maps/` (`.labmap` i `.labscene`) i natychmiastowym załadowaniem geometrii i świateł do przestrzeni reżyserskiej bez restartu programu.
