# TODO & Roadmap – Lab Engine & Frozen-Life (FPS)

Dokument zarzadczy silnika **Lab** oraz gry FPS **Frozen-Life**.
Wszystkie systemy tworzone sa w standardzie **C++20**, **OpenGL 4.5+ Core Profile** (Direct State Access) z zachowaniem filozofii **RAII**, braku wyciekow pamieci i zerowych ostrzezen kompilatora (MSVC /W4 /WX /EHsc).

---

## I. ZREALIZOWANE ETAPY (STAN OBECNY – 100% DONE)

### 1. Rdzen Silnika & Render (OpenGL 4.5 Core Profile)
- [x] Czysty profil rdzenny (DSA, VAO, VBO, EBO, UBO) – brak archaizmow glBegin/glEnd.
- [x] Oswietlenie Blinn-Phong o wysokim kontrascie w stylu Source Engine (Half-Life 2).
- [x] Dynamiczne swiatla punktowe (rozblysk lufy, eksplozje RPG).
- [x] Frustum Culling na bazie 6 plaszczyzn bryly widzenia kamery – odrzucanie obiektow AABB poza kadrem.
- [x] Szybka matematyka kolizyjna promieni Fast Slab Ray-AABB.
- [x] Silnik typografii LabFont (rasteryzator Windows GDI z antyaliasingiem, obsluga fontu systemowego i GeoSans).
- [x] Gradientowy i atmosferyczny skybox.
- [x] System Check-Once Caching dla zasobow – zero odpytywania dysku w petli renderowania (s_missingSTL, s_modelTexCache, s_pathCache).
- [x] Wbudowane proceduralne fallbacki tekstur generowane w pamieci RAM/GPU dla wszystkich powierzchni i broni.
- [x] Obsluga binarnego formatu STL (Mesh::loadSTL) z odczytem 16-bitowych kolorow wierzcholkow Magics/VisCAM.

### 2. Mechanika Gry Frozen-Life (FPS)
- [x] Kontroler FPS kamery (LabCamera) z obsluga myszy, ruchow WASD, skokow i grawitacji.
- [x] Detekcja kolizji geometrii LabCollision (plynny slizg Swept AABB Move & Slide bez klinowania sie w scianach).
- [x] Pelny arsenal 9 unikalnych broni (LabWeapon):
  1. Pipe (bron biala rura, zamach melee, startowa)
  2. Pistol (pistolet taktyczny semi-auto, startowy)
  3. Shotgun (strzelba pompka, 8 srucin, potezny odrzut)
  4. M4A4-S (wyciszony karabinek szturmowy)
  5. SG553 (karabin szturmowy o duzym kalibrze)
  6. Minigun (obrotowe lufy, ekstremalna szybkostrzelnosc)
  7. Plasma Gun (swiecace pociski plazmowe)
  8. Railgun (przebijajacy promien kinetyczny)
  9. RPG (fizyka lotu rakiety, dymny slad, eksplozja ze splash damage)
- [x] Przelaczanie broni pod klawiszami 1-9, kolkiem myszy oraz szybka zmiana Q.
- [x] Balistyka: fizyka pociskow lecych (rakiety, plazma) oraz celny hitscan ze smugami swietlnymi (Tracers).
- [x] Proceduralne animacje ViewModelu (LabAnim, SpringDamper): odrzut (kickback), bezwladnosc myszy (sway), kolysanie w marszu/biegu (bobbing), opuszczanie przy zmianie broni.
- [x] Podesty spawnu broni na mapie (Weapon Spawners): podest z pierscieniem neonowym (Cyan/Czerwony), obracajacy sie model 3D, zbieranie przez kolizje i automatyczny respawn po 60 sekundach z efektem czasteczkowym.
- [x] Podnoszenie amunicji (+36) oraz apteczek polowych (+50 HP).
- [x] Zaawansowane AI Botow (LabAI): maszyna stanow (Patrol, Investigate, Combat Strafe, Retreat), walka wielocelowa (bot vs bot vs gracz), hitscan z mnoznikiem Headshot, cykl smierci i respawn w bazach.
- [x] Tryby gry FFA (Deathmatch) oraz TDM (Team Alpha vs Team Beta) ze zroznicowanymi punktami odrodzen.
- [x] Tablica wynikow TAB (K/D, ping, punktacja, podzial na druzyny).
- [x] Czat w grze (LabChat) z buforem wiadomosci i znacznikami czasu.
- [x] Menu glowne: Host Game, Join Game, wybor mapy, konfiguracja botow.

