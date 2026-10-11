# 📋 Lab Engine — Audyt Funkcjonalności 2D, 3D oraz Lista TODO

## 🖼️ Audyt Funkcjonalności Silnika 2D (Stan i TODO)

| Funkcjonalność | Status | Co mamy w kodzie (`include/` & `src/`) | Czego brakuje |
| :--- | :---: | :--- | :--- |
| **Renderowanie sprite’ów, warstw i z-order** | ✅ **Pełne** | `Lab::SpriteBatcher` (`LabSpriteBatcher.h/.cpp`): wsadowe rysowanie do 10 000 quadów w 1 draw-callu, dedykowany shader GLSL 450, obrót wokół pivotu (`origin`), wycinki UV i stabilne sortowanie (`std::stable_sort`) po `layer` (z-order) oraz `sequenceId`. | — |
| **Animacje sprite’ów** | 🟡 **Częściowe** | `Lab::SpriteSheet2D`, `Lab::Animation2D`, `Lab::AnimationPlayer2D` (`LabAsset2D.h/.cpp`): cięcie arkuszy na siatkę (`sliceGrid`), animacje poklatkowe, kontrola FPS/pętli oraz zapis/odczyt JSON. | ❌ Brak **animacji szkieletowych 2D** (np. Spine / 2D bone mesh deforming – szkielet w silniku jest tylko 3D). |
| **Kolizje 2D i fizyka 2D** | 🟡 **Częściowe** | `Lab::PhysicsWorld2D`, `Lab::RigidBody2D`, `Lab::AABB2D` (`LabPhysics2D.h/.cpp`): własny silnik fizyki 2D (ciała `Static`, `Kinematic`, `Dynamic`, grawitacja, impulsy, tarcie, restytucja, raycast 2D) oraz kolizje **AABB (`Box`) i okręgów (`Circle`)**. | ❌ Brak kolizji dla **wielokątów 2D** (SAT 2D) oraz brak zewnętrznej biblioteki **Box2D** (fizyka jest autorska). |
| **Kamera 2D (przewijanie, zoom, parallax)** | 🟡 **Częściowe** | `Lab::Camera` (`LabCamera.h` – `CameraProjection::Orthographic`, `InitializeCamera2D`): rzutowanie ortogonalne, płynne przewijanie X/Y, zoom (`orthographicSize`) oraz konwersja współrzędnych `screenToWorldPoint2D`. | ❌ Brak wbudowanego systemu/komponentu **parallax scrolling** dla warstw tła. |
| **Tilemapy i edytor poziomów** | 🟡 **Częściowe** | `Lab::Tilemap2D` i `Lab::TilemapLayer2D` (`LabAsset2D.h/.cpp`): wielowarstwowe tilemapy renderowane przez `SpriteBatcher`, serializacja JSON oraz tryb `EditorWorkspace::Level2D_Tilemap` w `editor_main.cpp`. | ❌ W edytorze 2D brakuje wizualnej palety kafelków i narzędzia pędzla (malowania kafli myszką na żywo). |
| **Wejście (klawiatura, mysz, dotyk, gamepad)** | 🟡 **Częściowe** | `Lab::InputActionManager` (`LabInput.h/.cpp`) i `Lab::Input` (`LabCore.h`): pełna obsługa klawiatury, myszy oraz **gamepada** (15 przycisków, 6 osi, deadzone, bindowanie akcji i zapis do pliku). | ❌ Brak obsługi **dotyku (touch / multi-touch)**. |
| **Dźwięk (odtwarzanie, miksowanie, 3D/przestrzenny)** | ✅ **Pełne** | `Lab::AudioEngine` (`LabAudio.h/.cpp`) oparty na `miniaudio.h`: miksowanie do 64 kanałów, dźwięk 2D i przestrzenny 3D z tłumieniem dystansowym (`playSound3D`, `setListener`), głośność główna + proceduralny syntezator WAV. | — |
| **System zdarzeń/sygnałów i sceny** | 🟡 **Częściowe** | `Lab::EventBus` (`LabEvent.h` – subskrypcje, natychmiastowy broadcast i kolejkowanie zdarzeń), `Lab::LabScene` (`LabScene.h`), `Lab::SceneNode` (`LabSceneNode.h` – hierarchia transformacji rodzic-dziecko) oraz `Lab::Prefab`. | ❌ Brak menedżera stosu scen (`SceneManager`) z ładowaniem addytywnym lub asynchronicznymi przejściami między scenami. |
| **Prosty system cząsteczek i efektów** | 🟡 **Częściowe** | `Lab::ParticleSystem` (`LabParticles.h/.cpp`): ogień, dym, iskry, krew, mróz, odłamki z fizyką odbić i blendingiem addytywnym/alfa. | ❌ Obecny system cząsteczek operuje na billboardach kamery 3D – brak dedykowanego emitera 2D spiętego ze `SpriteBatcher`. |
| **UI i obsługa tekstu/czcionek** | ✅ **Pełne** | `Lab::LabFont` (`LabFont.h` – rasteryzacja TrueType `stb_truetype` do atlasów 512×512 + GDI/Dot-Matrix), `Lab::LabHUD` (`LabHUD.h`) oraz wizualny edytor `Lab::LabHUDEditor` (`LabHUDEditor.h` – kotwiczenie, przyciski, paski postępu, panele, karty, ikony). | — |

