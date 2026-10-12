# Lab Engine – Techniczna Lista Naprawcza (Audyt Kodu i Architektury)

Ten plik zawiera twarde błędy techniczne, dziury pamięciowe, pozorne implementacje i dług architektoniczny wykryte bezpośrednio w kodzie silnika (poza ogólnym zarysem z `PLAN.md`).

---

## 1. Krytyczne błędy pamięci, UB i zarządzania zasobami (P0)

- [ ] **Złamanie Rule of Five w zasobach OpenGL (`include/LabRenderer.h`):**
  - Klasy `Shader` (L24–40), `Texture` (L42–59), `Mesh` (L61–89) oraz `Skybox` (L91–102) posiadają destruktory wywołujące `glDeleteProgram`, `glDeleteTextures`, `glDeleteBuffers`, `glDeleteVertexArrays` oraz `delete _shader`, ale **nie mają zablokowanego kopiowania (`= delete`) ani semantyki przenoszenia (`noexcept`)**.
  - Skopiowanie obiektu przez wartość lub realokacja w `std::vector` powoduje zwolnienie aktywnego zasobu GPU lub podwójne `delete _shader` (double-free).
- [ ] **Brak walidacji strumieni i odczyt niezainicjalizowanej pamięci w loaderach (`src/LabRenderer.cpp`):**
  - `Mesh::loadSTL` (L906–1005): odczytuje `triangleCount` i w pętli `for` woła `file.read(...)` bez sprawdzenia `if (!file)`. Dla uciętego pliku lub pliku ASCII STL funkcja czyta niezainicjalizowane śmieci ze stosu (`float n[3], v[3][3]`) i alokuje miliony wierzchołków. Ponadto zwraca surowy wskaźnik `Mesh*` (`new Mesh`).
  - `loadBMP` (L487–535) i `loadTGA` (L461–485): rzutowanie `*(int*)&header[18]` (strict aliasing UB), brak sprawdzenia przepełnienia całkowitego przy `width * height * channels`, brak sprawdzenia czy `file.read` faktycznie wczytało oczekiwaną liczbę bajtów, ręczne `new unsigned char[]` / `delete[]`.
- [ ] **Wiszące wskaźniki (Dangling Pointers) w `MapEntityHandle` i Lua (`include/LabMap.h`, `src/LabScript.cpp`):**
  - `LabMap::findEntityByName` (`include/LabMap.h:197-207`) zwraca strukturę `MapEntityHandle` z surowymi wskaźnikami `Vec3* position`, `Vec3* rotation`, `Vec3* color` wskazującymi bezpośrednio do wnętrza wektorów `std::vector<MapBrush>`, `std::vector<MapProp>` itd.
  - Jakikolwiek `push_back` na tych wektorach podczas działania skryptu unieważnia wskaźniki (Use-After-Free).
- [ ] **Niepełne czyszczenie i wiszące wskaźniki w `Renderer::shutdown()` (`src/LabRenderer.cpp:1177-1188`):**
  - Po `delete _defaultShader`, `delete _uiShader`, `delete _cubeMesh` wskaźniki nie są zerowane (`nullptr` ustawiany jest tylko dla `_shadowDepthShader`), a `_quadMesh` nie jest zwalniany w ogóle.

---

## 2. Schizofrenia architektoniczna i wyciek logiki gry do silnika (P0 / P1)

- [ ] **Trzy równoległe, niekompatybilne systemy sceny i encji:**
  1. Płaskie wektory struktur w `LabMap` (`brushes`, `props`, `doors`, `sprites`, `cameras`, `lights` w `include/LabMap.h:209-231`), gdzie `MapProp` (L38–83) to „God Struct” z 25 polami (`hasAudio`, `hasPhysics`, `hasCloth`, `hasWaterVolume`, `hasForceField`, `hasVehicle`, `hasDecal`, `hasPostProcess`).
  2. `LabScene` (`include/LabScene.h:33`), która **dziedziczy publicznie po `LabMap`** (`class LabScene : public LabMap`) i dokłada obok starych wektorów nowy wektor `std::vector<std::shared_ptr<Entity>> entities`.
  3. Osobny graf sceny `SceneNode` (`include/LabSceneNode.h:11-56`) z hierarchią rodzic-dziecko i prefabami, całkowicie odcięty od `Entity` i `Transform`.
  4. `ScriptEngine` (`src/LabScript.cpp:57-85, 179-300`) operuje wyłącznie na starym `LabMap*`, ignorując `Entity` oraz `SceneNode`.
