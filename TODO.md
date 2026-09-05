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

### 5. Zestaw Testow Automatycznych (TestVerify.exe)
- [x] 22 zautomatyzowane zestawy testow integracyjnych i jednostkowych weryfikujace poprawnosc matematyki, renderera, walki, kolizji i edytora, wraz z eksportem klatek graficznych BMP.

---

## II. NAJBLIZSZE SPRINTY (SHORT-TERM / IN PROGRESS)

### Sprint 1: System Dzwieku Przestrzennego 3D (Audio Engine)
- [ ] Integracja lekkiej, nowoczesnej biblioteki audio C/C++ (np. miniaudio lub OpenAL Soft).
- [ ] Udzwiekowienie 9 broni:
  - Dzwieki wystrzalu dla kazdej broni (Pistol, Shotgun, M4A4-S, SG553, Minigun, Plasma, Railgun, RPG).
  - Dzwiek zamachu i uderzenia rura w sciany/wrogow (Pipe swing & blunt impact).
  - Odglosy pustego magazynka (Dry fire click) i przeladowania.
  - Odglosy wybuchow rakiet, swistu plazmy i wiazki magnetycznej railguna.
- [ ] Dzwieki krokow gracza i botow (Footsteps) zroznicowane wedlug materialu podloza (beton, blacha, lod, snieg).
- [ ] Odglosy podnoszenia przedmiotow (pickup amunicji, apteczki, odglos odrodzenia broni na podestach).
- [ ] Pozycjonowanie przestrzenne 3D (3D spatial audio) – tlumienie z odlegloscia, panorama stereo wg orientacji kamery gracza.
- [ ] Dzwieki otoczenia kompleksu badawczego Alpha (mrozny wiatr, buczenie transformatorow).

### Sprint 2: Rece Gracza FPP & Zaawansowane Stany Animacji Broni
- [ ] Wprowadzenie widocznych rak gracza w widoku FPP (rekawice bojowe / kombinezon taktyczny Frozen-Life) trzymajacych chwyt broni.
- [ ] System gniazda dloni (Socket Bone Attachment) – plynne wiazanie modeli STL z ruchem dloni.
- [ ] Pelna sekwencja animacji przeladowania (Reload): opuszczenie broni, wyjecie magazynka, wlozenie nowego, odciagniecie zamka i powrot na linie celowania.
- [ ] Animacja inspekcji broni (Inspect) na klawisz F (obrot broni w dloniach, ogladanie modelu z boku).
- [ ] Dynamiczny luk zamachu rura (Heavy Melee Slash) z efektem smugi ruchu (Motion Trail).

### Sprint 3: Oswietlenie w Czasie Rzeczywistym & Cienie (Shadow Mapping)
- [ ] Latarka gracza w stylu Half-Life 2 (Spotlight ze stozkiem swiatla i dynamicznym rzucaniem cieni).
- [ ] Dynamiczne mapy cieni (Shadow Mapping / CSM) dla glownego oswietlenia kierunkowego (slonce / swiatlo bazowe).
- [ ] Modul statycznego wypalania oswietlenia (Baking Lightmaps) w geometrii Hammera dla klimatycznych cieni w korytarzach.

---

## III. SREDNIOTERMINOWE KROKI (MID-TERM)

### 1. Import Modeli Szkieletowych glTF 2.0 (Blender Pipeline)
- [ ] Integracja loadera glTF 2.0 / GLB (np. cgltf) dla siatek ze szkieletem.
- [ ] Vertex Skinning na GPU w shaderze GLSL (layout(location=4) in uvec4 aBoneIDs, in vec4 aBoneWeights, tablica uBoneMatrices).
- [ ] Architektura hybrydowa: animowane postacie i rece z glTF + wymienne statyczne modele broni ze sztywnego formatu STL w gniazdach kosci.
- [ ] Maszyna stanow animacji postaci (Animation Blending: Idle -> Walk -> Run -> Jump -> Shoot -> Death).

### 2. Rozszerzenie Narzedzi Geometrii w Hammer Editorze
- [ ] **Clip Tool (Shift+X):** narzedzie przecinania bryl plaszczyzna na dwie niezalezne czesci.
- [ ] **Carve Tool (CSG Subtraction):** wycinanie otworow na drzwi i okna w scianach.
- [ ] **Vertex Manipulation Tool:** bezposrednia edycja wierzcholkow pedzli (tworzenie ramp, schodow i skosnych sufitow).

### 3. Fizyka Cial Sztywnych & Destrukcja
- [ ] Ciala sztywne (Rigid Body Physics) dla rekwizytow (skrzynki, beczki wybuchajace podlegajace pedowi i grawitacji).
- [ ] Odrzut cial botow po zgonie od eksplozji RPG (kinetyczna reakcja zamiast statycznego lezenia).

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