### 3. System Czasteczek 3D (LabParticles)
- [x] GPU emiter czasteczek: iskry przy trafieniu w metal i beton, krew przy trafieniu w boty, dym i plomien wylotowy (muzzle flash), eksplozje RPG (blysk, odlamki, fala uderzeniowa).
- [x] Obsluga przezroczystosci (Alpha Blending oraz Additive Blending).

### 4. Valve Hammer Editor (LabHammer.exe)
- [x] Widok trojwymiarowy (3D Viewport) oraz dwuwymiarowy rzut z gory (Top-Down).
- [x] Zaznaczanie raycastem bryl, jednostek, spawnow i spawnerow broni.
- [x] Manipulatory gizmo: translacja, przyciaganie do siatki (Grid Snap), klonowanie (Ctrl+D), usuwanie (Del), centrowanie kamery (F).
- [x] Pelny zestaw 18 ikon gornego paska narzedzi oraz 8 ikon lewej palety narzedzi (Lab::HammerIcons).
- [x] Zakladka Prebuilts (Prebuilty) z dedykowanymi ikonami: punkty spawnow FFA/TDM, skrzynki amunicji, apteczki, barykady, filary, bramy pancerne oraz 9 spawnerow broni.
- [x] Autentyczne holograficzne manekiny 3D gracza na podestach spawnow w swiecie (glowa, wizjer, tulow, nogi, obrys i strzalka kierunku patrzenia).
- [x] Inspektor wlasciwosci (Properties) z edycja UV oraz Outliner struktury mapy.
- [x] Przegladarka tekstur i mapowanie UV (skala i pozycjonowanie Source UV).
- [x] Pelny zapis i odczyt formatu .labmap.

### 5. System Dzwieku Przestrzennego 3D (LabAudio / miniaudio)
- [x] Integracja nowoczesnego silnika miniaudio v0.11.25 bez zewnetrznych zaleznosci (DirectSound/WASAPI).
- [x] Wbudowany syntezator proceduralny WAV dla wszystkich 20 dzwiekow gry (RIFF 16-bit PCM: FM synthesis, szum bialy/rozowy z filtrami, obwiednie ADSR).
- [x] Pelne udzwiekowienie 9 broni:
  - Wystrzaly: Pistol, Shotgun, M4A4-S, SG553, Minigun, Plasma Gun, Railgun, RPG.
  - Walka wrecz Pipe: zamach (swing) i tepe uderzenie (hit impact).
  - Pusty magazynek (Dry fire click) oraz przeladowanie broni (Reload).
  - Potezna przestrzenna detonacja rakiet RPG (Explosion).
- [x] Dynamiczne dzwieki krokow gracza (Footsteps) z rozroznieniem podloza (beton, blacha, lod).
- [x] Odglosy podnoszenia przedmiotow: amunicja (+36), apteczka (+50 HP), respawn broni na podestach.
- [x] Odglos otrzymywania obrazen przez gracza (Player Hurt).
- [x] Pelne pozycjonowanie 3D (3D spatial audio) – pozycjonowanie sluchacza (kamera gracza), kierunek, wektor gora, tlumienie odwrotno-kwadratowe i panorama stereo.
- [x] 3D odglosy strzalu botow w swiecie gry podczas walki.
- [x] Odporna na wycieki i wielowatkowosc pula 64 stalych instancji dzwiekowych bez realokacji pamieci.

### 6. Zestaw Testow Automatycznych (TestVerify.exe)
- [x] 23 zautomatyzowane zestawy testow integracyjnych i jednostkowych weryfikujace poprawnosc matematyki, renderera, walki, kolizji, edytora oraz kompletnego silnika dzwieku 3D (z generowaniem i weryfikacja naglowkow 20 plikow WAV).

