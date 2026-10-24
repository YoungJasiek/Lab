# FL (Frozen-Life)
TODO:
- [x] Visual Benchmark: "Site 14 Dirty Corridor" (wzorcowy korytarz definiujący styl graficzny: materiały, cubemap, normal mapy, oświetlenie)
- [x] Props & 3D Models: modele otoczenia (drzwi laboratoryjne, szafki, lampy, rury, zacieki/decale)
- [x] First AI Bot:
  - [x] Model 3D + animacje bazowe (Idle, Walk, Talk, Attack/Death)
  - [x] Test Lip-Sync z plikiem audio i blendshapes/visemes
  - [x] Dźwięki kroków i głosu bota (z pozycjonowaniem 3D)
- [x] Story Event w korytarzu: wejście w Trigger Volume odpala dialog i ruch bota

# LAB (Engine & Studio)
TODO:
- [x] Momentum & Bunny Hop (fizyka ruchu w stylu Source Engine):
  - [x] Zachowanie pędu poziomego przy skoku (brak natychmiastowego zerowania wektora prędkości)
  - [x] Przyspieszenie w powietrzu (Air Acceleration & Air Strafing z ruchem myszy)
  - [x] Tarcie podłoża (Ground Friction) z oknem czasowym na continuous bhop
- [x] Developer Console & Overlay (na podstawie dev_console.md):
  - [x] Okno konsoli deweloperskiej pod tyldą (`~`) lub klawiszem funkcyjnym
  - [x] Parser komend z modularnymi prefiksami (`sp_`, `log_`, `wrd_`, `cam_`, `r_`, `phys_`, `ai_`, `snd_`, `ui_`, `dcl_`, `mp_`, `net_`, `sys_`)
  - [x] System stałego overlayu telemetrycznego 2D na ekranie (`log_momentum`, `log_fps`, `log_pos`, itd.)
  - [x] Obsługa cheatów (`sp_cheats 1`, `sp_god`, `sp_noclip`, `sp_speed`, `sp_get`)
- [x] Dynamic Scene Collisions & Triggers (usunięcie kolizji na sztywno z kodu):
  - [x] Kolizje gracza generowane dynamicznie z obiektów/ścian sceny (.labscene / brushes)
  - [x] Trigger Volumes: niewidzialne strefy w scenie wyzwalające zdarzenia (np. dialog, skrypt)
- [x] 2D Screen & UI Editor (przekształcenie HUD Editora w uniwersalny edytor 2D Canvas):
  - [x] Tworzenie i edycja całych ekranów: Menu Główne, Menu Pauzy, Opcje/Ustawienia oraz HUD
  - [x] Komponenty interaktywne: przyciski (Button), panele (Panel), grafiki tła (Image), teksty (Label)
  - [x] Zapis i odczyt layoutów do plików .labui
- [x] Bot Pathing (Story Mode) w edytorze:
  - [x] Stawianie punktów trasy (Waypoints / Path Nodes) w scenie 3D
  - [x] Wizualizacja linii ścieżki i przypisanie trasy do bota
- [x] Editor Ergonomics & Comfort (QoL):
  - [x] 3D Grid Snapping (przyciąganie modułów korytarza do siatki)
  - [x] Skróty klawiszowe: Ctrl+D (duplikacja), Del (usuwanie), F (focus kamery)

# AKTUALNE ZADANIA (Notepad.md & Architektura Modów / Addonów)

