# Lab Engine: plan naprawy i rozwoju

Ten dokument jest źródłem prawdy dla pracy nad silnikiem. Zasady dla narzędzi AI są w `AGENTS.md` oraz `.agents/skills/FL_SKILL/SKILL.md`. Szczegółowa techniczna lista błędów w kodzie i bezpieczeństwie znajduje się w `tech_todo.md`. Stan opisany poniżej pochodzi z bezpośredniego przeglądu kodu i weryfikacji klatek testowych.

---

## 0. Cel i zakres

**Tożsamość projektu:**
- **Frozen-Life SDK (oparty na Lab Engine)** to wyspecjalizowany pakiet SDK oraz silnik FPS w stylu **Source Engine (Half-Life 2 / wczesne demo E3 2003)** (C++20, OpenGL 4.5+ Core DSA, Blinn-Phong, normal mapy, cienie PCF, audio 3D, Source bunny-hop, dev console, narzędzia ImGui).
- **Frozen-Life** to oficjalna gra flagowa oraz wzorcowy moduł gry (`IGameModule`), stanowiący bazę pod tworzenie map i modów.
- **Narzędzia autorskie SDK**: `LabEditor` (Hammer 4.1), `LabStudio` (HLMV), `LabServer` (serwer dedykowany), skrypty CLI (`scripts/run_sdk.py`, `scripts/verify_pipeline.py`).

**Najbliższy kamień milowy (DEADLINE: 20 GRUDNIA 2026):**
- **Pierwsze Małe Grywalne Demo Technologiczne (Vertical Slice / Demo E3 2003 Style):**
  - **Główny priorytet:** Grafika i wierność wizualna Source Engine (plastyczne oświetlenie, shader oka postaci, podpięcie wypieczonych lightmap w shaderze, refrakcja wody, kable/detal w geometrii, brak wrażenia pustego boxa).
  - **Warstwa gameplayowa demka:** Jedna dopracowana, zwarta mapa pokazowa (arktyczny posterunek kriogeniczny / `cryo_outpost`), stabilna pętla FPP, interakcja z sojusznikiem (strażnik otwierający śluzę kartą dostępu), potyczka z kilkoma wrogimi synthami i sekwencja ucieczki.
  - **Wydajność:** Stabilne 60+ FPS przy 1080p na OpenGL 4.5 Core DSA.

**Poza zakresem do 20 grudnia (odłożone na 2027 / Fazę 5):**
- Zaawansowany NavMesh i dynamiczne wyszukiwanie ścieżek A* (obecnie wystarczają waypointy i raycasty).
- Pełny wielomięśniowy FACS / Face Poser (zmarszczki, 40 flexów twarzy – lipsync pozostaje na obwiedni RMS audio).
- Fizyczny ragdoll ciał z kolizjami kości (zostaje dynamiczny knockback i animacje śmierci meshów).
- Blend Trees, Inverse Kinematics (IK), pojazdy z fizyką zawieszenia, streaming terenu, Vulkan, DirectX 12.
- Duże, ryzykowne podziały architektoniczne monolitu, które mogłyby zdestabilizować silnik tuż przed demkiem.

---

## 1. Stan faktyczny (Weryfikacja z kodu – Październik 2026)