---

## 🧊 Audyt Funkcjonalności Silnika 3D (Stan i TODO)

| Funkcjonalność | Status | Co mamy w kodzie (`include/` & `src/`) | Czego brakuje |
| :--- | :---: | :--- | :--- |
| **Pipeline renderowania: shadery, materiały, PBR, cienie, GI** | 🟡 **Częściowe** | `Lab::Renderer` (`LabRenderer.h`), `Lab::ShadowMap` (`LabLight.h` – cienie kierunkowe 2048×2048 z PCF 3×3), do 16 świateł punktowych + latarka, klasy `Lab::PBRMaterial` i `Lab::PBRShading` (`LabGraphics.h` – Cook-Torrance GGX + statyczne IBL z HDRI). | ❌ Domyślny shader sceny w `LabRenderer.cpp` nadal używa modelu Blinn-Phong zamiast pełnego PBR; brak dynamicznego Global Illumination (SSGI/Voxel GI). |
| **Import modeli (glTF, FBX, STL), siatki, szkielety, morfy** | ✅ **Pełne** | `Lab::GLTFLoader` (`LabSkeletal.h` – glTF 2.0 / GLB), `Lab::FBXLoader`, `Mesh::loadSTL`, skinning GPU (64 kości), gniazda kości (`getSocketTransform`) oraz `Lab::FacialMesh` (`LabFace.h`) z **Morph Targets** i lip-synciem z audio PCM. | — |
| **System animacji 3D (szkieletowe, blend trees, IK)** | 🟡 **Częściowe** | `Lab::Animator` (`LabSkeletal.h/.cpp`): interpolacja klatek (lerp/slerp), cross-fade między dwoma klipami animacji oraz proceduralna animacja rąk FPP (`Lab::ProceduralArms` w `LabArms.h`). | ❌ Całkowity brak **Blend Trees** (1D/2D blend spaces) oraz brak **odwrotnej kinematyki (IK)**. |
| **Fizyka 3D i kolizje 3D (BVH, bryły otaczające)** | 🟡 **Częściowe** | Bogata autorska fizyka: `Lab::PhysicsWorld` (`LabPhysics.h`), `Lab::CCDPhysics` (`LabCCD.h` – ciągła detekcja kolizji, `ConvexHull3D` SAT, `TriangleMeshCollider3D`), `moveAndSlide`, ragdolle (`LabRagdoll.h`), pojazdy (`LabVehicle.h`), tkaniny Verleta (`LabCloth.h`), ciecze (`LabFluid.h`). | ❌ Brak integracji z zewnętrznym silnikiem (Bullet/PhysX/Jolt) oraz brak drzew przestrzennych **BVH / Octree** (broadphase działa liniowo). |
| **Kamery 3D, system LOD i culling (frustum, occlusion)** | 🟡 **Częściowe** | Kamery perspektywiczne i ortogonalne (`Lab::Camera` w `LabCamera.h`) oraz **Frustum Culling** (test 6 płaszczyzn ostrosłupa względem AABB w `isInFrustum`). | ❌ Brak systemu **LOD** (Levels of Detail) dla siatek oraz brak **Occlusion Cullingu**. |
| **Terrain, niebo, mgła, post-processing** | 🟡 **Częściowe** | `Lab::Skybox` (`LabRenderer.h`) i `Lab::HDRISkybox` (`LabGraphics.h`), mgła odległościowa i wysokościowa (`FogSettings`), `Lab::PostProcessPipeline` (`LabPostProcess.h` – **HDR** `GL_RGBA16F`, **Bloom** Gaussowski, **Tone Mapping** ACES/Reinhard). | ❌ Brak systemu **Terrain** (`voxel_terrain.cpp` to tylko pusta zaślepka pluginu) oraz brak **Anti-Aliasingu** (FXAA/TAA/MSAA). |
| **Sztuczna inteligencja i nawigacja (navmesh, pathfinding)** | 🟡 **Częściowe** | `Lab::CombatBot` i `Lab::AIManager` (`LabAI.h/.cpp`): maszyna stanów FSM (`Patrol`, `Alert`, `Chase`, `Attack`, `Hurt`, `Dead`), patrolowanie po waypointach i test widoczności raycastem (`hasLineOfSight`). | ❌ Brak generatora **NavMesh** i algorytmu **pathfindingu (A\*)** (w edytorze jest tylko znacznik `NavMesh Obstacle Box`). |
| **Wiele platform i API graficznych (Vulkan, DX, Metal, OpenGL)** | 🟡 **Częściowe** | Pełne wsparcie **OpenGL 4.5+ Core DSA** oraz warstwa abstrakcji `Lab::IRHIDevice` i kompilator `Lab::RenderGraph` (`LabRHI.h/.cpp`). | ❌ Backendy **Vulkan, DirectX 12 i Metal** w `LabRHI.cpp` zwracają na razie atrapę (`MockDevice`). |
| **Edytor 3D, profilowanie i zarządzanie zasobami (streaming, pamięć)** | 🟡 **Częściowe** | Rozbudowany edytor 3D CSG/CAD (`Lab::LabHammerEditor` w `LabEditor.h`, `Lab::LabStudioApp` w `LabStudio.h`), `Lab::Profiler` (`LabProfiler.h` – czasy CPU, liczniki RAM/VRAM, draw calle), `Lab::VFS` (`LabVFS.h`) i `Lab::AssetRegistry` (`LabAsset.h`). | ❌ Brak **asynchronicznego streamingu zasobów** w tle (ładowanie z dysku jest synchroniczne) oraz brak własnych alokatorów aren/puli pamięci. |

