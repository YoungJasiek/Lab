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

### 13. Biometryczny Skaner Siatkowki Oka (Retinal Scanner) & Odryglowywanie Sluz (Sprint 8 - 100% DONE)
- [x] Podsystem encji interaktywnych `LabInteractive` (`InteractiveSystem`, `InteractiveEntity`, `InteractiveType`): biometryczne skanery siatkowki (`RetinalScanner`), klawiatury bezpieczenstwa (`Keypad`), przelaczniki zasilania (`WallSwitch`) z raycast pickingiem w zasiegu 2.8m.
- [x] Pelna eliminacja niepotrzebnego, modalnego terminala CRT OS na rzecz wbudowanego bezposrednio w swiat 3D **Biometrycznego Skanera Siatkowki**:
  - Proceduralna tekstura 3D siatkowki oka: pierscienie kalibracyjne, zrenica, teczowka, celownik krzyzowy oraz animowana linia lasera skanujacego.
  - Interakcja pod klawiszem `[E]` wyzwalajaca sekwencje skanowania biometrycznego (1.25s) z plynna synchronizacja czasowa.
  - Po zakonczeniu autoryzacji: weryfikacja tozsamosci badacza (Dr. Vance, Poziom uprawnien 3), odtworzenie syntetycznego dzwieku autoryzacji `SoundID::AccessGranted`, odryglowanie i plynne otwarcie powiazanej bramy sluzowej (`MapDoor::isLocked = false; isOpen = true`).
  - Efektowny widzet HUD 2D ze stanem skanowania siatkowki (animowany pasek postepu, dane podmiotu i stopien dopasowania).
- [x] Naprawa bledu ujemnego zdrowia (Negative HP Bug Fix):
  - Poprawka w `LabHUD.h`: zamkniecie wartosci wyswietlanego zdrowia i pancerza kombinezonu przez `std::max(0, (int)std::ceil(...))` – eliminacja bledow pokroju `-38 HP`.
  - Pelna obsluga zgonu od detonacji beczek z paliwem: wczesniejszy kod odejmowal obrazenia bez sprawdzania progu zgonu i ustawiania `_isPlayerDead = true`. Teraz wszystkie zrodla obrazen przechodza przez ujednolicona procedure `applyDamageToPlayer()`.

---

### 14. Podsystem Skryptow Gry w Lua 5.4 (LabScript) & Separacja Logiki Gry (Sprint 8.5 - 100% DONE)
- [x] Kompilacja i integracja czystego interpretera **Lua 5.4.6** (`external/lua/onelua.c` z flaga `MAKE_LIB` i poziomem `/W3`, brak wyciekow, C++20 RAII).
- [x] Modul silnika `LabScript` (`include/LabScript.h`, `src/LabScript.cpp`): zarzadzanie instancja `lua_State*`, bezpieczne wywolywanie skryptow, dwukierunkowe callbacki C++ <-> Lua (`Lab.log`, `Lab.playSound`, `Lab.unlockDoor`, `Lab.addChatMessage`).
- [x] Glowny skrypt mechanik gry **`assets/scripts/game_mechanics.lua`**:
  - `PlayerRules`: bazowe zdrowie (150 HP), pancerz kombinezonu (50 Armor), wspolczynnik absorpcji pancerza (70%), czas respawnu (4.0s), mnoznik wybuchow beczek (0.75x).
  - `RetinalScanner`: czas trwania skanowania (1.25s), uprawniony uzytkownik (`DR. VANCE`), clearance level (3), indeks docelowej bramy (0).
  - `Pickups`: leczenie apteczki (+50 HP), amunicja skrzynki (+36 sztuk), czas odrodzenia broni na padzie (60s).
  - `Weapons`: zewnetrzny balans wszystkich 9 broni (obrazenia, fireRate, pojemnosc magazynka, rezerwa, obrazenia obszarowe RPG/Plasma, odrzut).
  - Czysta funkcja Lua `CalculateDamage(incomingDamage, currentArmor, currentHealth)` wyliczajaca absorpcje pancerza oraz flage smiertelnosci `isLethal`.