| Obszar | Stan | Dowód / Plik w kodzie | Ocena statusu |
|---|---|---|---|
| **Rozmiar bazy kodu** | ~68 tys. linii C++ | suma `src/` + `include/` | Monolit rośnie wraz z nowymi funkcjami Hammera i testów |
| **Monolit testów** | `src/test_verify.cpp`, 11 800+ linii | jeden plik, 42 klatki weryfikacji wizualnej | Stabilny, ale monolit |
| **Monolit edytora** | `src/editor_main.cpp`, 11 150+ linii | zintegrowany Hammer 4.1 + Entity I/O + Waypoints | Stabilny, ale monolit |
| **Backendy RHI** | Tylko OpenGL 4.5 Core DSA | `src/LabRHI.cpp:44` (Vulkan/DX12 to atrapy `MockDevice`) | Zgodne z celem (Vulkan/DX12 poza zakresem) |
| **Shader sceny** | Blinn-Phong + Half-Lambert Diffuse + 6-Axis Ambient Cube | `src/LabRenderer.cpp:240-265` | **ZROBIONE** (wierny styl Source) |
| **Normal mapping** | Tangent-space Sobel Normal Maps + Specular Mask | `src/LabRenderer.cpp:145-163, 973-1020` | **ZROBIONE** (aktywne w shaderze i testach) |
| **Cienie dynamiczne** | 2048 DSA FBO + Interleaved Gradient Noise 16-sample PCF | `src/LabRenderer.cpp:173-200` | **ZROBIONE** (miękkie cienie latarki i słońca) |
| **Post-processing** | Bloom, ACES Tonemapping, Vignette, Frost, Color Grading | `src/LabPostProcess.cpp:76-230` | **ZROBIONE** (kontrast/nasycenie/szron) |
| **Anti-aliasing** | FXAA 3.11 (Edge Contrast Blend) | `src/LabPostProcess.cpp:129-158`, Test 71 | **ZROBIONE** (podstawowe wygładzenie) |
| **Lightmapy** | Klasa `BakedLightmap` i `LightmapBaker` istnieją na CPU/GPU | `src/LabGraphics.cpp:472-610` | **DO PODPIĘCIA / FIX** (brak w shaderze `LabRenderer.cpp`) |
| **Shader oka postaci** | Brak specjalnego shadera oka (statyczna tekstura głowy) | `assets/models/guard.glb`, `LabSkeletal.cpp` | **DO ZROBIENIA NA DEMO** (priorytet E3 2003) |
| **Woda z refrakcją** | Płaska tekstura / brak zniekształcenia du/dv | `LabRenderer.cpp` | **DO ZROBIENIA NA DEMO** (Fresnel water pass) |
| **Debug OpenGL** | `GL_DEBUG_OUTPUT` i `glDebugMessageCallback` włączone | `src/LabRenderer.cpp:1577-1586` | **ZROBIONE** (aktywne w Debug) |
| **CI na Linuxie** | Job Ubuntu w GitHub Actions (GCC/Clang, Xvfb + Mesa) | `.github/workflows/c-cpp.yml:51-102` | **ZROBIONE** |
| **Animacje botów** | 128 kości GPU skinning, 19 klipów FBX, 4 archetypy, LOD | `src/LabAI.cpp`, `src/LabSkeletal.cpp`, `LabRenderer.cpp` | **ZROBIONE** (Guard, Scientist, Worker, Synth) |
| **Entity I/O & Triggery** | Wyzwalacze, drzwi z zależnościami, karty chipowe, linki I/O | `include/LabMap.h`, `src/LabMap.cpp`, `editor_main.cpp` | **ZROBIONE** |
| **Dziennik Hammera** | System Undo/Redo oraz sesja awaryjna w `%TEMP%` | `src/editor_main.cpp`, Test 92 | **ZROBIONE** |
| **Fizyka rekwizytów** | Newtonian Rigid Bodies (skrzynie, beczki z odłamkami) | `src/LabPhysics.cpp`, Test 68 | **ZROBIONE** (podstawowa destrukcja) |
| **Ragdoll** | Uproszczony knockback i animacje zgonu | `src/LabAI.cpp` | **DOBRZE NA TERAZ** (pełny ragdoll kości po 20 grudnia) |
| **Nawigacja AI** | Waypointy z rotacją i akcjami w Hammerze | `include/LabMap.h`, `src/LabAI.cpp` | **DOBRZE NA TERAZ** (NavMesh po 20 grudnia) |

---

## 2. Zasady pracy

