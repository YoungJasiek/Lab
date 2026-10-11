# Check: `raw_new_delete`

- Przeskanowane pliki własne silnika (`include/`, `src/`): **143**
- Wszystkie dopasowania `grep -rn "\b(new|delete)\b"` w plikach silnika: **76**
  - Aktywne wystąpienia surowego `new` / `delete` w kodzie (backlog Fazy 3.3): **21**
  - Nowe / nieoczekiwane wystąpienia poza backlogiem Fazy 3.3: **0**
  - Wykluczone wystąpienia w plikach silnika (z wyjaśnieniem poniżej): **55**
- Dopasowania `grep -rn` w osadzonych bibliotekach zewnętrznych (`stb_truetype.h`, `stb_image.h`, `miniaudio.h`, `glad`): **97** trafień w **95** liniach (pomijane jako kod third-party C)

## Podsumowanie wykluczonych wystąpień w kodzie silnika według powodu

| Powód wykluczenia (rozbieżność względem surowego `grep -rn`) | Liczba wystąpień |
|--------------------------------------------------------------|-----------------:|
| komentarz C/C++ (angielskie słowo `new` lub `delete` w opisie) | 11 |
| literał napisowy (tekst UI / logu z angielskim słowem `new` lub `delete`) | 14 |
| usunięta funkcja składowa (`= delete`) | 28 |
| wzorzec RAII `unique_ptr<T> x(new ...)` / `std::unique_ptr<T> x(new ...)` (brak surowego posiadania) | 2 |
| **Razem wykluczonych w plikach silnika** | **55** |

## Wykluczone pliki bibliotek zewnętrznych (third-party w `include/` i `src/`)

| Plik zewnętrzny | Linie dopasowane przez `grep -rn` | Liczba trafień słów `new`/`delete` | Powód pominięcia |
|-----------------|----------------------------------:|-----------------------------------:|------------------|
| `include/miniaudio.h` | 77 | 79 | Zewnętrzna biblioteka single-header / wygenerowany loader C (komentarze i identyfikatory C) |
| `include/stb_image.h` | 6 | 6 | Zewnętrzna biblioteka single-header / wygenerowany loader C (komentarze i identyfikatory C) |
| `include/stb_truetype.h` | 12 | 12 | Zewnętrzna biblioteka single-header / wygenerowany loader C (komentarze i identyfikatory C) |

## 1. Aktywne wystąpienia surowego `new` / `delete` (do refaktoru w Fazie 3.3)

| Plik | Linia | Słowo kluczowe | Fragment kodu |
|------|------:|----------------|---------------|
| `src/LabPlugin.cpp` | 109 | `delete` | `delete p;` |
| `src/LabRenderer.cpp` | 476 | `new` | `unsigned char* data = new unsigned char[size];` |
| `src/LabRenderer.cpp` | 513 | `new` | `unsigned char* data = new unsigned char[size];` |
| `src/LabRenderer.cpp` | 519 | `new` | `unsigned char* rgbData = new unsigned char[(*width) * std::abs(*height) * channels];` |
| `src/LabRenderer.cpp` | 533 | `delete` | `delete[] data;` |
| `src/LabRenderer.cpp` | 596 | `new` | `unsigned char* procData = new unsigned char[_width * _height * 3];` |
| `src/LabRenderer.cpp` | 705 | `delete` | `delete[] data;` |
| `src/LabRenderer.cpp` | 824 | `delete` | `delete[] data;` |
| `src/LabRenderer.cpp` | 877 | `new` | `_shader = new Shader(skyboxVertSrc, skyboxFragSrc);` |
| `src/LabRenderer.cpp` | 881 | `delete` | `if (_shader) delete _shader;` |
| `src/LabRenderer.cpp` | 1004 | `new` | `return new Mesh(vertices, indices);` |
| `src/LabRenderer.cpp` | 1106 | `new` | `_defaultShader = new Shader(defaultVertexShaderSrc, defaultFragmentShaderSrc);` |
| `src/LabRenderer.cpp` | 1107 | `new` | `_shadowDepthShader = new Shader(shadowDepthVertexShaderSrc, shadowDepthFragmentShaderSrc);` |
| `src/LabRenderer.cpp` | 1108 | `new` | `_uiShader = new Shader(uiVertexShaderSrc, uiFragmentShaderSrc);` |
| `src/LabRenderer.cpp` | 1131 | `new` | `_cubeMesh = new Mesh(cubeVerts, cubeInds);` |
| `src/LabRenderer.cpp` | 1178 | `delete` | `delete _defaultShader;` |
| `src/LabRenderer.cpp` | 1179 | `delete` | `delete _shadowDepthShader;` |
| `src/LabRenderer.cpp` | 1181 | `delete` | `delete _uiShader;` |
| `src/LabRenderer.cpp` | 1182 | `delete` | `delete _cubeMesh;` |
| `src/LabSpriteBatcher.cpp` | 103 | `new` | `_shader = new Shader(spriteVertexShaderSrc, spriteFragmentShaderSrc);` |
| `src/LabSpriteBatcher.cpp` | 120 | `delete` | `delete _shader;` |