- [x] Refaktoryzacja `main.cpp`:
  - Wdrozenie `applyDamageToPlayer(rawDamage, sourceName)`: wywolanie funkcji Lua `CalculateDamage`, aktualizacja stanu pancerza i HP, obsluga smierci gracza od beczek, ostrzalu botow oraz wybuchu wlasnych rakiet RPG pod nogami.
  - Ladowanie i synchronizacja statystyk broni z pliku `.lua` przy starcie gry oraz po respawnie.
- [x] Zaktualizowany test 30 w `TestVerify.exe` weryfikujacy 100% integracji: skanowanie siatkowki, odryglowanie bramy, inicjalizacje interpretera Lua 5.4, kalkulacje obrazen w Lua, brak ujemnego HP na HUD oraz zrzut klatki weryfikacyjnej (`test_interactive_terminals.bmp`). Wszystkie 30 testow przechodza bezblednie!

---

### 15. Zaawansowany Post-Processing HDR, Bloom, Szron Wizjera & Fizyka Lodu (Sprint 9 - 100% DONE)
- [x] Rurociag post-processingu HDR (FBO `GL_RGBA16F`, Direct State Access - DSA, format polprecyzji 16-bit float):
  - Modul `LabPostProcess` (`include/LabPostProcess.h`, `src/LabPostProcess.cpp`).
  - Renderowanie calej sceny 3D (geometria szczotek, modele STL, boty, pociski, debrisy, czastki) w przestrzeni unclamped HDR.
- [x] Dwufazowe rozmycie gaussa (Separable Multi-Pass Gaussian Bloom):
  - Ekstrakcja nasyconych luminancji (`luminance > 0.95`).
  - Ping-pongowe bufory ramki w polowicznej rozdzielczosci (`W/2`, `H/2`) dla plynnego rozmywania blaskow pociskow plazmowych, rozblyskow lufy i spawnerow.
- [x] Algorytmy mapowania tonow (Tonemapping):
  - ACES Filmic tonemapper zachowujacy filmowy kontrast i nasycenie barw.
  - Alternatywny Reinhard tonemapper oraz precyzyjna korekcja gamma 2.2 (`pow(mapped, 1.0/2.2)`).
- [x] Cryo HUD Frost Vignette:
  - Dynamicznie generowany fraktalny szron krawedzi wizjera w shaderze kompozytowym.
  - Stopien zaszronienia sterowany poziomem zdrowia gracza (`low HP` krytyczna hipotermia) oraz arktycznym biometrem sektorow `cryo_outpost`.
- [x] Fizyka lodu i inercji:
  - Detekcja podloza lodowego (`cryo_ice` / `snow_frost`).
  - Wspolczynnik tarcia lodu `0.985f` (konserwacja 86% pedu po 10 tykach zamiast natychmiastowego zatrzymania `0.85f`) z plynna akceleracja driftu.
- [x] Dynamiczna zamiec sniezna (`_particleSystem.spawnAmbientWeather` z ukladem 3D, zawirowaniem wiatru i kolizjami).
- [x] Zautomatyzowany i wizualny test 31 w `TestVerify.exe`:
  - Weryfikacja FBO `GL_RGBA16F`, kompresji ACES i Reinhard, formul szronu, inercji lodu.
  - Wygenerowana i zwalidowana klatka referencyjna `test_postprocess_and_frost.bmp` oraz `web/assets/img/test_postprocess_and_frost.png`. Wszystkie 31 testow przechodza w 100%!

---

## II. NAJBLIZSZE SPRINTY (SHORT-TERM / IN PROGRESS)
*Wszystkie zaplanowane sprinty I fazy silnika Lab (Sprint 1 - 9) zostaly w 100% zrealizowane i zweryfikowane automatycznymi testami.*

---

## IV. DLUGOTERMINOWE KROKI (LONG-TERM - 100% DONE)