1. **Jeden cel nadrzędny:** Każde zadanie do 20 grudnia musi bezpośrednio przybliżać demo pokazowe lub stabilizować silnik.
2. **Nigdy nie pracuj bezpośrednio na `main`.** Jedno zadanie = gałąź `ai/<nazwa>` lub `feat/<nazwa>`.
3. **Prawda w statusach:** Nie oznaczaj atrap ani placeholderów jako gotowe. Jeśli funkcja istnieje w kodzie, ale nie jest podpięta pod render sceny (np. `BakedLightmap`), ma status **[DO PODPIĘCIA / FIX]**.
4. **Najpierw test, potem zmiana.**
5. **Styl wizualny:** Blinn-Phong, Half-Lambert, 6-Axis Ambient Cube, ostre normal mapy, miękkie cienie PCF, brak fotorealistycznego PBR, brak TAA.

---

## 3. Zaktualizowane Fazy i Zadania

---

### Faza 1: Higiena i stabilność platformowa (Ukończona w 80%)

- [x] **1.1 Ścisłe flagi kompilacji dla krytycznych modułów** (`src/LabSpriteBatcher.cpp`, `LabVFS.cpp`, `LabRHI.cpp`, `LabPostProcess.cpp`, `LabGraphics.cpp`, `LabPhysics.cpp` z `/WX` / `-Werror`).
- [x] **1.2 CI na Linuxie** (`ubuntu-latest` z GCC i Clang, Mesa llvmpipe + Xvfb pod OpenGL 4.5).
- [x] **1.5 Debug OpenGL** (`glDebugMessageCallback` i `GL_DEBUG_OUTPUT` aktywne w `Renderer::init()`).
- [ ] **1.3 Sanitizery (ASan/UBSan w CI)** – dodać dedykowany job w GitHub Actions [DO ZROBIENIA].
- [ ] **1.4 Przegląd kopiowalności klas z surowymi wskaźnikami** – zabezpieczenie destruktorów [W TRAKCIE].

---

### Faza 2: Testy i weryfikacja wizualna (Działa, reorganizacja po demku)

- [x] **2.1 Pipeline weryfikacji wizualnej** (`scripts/verify_pipeline.py` generujący 42 klatki `.png` z weryfikacji OpenGL).
- [x] **2.2 Testy serializacji formatów** (`.labscene`, `.labmap`, `.labpak`, `.labproject`, `.anim`).
- [x] **2.3 Testy fizyki i kolizji** (CCD, AABB, sweep sphere vs AABB, niszczenie skrzyń i beczek).
- [x] **2.4 Testy loaderów** (glTF 2.0 / GLB, BMP, PNG, odrzucanie uszkodzonych nagłówków).
- [ ] **2.5 Fizyczny podział `test_verify.cpp` na pliki per moduł w `tests/`** – **[ODŁOŻONE NA PO 20 GRUDNIA]**, by nie ryzykować regresji pipeline'u testowego tuż przed demkiem.

---

### Faza 3: Architektura i narzędzia SDK (Zamknięte pod kątem demka)

- [x] **3.1 Hammer Level Editor (`LabEditor`)**:
  - Narzędzia CSG (Sutherland-Hodgman clipping, generowanie zatyczek).
  - Właściwości brushy: UV Tiling scale (0.125, 0.25, 0.5, 1.0), miniatury tekstur, przypisywanie modeli STL.
  - Undo/Redo Engine i autosave w `%TEMP%`.
  - Przycisk szybkiego uruchomienia mapy `[F9] Compile & Run in Game`.
- [x] **3.2 Model & Weapon Studio (`LabStudio`)**:
  - Poprawione kadrowanie kamery orbitalnej postaci i broni.
  - Oś czasu animacji (Walk, Idle, Shoot, Reload, Inspect, Death).
  - Weapon Grip Studio: na żywo regulowane offsety i kąty chwytu broni w dłoniach FPP.
- [x] **3.3 Logika Entity I/O**:
  - Mechanizm wyzwalaczy powiązanych z drzwiami, terminalami i celami.
  - Warunki: zbliżenie, użycie manualne, wymagane karty chipowe (`keycard_l1`), agregaty zasilające.
- [x] **3.4 Multi-archetype Bot System i LOD**:
  - Strażnik (`guard.glb`), Naukowiec, Technik, Combat Synth.
  - Dynamiczny LOD (LOD0, LOD1, LOD2) redukujący narzut obliczeniowy riga kości.