- [ ] **Wyciek logiki gry *Frozen-Life* do rdzenia silnika (`LabRender`, `LabPhysics`, `LabWorld`):**
  - `Texture::Texture` (`src/LabRenderer.cpp:606-630`) ma zahardkodowane nazwy broni i obiektów z gry (`"m4a4"`, `"sg553"`, `"minigun"`, `"plasma"`, `"railgun"`, `"rpg"`, `"pistol"`, `"shotgun"`, `"crate"`, `"barrel"`).
  - `PostProcessPipeline` (`src/LabPostProcess.cpp:132-150`) ma na stałe zaszyty w głównym shaderze silnika efekt zamarzania wizjera (`uFrostIntensity`, `fbmFrost`).
  - `LabScene` (`include/LabScene.h:140-144`) zawiera metody `weaponIdToPrefab` / `prefabToWeaponId` mapujące konkretne bronie z FPS-a.
  - `RigidBody` i `PhysicsWorld` (`include/LabPhysics.h:15-60`, `src/LabPhysics.cpp:192-289`) zawierają logikę `PropType::Crate`, `PropType::ExplosiveBarrel`, `health`, `maxHealth`, `isHeadshot`, a solver fizyczny bezpośrednio wywołuje `AudioEngine::playSound3D(SoundID::CrateBreak, ...)` oraz `Renderer::drawCube(...)` (przez co `LabPhysics.dll` musi linkować `LabRender` i `LabAudio`).
- [ ] **Globalny, niereentrantny stan (`static`):**
  - `Renderer` (`include/LabRenderer.h:104-209`) składa się wyłącznie z pól i metod `static` – brak możliwości niezależnego renderowania wielu widoków bez ręcznego nadpisywania stanu globalnego.
  - `ScriptEngine` (`src/LabScript.cpp:27-34`) przechowuje stan w zmiennych statycznych pliku (`s_activeMap`, `s_hudTexts`, callbacki) zamiast w polach instancji.

---

## 3. Pozorne implementacje („Atrapy”) i martwy kod (P1)

- [ ] **Martwy kod PBR i mgły na CPU w `LabGraphics` (`include/LabGraphics.h:78-154`, `src/LabGraphics.cpp:249-374`):**
  - Pełna implementacja `PBRMaterial` i `PBRShading` (Cook-Torrance GGX BRDF) liczona na CPU dla pojedynczego wektora, podczas gdy silnik zakazuje PBR (`AGENTS.md`), a właściwy renderer używa wyłącznie Blinn-Phonga w GLSL.
  - Klasa `AtmosphericFog` liczy mgłę na CPU, dublując obliczenia z fragment shadera w `LabRenderer.cpp`.
- [ ] **Atrapa RHI (`src/LabRHI.cpp:57-61`):**
  - `createRHIDevice(GraphicsBackend backend)` ignoruje argument (`(void)backend;`) i zawsze zwraca `MockDevice`, który alokuje `std::vector<uint8_t>` w RAM-ie. Żaden kod renderera nie korzysta z RHI.
- [ ] **Niepełne OpenGL 4.5+ DSA (`src/LabRenderer.cpp`, `src/LabGraphics.cpp`, `src/LabPostProcess.cpp`):**
  - Tylko `Skybox` (`src/LabRenderer.cpp:848-854`) używa funkcji DSA (`glCreateVertexArrays`, `glNamedBufferStorage`). Klasy `Mesh`, `InstancedMesh`, `Texture`, `HDRISkybox`, `BakedLightmap` i `PostProcessPipeline` używają starego modelu bind-to-edit (`glGen*` + `glBind*` + `glBufferData`/`glTexImage2D`).
- [ ] **Uproszczona fizyka 3D udająca OBB (`src/LabPhysics.cpp:343-460`):**
  - `resolveBodyVsBodyCollision` (L430–460) ignoruje orientację (`Quat`) i wymiary prostopadłościanu – bierze `ra = std::max({x, y, z})` i zderza pudełka jak sfery w pętli $O(N^2)$, bez impulsu obrotowego.
  - `resolveGroundCollision` (L343–386) ma na sztywno zaszytą podłogę na wysokości `c.y < 0.0f`.
- [ ] **No-op w Color Gradingu (`src/LabPostProcess.cpp:169-170`):**
  - Linia `mapped = mix(vec3(luma), mapped, 1.0);` nie robi nic (współczynnik mieszania jest zahardkodowany na `1.0`).

---

## 4. Wąskie gardła wydajnościowe i konfiguracja budowania (P1 / P2)

- [ ] **Brak cache'owania lokalizacji uniformów + alokacje `std::string` co draw call (`src/LabRenderer.cpp:356-380, 1054-1097`):**
  - Każde `setMat4` / `setVec3` / `setFloat` odpytuje sterownik przez `glGetUniformLocation`.
  - `applyLightingAndShadowUniforms` jest wołane przy **każdym pojedynczym `drawCube` / `drawMesh`** i w pętli po 16 światłach punktowych za każdym razem skleja na stercie `std::string prefix = "uPointLights[" + std::to_string(i) + "].";`.