### 1. Animacje Mimiki Twarzy i Ruch Ust (Lip-Sync & Blend Shapes - 100% DONE)
- [x] Wdrozenie Blend Shapes / Morph Targets w geometrii wierzcholkow glowy (`Jaw_Open`, `Mouth_Narrow`, `Mouth_Smile`):
  - Modul `LabFace` (`include/LabFace.h`, `src/LabFace.cpp`).
  - Struktura `MorphTarget` przechowujaca wektory przemieszczen pozycji i normalnych.
  - Klasa `FacialMesh` z dynamicznym buforem VBO (OpenGL 4.5+ DSA) i wielowatkowym sumowaniem wag ksztaltow.
  - Proceduralny generator humanoidalnej glowy z cechami anatomicznymi (zuchwa, wargi, policzki, luk brwiowy, nos).
- [x] Modul `LipSyncEvaluator`:
  - Analizator obwiedni amplitudy audio (RMS / Envelope Follower) w czasie rzeczywistym.
  - Wygładzanie dynamiki mowy (asymetryczny Attack/Decay smoothing: szybkie otwieranie warg, plynne opadanie).
  - Obliczanie energii RMS bezposrednio z buforow 16-bit PCM (np. strumieni miniaudio).
  - Proceduralny generator sciezki mowy z pauzami miedzywyrazowymi dla kwestii radiowych NPC ("Dr. Vance").
- [x] Zautomatyzowany i wizualny test 32 w `TestVerify.exe`:
  - Weryfikacja 425 wierzcholkow glowy, 3 celow blend shape, matematyki przemieszczenia zuchwy, obwiedni audio RMS.
  - Wygenerowana i zwalidowana klatka referencyjna `test_facial_lipsync.bmp` oraz `web/assets/img/test_facial_lipsync.png`.

### 2. Dedykowana Siec Multiplayer (Authoritative Client-Server - 100% DONE)
- [x] Modul sieciowy `LabNetwork` (`include/LabNetwork.h`, `src/LabNetwork.cpp`):
  - Przenosna abstrakcja gniazd UDP (`UDPSocket`, `SocketAddress`) z obsluga Winsock2 (`ws2_32.lib`) na Windows i POSIX na Linux.
  - Binarny protokol pakietow (`NET_MAGIC = 0x4C414231` "LAB1", wersja 1):
    - `NetHeader`, `NetMsgConnectRequest`, `NetMsgConnectResponse`, `NetMsgPingPong`, `NetUserCmd`, `NetServerSnapshot`, `NetChatMessage`.
- [x] Autorytatywna fizyka i predykcja:
  - Klasa `ClientPrediction`: bufor pierscieniowy wyslanych polecen `NetUserCmd`, lokalna predykcja z zerowym opoznieniem (0ms lag).
  - Algorytm uzgadniania stanu (Server Reconciliation): wykrywanie desynchronizacji powyzej progu bledu, automatyczne cofniecie pozycji do autorytatywnego punktu serwera i ponowna symulacja niepotwierdzonych polecen (brak teleportacji i stutteringu).
  - Klasa `DedicatedServer`: petla serwera o stalym taktowaniu 64 Hz, autorytatywna symulacja ruchu, wykrywanie timeoutow klientow (>6s), rozglaszanie `NetServerSnapshot`.
  - Klasa `NetworkClient`: handshake polaczenia, probkowanie komend z wejscia uzytkownika, pomiar czasu RTT (ping).
- [x] Dedykowany serwer bezokienkowy (`LabServer.exe`):
  - Osobny target CMake (`src/server_main.cpp`) dzialajacy w czystym trybie konsolowym CLI (bez kontekstu GLFW/OpenGL).
  - Obsluga flag wiersza polecen (`-port <27015>`, `-map <map_name>`), graceful shutdown pod sygnale Ctrl+C / SIGINT, raporty heartbeat co 10 sekund.