- [ ] **3.5 Rozbicie monolitu `editor_main.cpp` na pliki per workspace** – **[ODŁOŻONE NA PO 20 GRUDNIA]** (kod edytora jest stabilny, refaktor nie blokuje demka).

---

### Faza 4: Grafika i prezentacja wizualna (GŁÓWNY PRIORYTET – DEADLINE: 20 GRUDNIA 2026)

Cel: Doprowadzić jakość obrazu i scenografii do poziomu legendarnego zwiastuna **Half-Life 2 (E3 2003 Showcase)**.

#### 4.1 Oświetlenie i materiały (W toku)
- [x] **Normal mapping w przestrzeni stycznej:** Sobel Normal Maps + TBN Gram-Schmidt bez szwów derywacyjnych.
- [x] **6-Axis Ambient Cube & Half-Lambert:** Miękkie oświetlenie otoczenia z 6 stron sześcianu barw.
- [x] **Cienie reflektora i słońca:** 2048 DSA FBO z 16-próbkowym PCF i obrotem gradientowym IGN.
- [x] **Color grading & Postprocess:** Bloom, ACES tonemapping, winieta, nasycenie i kontrast.
- [x] **Anti-aliasing:** Natywny pass FXAA 3.11.
- [ ] **4.2 Podpięcie Lightmap do shadera sceny [FIX / PRIORYTET WYSOKI]:**
  - Podpiąć `BakedLightmap` pod drugi zestaw współrzędnych UV (`aTexCoords2`) w `LabRenderer.cpp`.
  - Zmiksować oświetlenie statyczne z lightmapy z dynamicznym oświetleniem punktowym i latarką.
- [ ] **4.3 Dwuwarstwowy Shader Oka (The Source Eye Benchmark) [NOWE / PRIORYTET WYSOKI]:**
  - Twarze postaci nie mogą wyglądać jak manekiny z namalowanymi oczami.
  - Osobny pod-shader lub pass dla oczu: wypukła, przezroczysta rogówka z punktowym odblaskiem światła (*corneal specular glint*), głębia tęczówki i prosty kod śledzenia wzrokiem gracza (*eye saccades*).
- [ ] **4.4 Woda z refrakcją Fresnela (Source Water Pass) [NOWE / PRIORYTET ŚREDNI]:**
  - Tafla wody (np. podziemny zbiornik chłodzący w placówce kriogenicznej).
  - Zniekształcenie refrakcji dnem z użyciem normalnej wody i współczynnik Fresnela łączący dno z odbiciem otoczenia.
- [x] **4.5 Generator wiszących kabli i detal [CZĘŚCIOWO ZAIMPLEMENTOWANE]:**
  - [x] Generator geometrii kabli łańcuchowych/parabolicznych (CableGenerator w include/LabCable.h, src/LabCable.cpp).
  - [ ] Wstawianie kabli przez encje w Hammerze (lhmr.exe).
  - [ ] Rury industrialne z zaokrąglonymi kolankami wzdłuż sufitów.

---


### Narzędzia i Ekosystem Binarek (Zunifikowane krótkie nazewnictwo)
- **game.exe** – Główny klient gry (Frozen-Life / Lab Runtime).
- **lhmr.exe** – Edytor świata i geometrii (Lab Hammer).
- **lchs.exe** – Studio konfiguracji broni, postaci i botów (Lab Character Studio).
- **lfm.exe** – Dedykowane studio filmowe i sekwencer kinowy (Lab FilmMaker):
  - Wizualna, w pełni interaktywna oś czasu ze scrubbingiem na żywo (LPM click & drag).
  - Interpolacja kamery krzywymi Catmull-Rom oraz podwójny viewport (reżyserski + roboczy ze szkieletem i gizmem).
  - Trzyzakładkowy panel zasobów Lab Director Studio (hierarchia sceny, biblioteka 23 animacji FBX z podglądem, biblioteka audio WAV).
  - Wbudowane okno wyboru i ładowania map (.labmap / .labscene) bezpośrednio w programie.
  - Dedykowane okno eksportu wideo z opcjami rozdzielczości (720p/1080p/4K), klatkażu (24/30/60 FPS), zakresu i płynnym potokiem renderowania FFmpeg bez zawieszania UI.