- [ ] **Bruteforce 37 ścieżek przy szukaniu zasobów (`src/LabRenderer.cpp:401-459`):**
  - Funkcja `resolveAssetPath` w `LabRenderer.cpp` omija `LabVFS` i sprawdza w pętli 37 zgadywanych ścieżek względnych (`../../assets/models/textures/`, `build/Release/` itd.).
- [ ] **Liniowe `dynamic_cast` w `Entity::GetComponent<T>()` (`include/LabEntity.h:46-57`):**
  - Każde pobranie komponentu (w tym szukanie kamery w `LabScene::GetMainCamera`) wykonuje liniowy przegląd wektora i `dynamic_cast`.
- [ ] **Konfiguracja `CMakeLists.txt` (`CMakeLists.txt:9-21`):**
  - Globalne `CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON` eksportuje wszystkie symbole wewnętrzne z 11 bibliotek `.dll` zamiast polegać na jawnych makrach API.
  - Flaga `WARNING_AS_ERROR_FLAG` (`/WX` / `-Werror`) jest nałożona wyłącznie na jeden plik (`src/LabSpriteBatcher.cpp`), a nie na targety silnika.

---

## 5. Bezpieczeństwo i walidacja danych wejściowych (Security Audit)

- [x] **[HIGH] Path Traversal w `VFS::resolve` (`src/LabVFS.cpp:19-42`):**
  - `VFS::resolve` skleja `it->second / relative` bez kanonizacji (`std::filesystem::weakly_canonical`) i bez sprawdzenia, czy wynikowa ścieżka nadal znajduje się wewnątrz zamontowanego katalogu. Ścieżka typu `assets://../../../../Windows/win.ini` pozwala wyjść poza katalog projektu przy ładowaniu map/skryptów. *(Naprawione: normalizacja leksykalna i odrzucanie `..` oraz ścieżek głównych + test regresji w `Test 58`)*.
- [x] **[HIGH] Brak sandboxa Lua przy automatycznym uruchamianiu skryptów z map i katalogu `plugins/` (`src/LabScript.cpp:62-85`, `src/LabPackage.cpp:9-20`):**
  - `ScriptEngine::setActiveMap` automatycznie wykonuje dowolny skrypt przypisany do obiektów mapy (`b.script`, `p.script`), a `PackageManager` automatycznie uruchamia każdy plik `.lua` znaleziony w `plugins/`.
  - Maszyna wirtualna Lua otwiera wszystkie biblioteki standardowe (`luaL_openlibs`, w tym `os.execute`, `os.remove`, `io.open`), co oznacza, że wczytanie cudzej mapy `.labmap` może wykonać dowolne polecenie systemowe (Arbitrary Code Execution). Należy wyciąć niebezpieczne funkcje z biblioteki `os`/`io`/`package` dla skryptów map. *(Naprawione: wycięcie `os.execute`/`remove`/`rename`/`exit`/`setenv`, `io`, `dofile`, `loadfile`, `package.loadlib` + walidacja ścieżek skryptów mapy + test regresji w `Test 58`)*.
- [x] **[MEDIUM] Brak walidacji w `NATDiscovery::openUPnPPort` (`src/LabNetwork.cpp:802-888`):**
  - Parametr `description` jest wklejany bezpośrednio do ciała XML (`<NewPortMappingDescription>`) bez escapowania znaków `<`, `>`, `&`.
  - Odpowiedź UDP `M-SEARCH` (`LOCATION:`) z sieci lokalnej nie jest weryfikowana pod kątem tego, czy pochodzi z prywatnej podsieci bramy domyślnej przed wysłaniem żądania HTTP GET/POST. *(Naprawione: escapowanie XML `escapeXml` oraz weryfikacja prywatnej podsieci IPv4 `isPrivateIPv4`)*.
- [ ] **[MEDIUM] Dodatkowe wycieki logiki *Frozen-Life* w `ScriptEngine` (`src/LabScript.cpp:1150-1305`):**
  - Klasa `ScriptEngine` w `LabScript.dll` ma na stałe wbudowane pola `_weaponDefs`, metodę `getWeaponNameById` z tablicą `{"Pipe", "Pistol", "Shotgun", "M4A4-S", "SG553", "Minigun", "Plasma Gun", "Railgun", "RPG"}`, obsługę kart dostępu (`_missionCardCount`) oraz pancerza (`CalculateDamage`).