- [x] Integracja z menu gry w `src/main.cpp`:
  - "START SERVER / LAUNCH MATCH": uruchomienie lokalnego serwera na porcie 27015 i polaczenie klienta.
  - "CONNECT TO SERVER": natychmiastowe polaczenie klienta pod adres docelowy `127.0.0.1:27015`.
  - Rejestracja pakietow i uzgadnianie predykcji ruchu gracza bezposrednio w `onUpdate()`.
- [x] Zautomatyzowany test 33 w `TestVerify.exe`:
  - Weryfikacja uruchomienia serwera na porcie 27019, handshake klienta, transmisja pakietow UserCmd, odbior snapshotow z autorytatywna pozycja, rozwiazanie 3.5-metrowego desyncu przez modul ClientPrediction, bezpieczne rozlaczenie. Wszystkie 33 testy przechodza w 100%!

### 3. Remaster Klimatu Post-Apo, Fizyki Botów, Post-Processingu i Dynamicznego Server Browsera (100% DONE)
- [x] **Trwałe doklejenie botów do podłoża (Zero lewitacji):**
  - Poprawiono `LabCollision::moveAndSlide` dla botów (`eyeHeight = 0.0f` zamiast `1.6f`), likwidując błąd unoszenia stóp bota 1.6m nad ziemię.
  - Wymuszono podłoże `botSpawnPos.y = 0.0f` w `AIManager::spawnBotsForMap`.
  - Weryfikacja w teście 9 i 31: buty botów stoją stabilnie na posadzce/śniegu.
- [x] **Nowy organiczny post-processing i filmic color grading:**
  - Całkowicie wyeliminowano ordynarny 90-pikselowy szum blokowy `floor(p)` i prostokątne ramki.
  - Zaimplementowano gładką eliptyczną winietę z drobnokrystalicznym fraktalnym szronem, pojawiającym się organicznie i półprzezroczyście wyłącznie przy spadku HP poniżej 40.
  - Dodano mroczny, surowy color grading w klimacie Half-Life 2 Beta / S.T.A.L.K.E.R. (zmiażdżone cienie `pow(mapped, 1.10)`, zimna desaturacja ruin `mix(luma, mapped, 0.82)`, subtelne ziarno filmowe).
- [x] **Wolumetryczna mgła odległościowa (Distance Blizzard Fog) i mroczny klimat:**
  - Dodano do `defaultFragmentShaderSrc` i `Renderer::setFog()` kwadratową mgłę odległościową (`uFogColor = (0.05, 0.07, 0.10)`, zasięg 10-75m).
  - Zmieniono kolor tła w silniku na ciemny, zamieciowy granat polarny (`0.05, 0.07, 0.10`).
  - Przebudowano mapę `cryo_outpost.labmap`: usunięto wiszącą małpę `Model.stl`, dodano zniszczoną industrialną bazę bunkra, zardzewiałe stalowe dźwigary i zapory, oraz zimne, polarne oświetlenie burzowe.
- [x] **Dynamiczny Server Browser po UDP LAN:**
  - Dodano pakiety `ServerQuery` (0x09) i `ServerInfo` (0x0A) w `LabNetwork`.
  - Serwer `DedicatedServer` natychmiast odpowiada na zapytania pakietem zawierającym nazwę serwera, mapę, tryb gry, liczbę graczy oraz limit.
  - Zaimplementowano klasę `ServerBrowser` automatycznie odpytującą sieć LAN i localhost co 1s.
  - Menu `JoinGame` w `src/main.cpp` dynamicznie renderuje wykryte uruchomione instancje (np. `LabServer.exe`) z realnym zielonym pingiem (2ms), informacjami o mapie i pozwala na bezpośrednie kliknięcie i połączenie.

### 4. Animacje Chodzenia z Bronią, Przełącznik w LabStudio, Ludzki Recoil Bota i Modularna Dokumentacja (100% DONE)
- [x] **Animacja chodzenia z bronią (Tactical Walking Kinematics):**
  - Zaimplementowano dynamiczny roll i pitch broni podczas chodu (`WeaponAnimator::calculateRotationOffset`), harmoniczny roll sway (`1.8 deg`) i pitch bob (`1.2 deg`) zsynchronizowane z krokami.
  - Zsynchronizowano ruch modelu broni i proceduralnych rąk w widoku FPP oraz w LabStudio.