## 1. General & Graphics
- [x] **Error Textures (Brakujące tekstury w stylu Source Engine):**
  - [x] Implementacja procedury generowania fioletowo-czarnej szachownicy (#FF00FF / #000000, 32x32) w `src/LabRenderer.cpp` dla brakujących tekstur
  - [x] Całkowita blokada renderowania niewidzialnych/czarnych powierzchni w przypadku braku pliku tekstury na dysku lub w VFS

## 2. Players & Characters (Postacie i Gracz)
- [x] **Model gracza (Guard as Player):**
  - [x] Użycie modelu `guard.glb` jako reprezentacji gracza w widoku TPP oraz w multiplayerze
  - [x] Wymiana klocków/cubes na pełne rigi kości (szkielety) kompatybilne ze standardem modeli 3D (glTF/FBX skinning na 70 kościach)
- [x] **Guard Viewmodel (Widok FPP gracza):**
  - [x] Dedykowany model rąk/ramion strażnika w widoku pierwszej osoby trzymających poszczególne bronie

## 3. Multiplayer & Networking (Tryb Sieciowy)
- [x] **Loading Map Sequence (Ekran ładowania w stylu Garry's Mod / Source Engine):**
  - [x] Sekwencja etapów ładowania mapy: łączenie, walidacja zasobów, wczytywanie geometrii, spawning gracza (na wzór `insp/fl/menu/extracted/garrys_mod/`)
  - [x] Integracja paska postępu pobierania FastDL dla brakujących map `.lmap` i pakietów `.lpak`
- [x] **Rozdzielenie trybów: Story vs Arcade / Multiplayer:**
  - [x] Wyłączenie triggerów dialogowych, cutscenek i pauz fabularnych w sesjach Arcade i Multiplayerze
- [x] **Blokada cheatów na serwerach publicznych i oficjalnych:**
  - [x] Wymuszenie `sv_cheats 0` – autorytatywne odrzucanie komend ułatwiających rozgrywkę (`sp_god`, `sp_noclip`, `sp_speed`, `sp_get`)
- [x] **Przeglądarka serwerów (Server Browser):**
  - [x] Wyszukiwarka i przeglądarka serwerów dedykowanych z zakładkami Internet / LAN / Ulubione, filtry map, liczby graczy i pingu (na wzór `insp/fl/menu/extracted/cs_source/` oraz `garrys_mod/`)

## 4. Addons & Modding System (Architektura `/addons/`)
- [x] **Standard katalogu `/addons/` (Source Engine / Garry's Mod model):**
  - [x] Centralny folder `/addons/<nazwa_moda>/` dla wszystkich modów (skórki, bronie, mapy, pakiety UI, mody skryptowe)
  - [x] Warstwowe montowanie VFS: `addons/*` (najwyższy priorytet) -> `assets/` (baza FL) -> `engine/` (rdzeń silnika)
  - [x] Flaga startowa linii komend: `game.exe -game addons/<nazwa>` oraz `lhmr.exe -game addons/<nazwa>`
- [x] **Wyrzucenie 'Projects Hub' z Hammera:**
  - [x] Usunięcie dashboardu "Projects Hub" z edytora – Hammer odpala się natychmiast w trybie edycji 3D siatki i pędzli (czysty Hammer, bez zbędnych ekranów w stylu Unity/Godot)
  - [x] Zastąpienie zakładki Character Studio w edytorze dedykowaną zakładką **Mods / Addons Explorer** (przeglądanie i aktywacja modów z dysku, na wzór `insp/fl/menu/extracted/garrys_mod/`)
  - [x] Jedna komenda CLI do scaffoldingu modów: `lhmr --create-mod <nazwa>` generująca szablon w `addons/<nazwa>/` z `addon.json`, podfolderami i skryptami
- [x] **Startup Hooks & Przykładowy Mod Lua (np. intro w stylu CoD):**
  - [x] Wsparcie dla skryptu `scripts/startup.lua` lub mediów `media/startup.mp4` w addonach (jako demonstracja modu wykorzystującego Lua)
  - [x] Opcjonalna sekwencja wejścia w stylu Call of Duty dla Frozen-Life (czarny ekran, zielone napisy typewriter ze współrzędnymi satelitarnymi, dźwięk radiowy)
  - [x] Własny pakiet UI/HUD moda (`.labui`) definiowany w addonie
- [x] **Zabezpieczenie przed złośliwym oprogramowaniem (Malware w `.mp4`, `.mkv`, itp.):**
  - [x] Weryfikacja sygnatury binarnej (Magic Bytes) nagłówków wideo (`ftyp` dla MP4, EBML `0x1A45DFA3` dla MKV) – natychmiastowe odrzucanie plików wykonywalnych `.exe`/`.dll` ze zmienionym rozszerzeniem (sygnatura `MZ`)
  - [x] Całkowity zakaz wywołań powłoki systemowej (`ShellExecute`, `system()`) do odtwarzania mediów
  - [x] Izolowany potok dekodera wideo (wypychanie surowych ramek RGB/YUV bezpośrednio do tekstury GPU bez uprawnień wykonywania kodu)
  - [x] Wyłączenie obsługi załączników (attachments) i skryptów w napisach kontenera MKV
  - [x] Sanityzacja ścieżek pod kątem Directory Traversal (`..`, `:`, `/`)

## 5. UI & UX (Frozen-Life Rebuild & Source VGUI2 Aesthetic)
- [x] **Rebuild UI w Frozen-Life (Nowe, lepsze menu w stylu Source Engine - insp/FL/menu):**
  - [x] Przepisanie menu głównego z ciężkich, niebieskich prostokątów na minimalistyczny, elegancki układ w stylu Source Engine (Half-Life 2 / L4D)
  - [x] Lewostronna, czysta typografia nawigacji (RESUME, NEW GAME, FIND SERVERS, CREATE SERVER, ADDONS, ACHIEVEMENTS, STUDIO, OPTIONS, QUIT) z subtelnym wskaźnikiem hover (Source amber highlight)
  - [x] Spójne centrowane okna dialogowe VGUI2 (`980x580`, `820x540`): ciemnografitowe tło slate `#1c2026`, fazowanie 1px (bevel), nagłówek 34px i minimalistyczny przycisk `[X]`
  - [x] Usunięcie jaskrawych, klockowatych bloków i neonowych przycisków na rzecz stonowanych przycisków VGUI2 z krótkimi etykietami (1-2 słowa) i akcentem amber dla akcji wiodącej
  - [x] Spójny redesign wszystkich ekranów: `Singleplayer`, `JoinGame` (Find Servers), `HostGame` (Create Server), `Addons`, `Options`, `MultiSelect`
- [x] **Osiągnięcia i statystyki (Achievements & Statistics - CS:Source / HL2 VGUI):**
  - [x] Dedykowany ekran osiągnięć i statystyk kariery (`MenuScreen::Achievements`) z zakładkami: `ACHIEVEMENTS` i `STATISTICS`
  - [x] Graficzne odznaki osiągnięć (autentyczne ikony PNG CC0 z Kenney pobrane przez subagenta, zero generowania proceduralnego)
  - [x] Autentyczny, segmentowy pasek postępu (Source segmented progress bar) z podziałkami co 10px: `X of Y Achievements Unlocked (Z%)`
  - [x] Siatka kart osiągnięć (2 kolumny x 4 wiersze) z odznaką/teksturą PNG, tytułem, opisem, mini paskiem postępu i statusem `[ UNLOCKED ]` (złoto) / `[ LOCKED ]` (szary)
  - [x] Opcja filtrowania: pole wyboru `[x] Hide Unlocked` (ukrywanie zdobytych osiągnięć)
  - [x] Tabela statystyk kariery (Kills, Deaths, K/D Ratio, Headshots, Celność %, Bunnyhop Jumps, Wybuchy beczek, Czas gry)
  - [x] Zintegrowane haki w silniku rozgrywki: zliczanie fragów, strzałów, trafień, bunnyhopów i czasu gry na żywo
- [x] **Separacja narzędzi od gry (Clean Game Client Architecture):**
  - [x] Usunięcie Character Studio i narzędzi edytorskich z `game.exe` (brak opcji STUDIO w menu głównym)
  - [x] Usunięcie trybu Hammer Editor oraz skrótu klawiszowego F2 z klienta gry
  - [x] Narzędzia deweloperskie działają jako w pełni niezależne, wyspecjalizowane aplikacje: `lhmr.exe` (Hammer), `lchs.exe` (Character Studio), `lfm.exe` (Lab FilmMaker)
- [x] **Paginacja i naprawa listy map (Map Selection Polish):**
  - [x] Eliminacja overflowu listy map poza okno dialogowe i nakładania się na przyciski nawigacyjne ("Back", "Browse...", "Start")
  - [x] Deduplikacja plików map według nazwy bazowej (stem) z priorytetem dla formatu `.labscene`
  - [x] Czytelne, sformatowane nazwy map w UI (np. "Site 14 :: Chapter 1", "Facility Alpha") zamiast surowych ścieżek
  - [x] Paginacja z ograniczeniem do max 7 widocznych pozycji, przyciskami przewijania `UP` / `DN` i obsługą kółka myszy
- [x] **Żywe tło 3D w Menu Głównym (Real-Time 3D Background Scene - Source Engine Style):**
  - [x] Loader modeli binarnych glTF 2.0 (`Mesh::loadGLB`) w `LabRenderer` obsługujący geometrię, hierarchię węzłów, transformacje, współrzędne UV i normalne
  - [x] Stworzenie modularnej mapy tła `menu_background.labscene` z modelami fabryki/laboratorium (taśmociągi, maszyny, ramiona robotów, pomosty, rurociągi, terminale) pobranymi z autentycznego pakietu Kenney CC0
  - [x] Płynna, kinowa trajektoria kamery (dolly & orbit) w czasie rzeczywistym
  - [x] Pełny potok renderowania 3D za menu: Shadow Depth Pass, Blinn-Phong oświetlenie punktowe/kierunkowe, ACESFilmic HDR Tonemapping & Bloom, oraz 42% przyciemnienie tła dla kontrastu interfejsu VGUI2
- [ ] **Edytor 2D Canvas w stylu Figma (Figma-like 2D HUD/Canvas Editor):**
  - [ ] Nowoczesny interfejs edytora 2D: hierarchia warstw, wektorowe ramki (frames), inspekcja właściwości w stylu Figmy, transformacje (odłożone na później per wytyczne)