## 2. Pełne wyjaśnienie wszystkich 55 rozbieżności względem `grep -rn` w plikach silnika

| Plik | Linia | Słowo | Wyjaśnienie rozbieżności (powód wykluczenia) | Fragment linii |
|------|------:|-------|----------------------------------------------|----------------|
| `include/LabEvent.h` | 73 | `delete` | usunięta funkcja składowa (`= delete`) | `EventBus(const EventBus&) = delete;` |
| `include/LabEvent.h` | 74 | `delete` | usunięta funkcja składowa (`= delete`) | `EventBus& operator=(const EventBus&) = delete;` |
| `include/LabFace.h` | 30 | `delete` | usunięta funkcja składowa (`= delete`) | `FacialMesh(const FacialMesh&) = delete;` |
| `include/LabFace.h` | 31 | `delete` | usunięta funkcja składowa (`= delete`) | `FacialMesh& operator=(const FacialMesh&) = delete;` |
| `include/LabGraphics.h` | 24 | `delete` | usunięta funkcja składowa (`= delete`) | `HDRISkybox(const HDRISkybox&) = delete;` |
| `include/LabGraphics.h` | 25 | `delete` | usunięta funkcja składowa (`= delete`) | `HDRISkybox& operator=(const HDRISkybox&) = delete;` |
| `include/LabGraphics.h` | 182 | `delete` | usunięta funkcja składowa (`= delete`) | `InstancedMesh(const InstancedMesh&) = delete;` |
| `include/LabGraphics.h` | 183 | `delete` | usunięta funkcja składowa (`= delete`) | `InstancedMesh& operator=(const InstancedMesh&) = delete;` |
| `include/LabGraphics.h` | 212 | `delete` | usunięta funkcja składowa (`= delete`) | `BakedLightmap(const BakedLightmap&) = delete;` |
| `include/LabGraphics.h` | 213 | `delete` | usunięta funkcja składowa (`= delete`) | `BakedLightmap& operator=(const BakedLightmap&) = delete;` |
| `include/LabGraphics.h` | 257 | `delete` | usunięta funkcja składowa (`= delete`) | `ShadowAtlas(const ShadowAtlas&) = delete;` |
| `include/LabGraphics.h` | 258 | `delete` | usunięta funkcja składowa (`= delete`) | `ShadowAtlas& operator=(const ShadowAtlas&) = delete;` |
| `include/LabGraphics.h` | 300 | `delete` | usunięta funkcja składowa (`= delete`) | `CubemapProbe(const CubemapProbe&) = delete;` |
| `include/LabGraphics.h` | 301 | `delete` | usunięta funkcja składowa (`= delete`) | `CubemapProbe& operator=(const CubemapProbe&) = delete;` |
| `include/LabLight.h` | 47 | `delete` | usunięta funkcja składowa (`= delete`) | `ShadowMap(const ShadowMap&) = delete;` |
| `include/LabLight.h` | 48 | `delete` | usunięta funkcja składowa (`= delete`) | `ShadowMap& operator=(const ShadowMap&) = delete;` |
| `include/LabNetwork.h` | 195 | `delete` | usunięta funkcja składowa (`= delete`) | `UDPSocket(const UDPSocket&) = delete;` |
| `include/LabNetwork.h` | 196 | `delete` | usunięta funkcja składowa (`= delete`) | `UDPSocket& operator=(const UDPSocket&) = delete;` |
| `include/LabPackage.h` | 127 | `delete` | komentarz C/C++ (angielskie słowo `new` lub `delete` w opisie) | `/// @brief Full removal: disable, unregister, delete file from disk.` |
| `include/LabPlugin.h` | 55 | `new` | komentarz C/C++ (angielskie słowo `new` lub `delete` w opisie) | `/// @brief Registers and loads a new plugin instance.` |
| `include/LabPlugin.h` | 85 | `delete` | usunięta funkcja składowa (`= delete`) | `PluginManager(const PluginManager&) = delete;` |
| `include/LabPlugin.h` | 86 | `delete` | usunięta funkcja składowa (`= delete`) | `PluginManager& operator=(const PluginManager&) = delete;` |
| `include/LabPostProcess.h` | 21 | `delete` | usunięta funkcja składowa (`= delete`) | `PostProcessPipeline(const PostProcessPipeline&) = delete;` |
| `include/LabPostProcess.h` | 22 | `delete` | usunięta funkcja składowa (`= delete`) | `PostProcessPipeline& operator=(const PostProcessPipeline&) = delete;` |
| `include/LabProject.h` | 69 | `new` | komentarz C/C++ (angielskie słowo `new` lub `delete` w opisie) | `/// @brief Create a brand new decoupled game project with 2D/3D dimension, display config, and clean starter scene` |
| `include/LabSceneNode.h` | 64 | `new` | komentarz C/C++ (angielskie słowo `new` lub `delete` w opisie) | `/// @brief Instantiates a brand new hierarchy clone from this prefab template.` |
| `include/LabScript.h` | 45 | `delete` | usunięta funkcja składowa (`= delete`) | `ScriptEngine(const ScriptEngine&) = delete;` |
| `include/LabScript.h` | 46 | `delete` | usunięta funkcja składowa (`= delete`) | `ScriptEngine& operator=(const ScriptEngine&) = delete;` |
| `include/LabSkeletal.h` | 42 | `delete` | usunięta funkcja składowa (`= delete`) | `SkinnedMesh(const SkinnedMesh&) = delete;` |
| `include/LabSkeletal.h` | 43 | `delete` | usunięta funkcja składowa (`= delete`) | `SkinnedMesh& operator=(const SkinnedMesh&) = delete;` |
| `include/plugins/LuaDropinPlugin.h` | 19 | `delete` | usunięta funkcja składowa (`= delete`) | `LuaDropinPlugin(const LuaDropinPlugin&) = delete;` |
| `include/plugins/LuaDropinPlugin.h` | 20 | `delete` | usunięta funkcja składowa (`= delete`) | `LuaDropinPlugin& operator=(const LuaDropinPlugin&) = delete;` |
| `src/editor_main.cpp` | 792 | `delete` | komentarz C/C++ (angielskie słowo `new` lub `delete` w opisie) | `// Backspace: delete char or merge line` |
| `src/editor_main.cpp` | 810 | `delete` | komentarz C/C++ (angielskie słowo `new` lub `delete` w opisie) | `// Delete: delete char or pull up next line` |
| `src/editor_main.cpp` | 2534 | `delete` | literał napisowy (tekst UI / logu z angielskim słowem `new` lub `delete`) | `logMessage("Cannot delete the last remaining spawn point!");` |
| `src/LabCommand.cpp` | 12 | `new` | komentarz C/C++ (angielskie słowo `new` lub `delete` w opisie) | `// Executing new command truncates redo stack` |
| `src/LabHUDEditor.cpp` | 894 | `new` | literał napisowy (tekst UI / logu z angielskim słowem `new` lub `delete`) | `openModal(ModalType::EditText, "Edit Text", "Enter new text:", e->text, [this, targetRef](const std::string& v){` |
| `src/LabHUDEditor.cpp` | 1633 | `new` | literał napisowy (tekst UI / logu z angielskim słowem `new` lub `delete`) | `LabFont::drawText(bx + 40.0f, by + 75.0f, "Choose an existing .labhud interface project or author a new layout:", 1.5f, Vec3(0.7f, 0.7f, 0.7f), LabFontType::System);` |
| `src/LabPackage.cpp` | 166 | `delete` | literał napisowy (tekst UI / logu z angielskim słowem `new` lub `delete`) | `LabLog::error("deleteDropinPlugin: cannot delete " + path + ": " + ec.message());` |
| `src/main.cpp` | 83 | `new` | komentarz C/C++ (angielskie słowo `new` lub `delete` w opisie) | `// Preload Core Textures (Classic engine materials + 25 new high-definition environmental textures)` |
| `src/main.cpp` | 1902 | `new` | literał napisowy (tekst UI / logu z angielskim słowem `new` lub `delete`) | `_punchStatusMsg = "Generated new Room Code: [" + _pnpRoomCode + "]!";` |
| `src/test_verify.cpp` | 3727 | `new` | wzorzec RAII `unique_ptr<T> x(new ...)` / `std::unique_ptr<T> x(new ...)` (brak surowego posiadania) | `std::unique_ptr<Lab::Texture> pipePng(new Lab::Texture("assets/models/textures/PipeWrenchTool_baseColor.png"));` |
| `src/test_verify.cpp` | 3732 | `new` | wzorzec RAII `unique_ptr<T> x(new ...)` / `std::unique_ptr<T> x(new ...)` (brak surowego posiadania) | `std::unique_ptr<Lab::Texture> pipeBmpPng(new Lab::Texture("assets/textures/weapon_pipe.bmp"));` |
| `src/test_verify.cpp` | 5078 | `new` | literał napisowy (tekst UI / logu z angielskim słowem `new` lub `delete`) | `std::cerr << "Assertion failed: Failed to load new texture: " << texName << "\n";` |
| `src/test_verify.cpp` | 5093 | `new` | literał napisowy (tekst UI / logu z angielskim słowem `new` lub `delete`) | `std::cout << "  [PASS] All 25 new PNG textures (128x128 RGB) ingested and uploaded to GPU successfully!\n";` |
| `src/test_verify.cpp` | 5130 | `new` | komentarz C/C++ (angielskie słowo `new` lub `delete` w opisie) | `// 3. Load darkness.labmap and verify it contains brushes using the new textures` |
| `src/test_verify.cpp` | 5149 | `new` | literał napisowy (tekst UI / logu z angielskim słowem `new` lub `delete`) | `std::cerr << "Assertion failed: Expected at least 15 brushes referencing new textures, got "` |
| `src/test_verify.cpp` | 5154 | `new` | literał napisowy (tekst UI / logu z angielskim słowem `new` lub `delete`) | `<< " brushes (" << newTextureBrushCount << " textured with new PNG materials)!\n";` |
| `src/test_verify.cpp` | 5194 | `new` | literał napisowy (tekst UI / logu z angielskim słowem `new` lub `delete`) | `std::cout << "  [PASS] Rendered Darkness Showcase arena with 25 new textures to 'test_new_textures_game.bmp'!\n";` |
| `src/test_verify.cpp` | 6398 | `new` | literał napisowy (tekst UI / logu z angielskim słowem `new` lub `delete`) | `std::cerr << "Assertion failed: Redo branch was not truncated upon new command execution!\n";` |
| `src/test_verify.cpp` | 8224 | `delete` | komentarz C/C++ (angielskie słowo `new` lub `delete` w opisie) | `// Sub-test B: Script Editor Buffer manipulation (insert, split, backspace, delete, indent)` |
| `src/test_verify.cpp` | 8689 | `new` | literał napisowy (tekst UI / logu z angielskim słowem `new` lub `delete`) | `std::cerr << "Assertion failed: Failed to create new project from Project Hub!\n";` |
| `src/test_verify.cpp` | 8705 | `new` | literał napisowy (tekst UI / logu z angielskim słowem `new` lub `delete`) | `std::cout << "  [PASS] Sub-test D: Created new user game via Hub and confirmed discovery in project list.\n";` |
| `src/test_verify.cpp` | 9093 | `new` | komentarz C/C++ (angielskie słowo `new` lub `delete` w opisie) | `// Sub-test A: Create new project and verify assets/scripts is 100% clean (zero engine scripts copied)` |
| `src/test_verify.cpp` | 9097 | `new` | literał napisowy (tekst UI / logu z angielskim słowem `new` lub `delete`) | `std::cerr << "Assertion failed: Failed to create new clean project!\n";` |