### 7. Rece Gracza FPP & Zaawansowane Stany Animacji Broni (Sprint 2 - 100% DONE)
- [x] Wprowadzenie widocznych rak gracza w widoku FPP (rekawice bojowe / kombinezon taktyczny Frozen-Life) trzymajacych chwyt broni.
- [x] System gniazda dloni (Socket Bone Attachment) – plynne wiazanie modeli STL z ruchem dloni (chwyt pistoletowy, karabinowy, oburacz do rury).
- [x] Pelna sekwencja animacji przeladowania (Reload): opuszczenie broni, wyjecie magazynka ze zlotymi nabojami, wlozenie nowego magazynka, odciagniecie zamka i powrot na linie celowania.
- [x] Animacja inspekcji broni (Inspect) na klawisz F (obrot broni w dloniach, prezentacja odbiornika, rekawic i cyber-karwasza).
- [x] Dynamiczny luk zamachu rura (Heavy Melee Slash) z efektem smugi ruchu (Kinetic Motion Trail).
- [x] Cyber-karwasz z dioda telemetryczna LED (cyan) i kompozytowymi wkladkami ochronnymi.

### 8. Oswietlenie w Czasie Rzeczywistym, Latarka Half-Life 2 & Dynamiczne Cienie (Sprint 3 - 100% DONE)
- [x] Rzutowanie perspektywiczne i ortograficzne (`Mat4::ortho`, `Mat4::perspective`).
- [x] Podsystem swiatel `LabLight` (klasa `Flashlight` – taktyczny reflektor HL2 ze stozkiem wewnetrznym/zewnetrznym, tlumieniem odleglosciowym i zbieznoscia na linie wzroku).
- [x] Dynamiczny bufor cieni `ShadowMap` – FBO 2048x2048 `GL_DEPTH_COMPONENT24` z Direct State Access (DSA) i bezpiecznym zarzadzaniem viewportem.
- [x] Syntetyzator przelacznika latarki (`SoundID::FlashlightToggle`, mechaniczny dzwiek klikniecia w `LabAudio`).
- [x] Pipeline shaderow cieni (Vertex & Fragment pass glebokosci `shadowDepth`, 3x3 Percentage-Closer Filtering PCF z dynamicznym slope-scaled normal bias w glownym shaderze).
- [x] Dwufazowy render sceny w `main.cpp` (Pass 1: mapa glebokosci cieni; Pass 2: oswietlenie sceny i cienkowanie).
- [x] Sterowanie: klawisz `F` do wlaczenia/wylaczenia latarki z dynamicznym cieniem, klawisz `V` do inspekcji broni FPP.
- [x] Zautomatyzowany i wizualny test 25 w `TestVerify.exe` weryfikujacy 2048x2048 FBO, stozek latarki i cienkowanie (`test_flashlight_and_shadows.bmp`).

### 9. Silnik Animacji Szkieletowej glTF 2.0 & GPU Vertex Skinning (Sprint 4 - 100% DONE)
- [x] Pelna biblioteka matematyki kwaternionow `Quat` w `LabMath.h`: slerp, fromEuler, fromAxisAngle, toMat4 oraz transformacje z odwrotnoscia macierzy `Mat4::inverse()`.
- [x] Format glTF 2.0 (`LabSkeletal.h`, `LabSkeletal.cpp`): loader plikow glTF oraz wbudowany generator rigow z 20 koscmi (Root, Pelvis, Spine, Torso, Neck, Head, Clavicle_L/R, Arm_L/R, Forearm_L/R, Hand_L/R, Socket_Weapon, Thigh_L/R, Shin_L/R, Foot_L/R) oraz 6 klipami animacji (`Idle`, `Walk`, `Run`, `Shoot`, `Melee_Swing`, `Inspect`).
- [x] GPU Vertex Skinning: atrybuty `location=4` (`aBoneIDs`) i `location=5` (`aBoneWeights`), tablica macierzy `uBoneMatrices[64]` w shaderach glownym i cieni, Direct State Access (DSA).
- [x] Architektura hybrydowa glTF + STL: system gniazda kosci (`Bone Socket Attachment`) dynamicznie wiazacy sztywne modele broni STL (`pipe.stl`, `weapon_pistol` itp.) bezposrednio z dlonia kosci szkieletu (`Socket_Weapon`).
- [x] Integracja z botami bojowymi `CombatBot` i `AIManager` (plynne odtwarzanie animacji chodu/stania, podpiecie broni, cieniowanie dynamiczne).
- [x] Zautomatyzowany i wizualny test 26 w `TestVerify.exe` weryfikujacy kwaterniony, slerp, ladowanie 20 kosci, animator, socket broni i renderowanie cieni (`test_skeletal_animation.bmp`).