- [x] **Przełącznik animacji w LabStudio (Animation Preview & Kinematics):**
  - Przyciski `IDLE`, `WALK`, `SHOOT`, `RELOAD`, `INSPECT` w prawym panelu Studio.
  - Przycisk `PLAY` / `PAUSE` (obsługiwany również spacją w zakładce GripPoser) oraz płynny suwak prędkości odtwarzania (`0.25x - 2.50x`).
  - Dynamiczne wykrywanie i lista klipów (`CLIPS:`): automatyczne ładowanie i renderowanie przycisków dla wszystkich klipów z pliku `.glb` / Mixamo (`Idle`, `Walk`, `Shoot`, `Inspect`, itp.) - użytkownik może dodawać własne animacje bez edycji kodu C++.
- [x] **Ludzkie strzelanie botów (Human-Like Shooting & Recoil Arc):**
  - Wydłużono łuk animacji strzału do 280ms (`shootAnimTimer = 0.28f`), likwidując przedwczesne urywanie animacji przy gaśnięciu błysku lufy (80ms). Bot unosi lufę, absorbuje odrzut i płynnie wraca na cel jak człowiek.
  - Zaimplementowano dedykowaną kadencję, pojemności magazynków, czasy przeładowania (`reloadTimer`) oraz unikalne dźwięki 3D dla każdej z 9 broni bota (Minigun, Shotgun, Pistol, Railgun, RPG, itp.).
  - Zaktualizowano pętlę animacji szkieletowej w `AIManager::update` i `CombatBot::update`.
- [x] **Rozbicie i aktualizacja dokumentacji silnika (`docs/`):**
  - Utworzono modułowy katalog `docs/` z 10 szczegółowymi artykułami technicznymi:
    - `docs/README.md` — Główny spis treści i hub nawigacyjny.
    - `docs/ARCHITECTURE.md` — Standardy C++20, RAII, OpenGL 4.5+ DSA, stałokrokowa pętla 64Hz.
    - `docs/CHARACTER_STUDIO.md` — LabStudio, Grip Poser, Bot Socket, ADS, Timeline, Facial Morphs, Anim Switcher.
    - `docs/SKELETAL_ANIMATIONS.md` — glTF 2.0 / GLB, szkielet 35 kości T-800, GPU skinning, ludzki łuk odrzutu.
    - `docs/BOT_AI_SYSTEM.md` — Maszyna stanów, LOS ray-AABB, arsenał, kadencja, taktyczne przeładowania, zrzut broni.
    - `docs/WEAPONS_AND_VIEWMODEL.md` — 9 broni, sprężynowo-tłumikowy odrzut, chodzenie z kołysaniem, cryo-hands.
    - `docs/MULTIPLAYER.md` — Serwer autorytatywny 64Hz UDP, predykcja klienta, uzgadnianie stanów, LAN browser.
    - `docs/LEVEL_EDITOR_HAMMER.md` — LabHammer 3D, wycinanie CSG, encje, specyfikacja `.labmap`.
    - `docs/AUDIO_SYSTEM.md` — miniaudio, przestrzenne 3D, 26 syntezowanych dźwięków PCM, analiza RMS i lip-sync.
    - `docs/LUA_SCRIPTING.md` — Lua 5.4.6, statyczna integracja, tabele balansu, skaner siatkówki.
  - Zaktualizowano główny `README.md` o odnośniki i tabelę celów silnika.
- [x] **Kompleksowa weryfikacja (Test 42):**
  - 42/42 testów zaliczonych w 100% sukcesem w `TestVerify.exe`.
  - Zapisano klatkę weryfikacji wizualnej `test_studio_anim_and_human_shooting.bmp`.