### Faza 4B: Mapa pokazowa małego demka (DEADLINE: 20 GRUDNIA 2026)

- [ ] **Jedna spójna mapa kampanii: `cryo_outpost_showcase.labscene`:**
  1. **Akt 1 (Wprowadzenie):** Gracz budzi się w arktycznym posterunku badawczym, FPP viewmodel z kombinezonem HEV, za oknem śnieżyca i zamglony 3D skybox.
  2. **Akt 2 (Interakcja z sojusznikiem):** Strażnik (`Guard`) wita gracza, rozmawia przez radio/dialog, podchodzi do terminala i otwiera pancerną śluzę kartą dostępu (pełne wykorzystanie systemu Entity I/O i waypointów).
  3. **Akt 3 (Korytarze i potyczka):** Ciemne tunele serwisowe oświetlane latarką z ostrymi normal mapami, wyciek płynu chłodzącego (refrakcyjna woda), zniszczalne skrzynie z fizyką i potyczka z 3 wrogimi synthami.
  4. **Akt 4 (Finał demka):** Wyjście na zewnętrzną platformę lądowiska z widokiem na gigantyczny kompleks znikający we mgle.

---

### Faza 5: Zaawansowane podsystemy (PO 20 GRUDNIA 2026 / 2027)

Zadania odłożone na etap po wydaniu małego demka:
1. **NavMesh i dynamiczny A\*** (zastąpienie tras waypointowych pełną siatką nawigacyjną).
2. **Wielomięśniowy FACS / Face Poser** (pełna mimika twarzy, zmarszczki, zaawansowane fonemy).
3. **Fizyczny ragdoll szkieletowy** (symulacja fizyczna ciał z kolizjami kości po śmierci).
4. **Blend Trees i Inverse Kinematics (IK)** dla nóg postaci na nierównościach terenu.
5. **Architektoniczne rozbicie monolitu:** Wydzielenie `editor_main.cpp` na moduły workspace i rozbicie `test_verify.cpp` na `tests/*.cpp`.
6. **Streaming terenu, Displacements i zaawansowane pojazdy kołowe.**

---

## 4. Kryteria odbioru małego demka (20 Grudnia 2026)

- [ ] Build Release kompiluje się na czysto na MSVC (Windows) i GCC/Clang (Linux CI).
- [ ] Scena demonstracyjna uruchamia się stabilnie z poziomu `python scripts/run_sdk.py --game`.
- [ ] Shader oczu i oświetlenie postaci eliminuje efekt martwego manekina.
- [ ] Lightmapy wypieczone na brushach są połączone z dynamicznym spotlightem latarki.
- [ ] Stałe 60+ FPS na rekomendowanej konfiguracji GPU.
- [ ] Przejście poziomu od początku do końca bez crasha i zacięć pamięci.

---

## 5. Matryca Ryzyk i Mitygacji

| Ryzyko | Wpływ na deadline 20.12 | Plan zapobiegawczy |
|---|---|---|
| Zbyt duży zakres prac graficznych | Krytyczny | Ograniczenie do jednej dopracowanej mapy i kluczowych shaderów (oczy, lightmapy, woda). Bez zbędnych wodotrysków. |
| Rozbicie monolitu niszczy testy | Wysoki | Przesunięcie refaktoru plików `editor_main.cpp` i `test_verify.cpp` na Faza 5 (po demku). |
| Lightmapy psują rendering dynamiczny | Średni | Zastosowanie addytywnego lub modulowanego miksu w shaderze (`baseLighting = lightmap * diffuse + dynamicSpot`). |
| Spadek klatek poniżej 60 FPS | Średni | Działający system LOD dla botów oraz culling niewidocznych brushy w Hammerze. |