---

## II. NAJBLIZSZE SPRINTY (SHORT-TERM / IN PROGRESS)

### 10. Fizyka Cial Sztywnych (Rigid Body Props), Beczki Wybuchowe & Destrukcja Srodowiska (Sprint 5 - 100% DONE)
- [x] Silnik cial sztywnych `LabPhysics` (`RigidBody`, `PhysicsWorld`): pelna dynamika newtonowska 3D (masa, lokalny tensor momentu bezwladnosci dla prostopadloscianu, pol-niejawna integracja Eulera, tlumienie predkosci liniowej i katowej, dynamiczne usypianie cial w spoczynku).
- [x] Detekcja kolizji OBB/AABB cial sztywnych z podlozem i brylami swiata (wielowierzcholkowa detekcja kontaktu, restytucja sprezysta, tarcie statyczne i kinetyczne).
- [x] Destrukcyjne skrzynki drewniane (`PropType::Crate`): wytrzymalosc 40 HP, autentyczna tekstura desek ze stalowymi okuciami i przekatnymi belkami w stylu Source Engine, rozpad po zniszczeniu na 7 fizycznych drewnianych desek/odlamkow o losowych predkosciach i rotacji.
- [x] Czerwone beczki wybuchowe (`PropType::ExplosiveBarrel`): detonacja po trafieniu kula, uderzeniu rura lub fali uderzeniowej; radialny impuls uderzeniowy (promien 6.5m, impuls 450 N·s, 130 obrazen), lancuchowe detonacje sasiednich beczek (reakcja lancuchowa); autentyczna tekstura z czarno-zoltymi pasami ostrzegawczymi i pierscieniami stalowymi.
- [x] Kinetyczny odrzut botow bojowych (`CombatBot`): wektor pedu `velocity`, fizyczny odrzut i lot ciala w powietrzu po smierci od pociskow RPG, strzelby lub eksplozji beczek z odbiciem od podloza.
- [x] Efekty dzwiekowe 3D: proceduralne odglosy RIFF 16-bit PCM w `LabAudio` (`SoundID::CrateBreak`, `SoundID::BarrelImpact`).
- [x] Integracja w grze (`main.cpp`): populacja mapy skrzynkami i beczkami, raycast trafien bronia palna i biala z iskrami i odpryskami drewna, wybuchy RPG oddzialujace kinetycznie na rekwizyty i postacie, dwufazowy cienkowany render.
- [x] Zautomatyzowany i wizualny test 27 w `TestVerify.exe` weryfikujacy matematyke, grawitacje, rozpad skrzynek, kaskadowe detonacje beczek oraz jakosc grafiki (`test_physics_and_destruction.bmp`).

---

