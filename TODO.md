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