---

## ⚙️ Audyt Elementów Wspólnych 2D/3D (Stan i TODO)

| Funkcjonalność | Status | Co mamy w kodzie (`include/` & `src/`) | Czego brakuje |
| :--- | :---: | :--- | :--- |
| **Pętla gry i zarządzanie czasem (delta time, fixed step)** | ✅ **Pełne** | `Lab::Engine::run` (`LabCore.h/.cpp`): precyzyjny `deltaTime`, akumulator fizyki `_physicsAccumulator` wywołujący `onFixedUpdate` ze stałym krokiem (domyślnie 64 Hz) niezależnie od FPS. | — |
| **Architektura ECS lub komponentowa** | ✅ **Pełne (Komponentowa)** | `Lab::Entity` (`LabEntity.h`) i `Lab::Component` (`LabComponent.h`, `LabComponents.h`): pełny obiektowy system komponentowy (`AddComponent<T>`, `GetComponent<T>`, cykl życia `Awake`/`Start`/`Update`/`LateUpdate`). | ℹ️ Architektura komponentowa w stylu klasycznego Unity (`std::unique_ptr<Component>`), brak czystego Data-Oriented ECS opartego o archetypy/sparse-sety. |
| **System zasobów (ładowanie, cache, referencje)** | 🟡 **Częściowe** | 128-bitowe identyfikatory GUID (`Lab::AssetID`), rejestr metadanych `Lab::AssetRegistry` (`LabAsset.h`) oraz wirtualny system plików `Lab::VFS` (`LabVFS.h`, `assets://`). | ❌ Brak centralnego cache'u z automatycznym zliczaniem referencji (smart handles / LRU) – każdy podsystem przechowuje załadowane zasoby osobno. |
| **Serializacja scen i danych** | ✅ **Pełne** | Zapis/odczyt scen `.labscene` (`LabScene.cpp`), map CSG `.labmap` (`LabMap.cpp`), projektów `.labproject`, archiwów `.labpak` (`LabPackage.h`), animacji `.anim` oraz plików JSON dla 2D. | — |
| **Skryptowanie (np. C#, Lua, GDScript)** | ✅ **Pełne (Lua 5.4)** | `Lab::ScriptEngine` (`LabScript.h/.cpp`) i `Lab::ScriptComponent` (`LabComponents.h`): wbudowana **Lua 5.4** z hot-reloadingiem, kompilacją do bytecode'u `.luac` i bindingami C++. | ℹ️ Wspierana jest wyłącznie Lua 5.4 (brak C# i GDScript). |
| **Debugowanie, logowanie i profilowanie** | ✅ **Pełne** | `Lab::LabLog` (`LabCore.h` – logi konsolowe i do pliku `lab_engine.log`), `Lab::Profiler` (`LabProfiler.h` – `ProfileScope`), rysowanie kształtów debugowych (`drawDebugLine`, `drawDebugBox`, `drawBoundingBox`) i konsola REPL w edytorze. | — |
| **Lokalizacja i obsługa wielu języków** | 🟡 **Częściowe** | `Lab::LabTheme::tr` (`LabTheme.h/.cpp`): przełączanie języków (`English` / `Polish`) w interfejsie edytora. | ❌ Słownik jest zaszyty w kodzie C++ (tylko dla UI edytora); brak ładowania zewnętrznych tabel tłumaczeń (CSV/JSON/.po) dla gier. |
| **Eksport na różne platformy** | 🟡 **Częściowe** | `Lab::ExportPipeline` (`LabExportPipeline.h/.cpp`): pełny eksport standalone dla **Windows x64** (z kompilacją skryptów Lua do bytecode'u w trybie `Shipping` i pakowaniem assetów) oraz generowanie szablonów paczek dla **Linux, macOS i WebAssembly (WebGL 2.0)**. | ❌ Dla Linux/macOS/WASM eksporter generuje skrypty startowe, `index.html` i pliki CMake, ale wymaga zewnętrznego kompilatora (np. Emscripten), by zbudować binarkę docelową. |

---

## ✅ Lista Zadań Uzupełniających (Brakujące Funkcje 2D / 3D / Core)

### Zadania 2D:
- [x] `SpriteBatcher` z obsługą warstw (`layer` z-order) i obrotu wokół pivotu (`LabSpriteBatcher`)
- [x] Arkusze sprite'ów (`SpriteSheet2D`) i animacje poklatkowe (`AnimationPlayer2D`)
- [x] Fizyka 2D dla AABB (`Box`) i okręgów (`Circle`) oraz Raycast 2D (`LabPhysics2D`)
- [ ] **Kolizje wielokątów 2D (Convex Polygon SAT 2D)** w `LabPhysics2D`
- [ ] **Animacje szkieletowe 2D** (kości 2D, deformacja siatek 2D / import Spine lub DragonBones)
- [ ] **Komponent Parallax Scrolling 2D** (`ParallaxLayer2D` z mnożnikiem prędkości względem kamery)
- [ ] **Interaktywny Edytor Tilemap (Tile Palette & Brush Tool)** w przestrzeni roboczej `Level2D_Tilemap`
- [ ] **Emiter Cząsteczek 2D (`ParticleEmitter2D`)** zintegrowany bezpośrednio ze `SpriteBatcher`
- [ ] **Obsługa ekranów dotykowych (Touch / Multi-touch)** w `LabInput`

### Zadania 3D:
- [x] Import glTF 2.0 / GLB, FBX, STL, skinning GPU (64 kości) oraz Morph Targets + Lip-Sync (`LabSkeletal`, `LabFace`)
- [x] Cienie kierunkowe (`ShadowMap` 2048×2048 PCF), Skybox / HDRI, Mgła, HDR + Bloom + Tone Mapping (`ACES`/`Reinhard`)
- [x] Frustum Culling (`Camera::isInFrustum`)
- [ ] **Domyślny shader PBR w `LabRenderer`** (zastąpienie głównego shadera Blinn-Phong pełnym Cook-Torrance PBR z `LabGraphics`)
- [ ] **Anti-Aliasing (FXAA / TAA / MSAA)** w potoku `PostProcessPipeline`
- [ ] **Drzewa miksowania animacji (1D/2D Blend Trees)** oraz **Odwrotna Kinematyka (Two-Bone IK / Foot IK)** w `LabSkeletal`
- [ ] **Struktura przestrzenna BVH / Octree** dla szerokiej fazy kolizji 3D i raycastingu
- [ ] **System LOD (Levels of Detail)** dla automatycznego przełączania siatek 3D zależnie od dystansu
- [ ] **Occlusion Culling** (np. zapytania GPU Hardware Occlusion Queries lub Hi-Z)
- [ ] **System Terenu 3D (`TerrainMesh` / Heightmap / Voxel Terrain)** z malowaniem tekstur (splatmap)
- [ ] **Generowanie NavMesh i Pathfinding A\*** w `LabAI` (wykorzystujące istniejące znaczniki `NavMesh Obstacle Box`)
- [ ] **Asynchroniczny Streaming Zasobów** (wielowątkowe ładowanie tekstur i modeli w tle)
- [ ] **Prawdziwy backend Vulkan / DirectX 12** pod interfejsem `IRHIDevice` (`LabRHI`)

### Zadania Wspólne (Core):
- [x] Pętla gry z `deltaTime` i stałym krokiem fizyki (`onFixedUpdate` 64 Hz)
- [x] Obiektowa architektura komponentowa (`Entity` / `Component`) oraz serializacja `.labscene` / `.labpak`
- [x] Skryptowanie Lua 5.4 z kompilacją do bytecode'u `.luac` i hot-reloadingiem
- [x] Logowanie (`LabLog`), geometria debugowa i `Profiler`
- [ ] **Menedżer Scen (`SceneManager`)** z obsługą stosu scen, ładowania addytywnego i płynnych przejść
- [ ] **Centralny Cache Zasobów (`ResourceManager<T>`)** ze zliczaniem referencji (`std::shared_ptr` / smart handles) i zwalnianiem pamięci (LRU)
- [ ] **Zewnętrzny System Lokalizacji (i18n)** ładujący pliki tłumaczeń JSON/CSV dla gier (zamiast sztywnego słownika w `LabTheme.cpp`)
- [ ] **Automatyczna integracja kompilatorów cross-platform (Emscripten / Linux)** w `ExportPipeline`