### 11. Narzedzie Przecinania Bryl CSG (Clip Tool) w Edytorze Hammer & Rekwizyty Fizyczne (Sprint 6 - 100% DONE)
- [x] Silnik geometrii wypuklej CSG `LabCSG` (`CSGTool::sliceBox`, `CSGTool::sliceConvexMesh`, `computePlanarUV`, `makePlaneFrom2DLine`): wielokatowe przecinanie algorytmem Sutherland-Hodgmana, zamykanie przekroju plaszczyzny (cap triangulation) ze zgodnym zwrotem normalnych i ukladem wierzcholkow CCW.
- [x] Narzedzie Clip Tool w `LabHammer` (skrot klawiszowy `Shift+X` / `X` do cyklicznego przelaczania trybow `KeepFront`, `KeepBack`, `KeepBoth` oraz `Enter` do zatwierdzenia podzialu).
- [x] Trojwymiarowa wizualizacja plaszczyzny ciecia w rzucie 3D (punkty A i B, linia prowadzaca, polprzezroczysta kurtyna przekroju oraz wektor normalny kierunku zachowania bryly).
- [x] Format map `.labmap` z obsluga wypuklych polyhedrow (`poly_brush` / `b.type == "poly"`): bezstratny zapis i odczyt tablicy wierzcholkow (`customVertices`) z pozycjami, normalnymi, koordynatami UV i indeksami trzonu (`customIndices`).
- [x] Pelna integracja rekwizytow fizycznych (Drewniana Skrzynia `prop_crate` i Czerwona Wybuchowa Beczka `prop_barrel`) w palecie Prebuilts Hammera z dedykowanymi ikonami 2D, podgladem 3D i pelna deserializacja cial sztywnych w grze (`main.cpp`).
- [x] Zautomatyzowany i wizualny test 28 w `TestVerify.exe` weryfikujacy ciecie bryl CSG, podwojne sekwencyjne ciecie polyhedru, roundtrip serializacji `.labmap` oraz render ze zrzutem klatki (`test_csg_clipping_and_hammer.bmp`).

---

### 12. Dynamiczny System Dekali (Projective Decal System) & Odciski Strzalow (Sprint 7 - 100% DONE)
- [x] Podsystem rzutnika dekali 3D `LabDecals` (`DecalSystem`, `DecalInstance`, `DecalType`): projekcja sladow kul betonowych, przebic metalowych, plam krwi botow oraz okopcen wybuchowych bezposrednio na prostopadlosciany i polyhedry CSG.
- [x] Proceduralny generator tekstur 64x64 RGBA w pamieci (Concrete Hole z promienistymi peknieciami, Metal Puncture ze srebrnym rantem, organic Blood Splatter z satelitarnymi kroplami, Explosive Scorch z wasami fali uderzeniowej).
- [x] Zaawansowany depth bias w OpenGL 4.5 Core Profile (`glPolygonOffset(-2.0, -2.0)`, przesuniecie normalne 2.5mm, `glDepthMask(GL_FALSE)`) bez efektu z-fighting i migotania.
- [x] Bufor kolowy instancji (do 256 dekali) z plynna anizotropowa przezroczystoscia zanikania (Alpha Fadeout w ostatnich 5 sekundach zycia).
- [x] Wszechstronna integracja z gra (`main.cpp`): odpryski kul ze strzalow hitscan na swiecie, drzwiach i rekwizytach fizycznych, rozbryzgi krwi za botami na scianach i podlodze, radialne okopcenia po wybuchach rakiet RPG i detonacjach beczek.
- [x] Zautomatyzowany i wizualny test 29 w `TestVerify.exe` weryfikujacy geometrie, orientacje w przestrzeni stycznej, starzenie, rzutowanie na sciany i podloge (`test_decals_and_impacts.bmp`).

---

### 13. System Interaktywnych Terminali Swiatowych (LAB-OS v3.42) & Odryglowywanie Bram (Sprint 8 - 100% DONE)
- [x] Podsystem encji interaktywnych `LabInteractive` (`InteractiveSystem`, `InteractiveEntity`, `InteractiveType`): terminale bezpieczenstwa, klawiatury numeryczne, przelaczniki szynowe i konsole sluzowe z raycast pickingiem (zasieg 2.8m).
- [x] Pelny interaktywny system operacyjny terminala **LAB-OS v3.42** (GUI w stylu Doom 3 / System Shock / Fallout) aktywowany klawiszem `[E]`:
  - Kineskopowy ekran CRT z zielonym/cyjanowym fosforem, liniami rastra i naglowkiem stacji badawczej KORF Cryogenics.
  - Interaktywne menu klawiaturowe (`1`, `2`, `3`, `0`, `ESC`, `E`, `Backspace`):
    * `[1] AIRLOCK OVERRIDE`: Przelaczenie stanu rygla (`LOCKED` <-> `UNLOCKED`), dzwiek autoryzacji/odmowy, uruchomienie silnikow hydraulicznych bramy w swiecie 3D.
    * `[2] SECURITY LOGS`: Baza wiedzy i lore stacji (akta incydentu #0451 o wycieku cieklego azotu i buncie syntetycznych jednostek strazniczych).
    * `[3] BOT TELEMETRY`: Radar sensorowy stacji z odczytem liczby aktywnych syntetykow, uzbrojenia i stanu zagrozenia.
    * `[ESC] / [E] EXIT`: Plynne zamkniecie terminala OS i powrot do sterowania postacia.
- [x] Fizyczne ryglowanie bram w `LabMap` (`MapDoor::isLocked`): brama jest fizycznie zablokowana i nie reaguje na czujnik zblizeniowy, dopoki gracz nie uzyje terminala do autoryzacji i odryglowania.
- [x] Generowanie dynamicznej tekstury monitora 3D w swiecie gry z czytelnym napisem `LAB-OS // SEC-04`, statusem `LOCKED / UNLOCKED`, informacja o sluzie oraz pulsujacym kursorem `> PRESS [E] _` z korekcja orientacji osi pionowej OpenGL.
- [x] Rozszerzenie silnika dzwiekowego `LabAudio` o syntetyzatory proceduralne WAV (`SoundID::TerminalBeep`, `SoundID::AccessGranted`, `SoundID::AccessDenied`) z pelna weryfikacja RIFF 16-bit PCM (26 proceduralnych dzwiekow).
- [x] Zaktualizowany test 30 w `TestVerify.exe` weryfikujacy calosc: rejestracje encji, raycast picking, blokade keypadu, wejscie w tryb LAB-OS po `[E]`, przelaczenie odryglowania bramy opcja `[1]`, nawigacje do logow opcja `[2]`, powrot `[0]`, telemetrie botow `[3]`, rendering ekranu OS (`test_interactive_terminals.bmp`) i wyjscie `[ESC]`. Wszystkie 30 testow przechodza w 100%!

---

## II. NAJBLIZSZE SPRINTY (SHORT-TERM / IN PROGRESS)

### Sprint 9: Zaawansowany Post-Processing, HDR Tonemapping & Szron Wizjera (Cryo HUD Frost)
- [ ] Bufor post-processingu HDR (FBO `GL_RGBA16F` / `GL_RGB16F`) z dynamicznym tone-mappingiem Reinhard / ACES Filmic.
- [ ] Efekt szronu i zamarzania krawedzi wizjera gracza (`Cryo Frost Vignette`) narastajacy przy niskim poziomie zdrowia oraz w arktycznych sektorach `cryo_outpost`.
- [ ] Rozmycie jasnych obszarow (Screen-Space Bloom / HDR Glow) dla pociskow plazmowych, rozblyskow lufy i neonowych spawnerow broni.
- [ ] Zautomatyzowany i wizualny test 31 w `TestVerify.exe` (`test_postprocess_and_frost.bmp`).

---

## IV. DLUGOTERMINOWE KROKI (LONG-TERM)

### 1. Animacje Mimiki Twarzy i Ruch Ust (Lip-Sync)
- [ ] Wdrozenie Blend Shapes / Morph Targets w shaderach wierzcholkowych dla glowy postaci (Jaw_Open, Mouth_Narrow, Mouth_Smile).
- [ ] Automatyczny analizator fonemow / amplitudy audio generujacy ruch warg postaci w rytm kwestii dialogowych.

### 2. Dedykowana Siec Multiplayer (Authoritative Client-Server)
- [ ] Architektura klient-serwer oparta o UDP z kompresja pakietow stanu gry.
- [ ] Predykcja ruchu po stronie klienta (Client-Side Prediction) i uzgadnianie stanu (Reconciliation).
- [ ] Kompensacja opoznien (Lag Compensation) dla rejestracji strzalow hitscan.
- [ ] Tryb serwera dedykowanego bezokienkowego (Headless Server) pod Linux/Windows.

### 3. Klimat Swiata Frozen-Life
- [ ] Efekt szronu i zamarzania wizjera gracza przy krytycznym stanie zdrowia / niskiej temperaturze.
- [ ] Dynamiczny emiter zamieci snieznej (Blizzard Weather) wplywajacy na widocznosc.
- [ ] Fizyka poslizgu na powierzchniach lodowych (cryo_ice).
