# Lab Engine: plan naprawy i rozwoju

Ten dokument jest źródłem prawdy dla pracy nad silnikiem. Zasady dla narzędzi AI są w `AGENTS.md`. Stan opisany poniżej pochodzi z przeglądu kodu (commit `6842c94`), nie z deklaracji w README.

## 0. Cel i zakres

**Cel:** stabilny silnik FPS w stylu Source Engine (Half-Life 2): Blinn-Phong, ostre cienie, lightmapy, cubemapy, subtelny bloom i HDR, korekcja kolorów i winieta, FXAA lub MSAA. **Bez PBR i bez TAA.** Z modułem 2D na poziomie „działa i jest przetestowany”.

**Poza zakresem na teraz:** Vulkan, DirectX 12, Metal, nowe systemy (LOD, NavMesh, Blend Trees, IK, streaming), nowe funkcje edytora. Wracają po fazach 1–3.

**Kryterium sukcesu:** każdy moduł ma jedno z trzech oznaczeń w README: *stabilny* (testy, CI, dokumentacja), *eksperymentalny* (działa, brak testów) albo *atrapa* (nie działa, zapisane jako ograniczenie).

## 1. Stan wyjściowy (fakty z kodu)

| Obszar | Stan | Dowód |
|---|---|---|
| Rozmiar | ~61 tys. linii C++ | suma `src/` + `include/` bez bibliotek zewnętrznych |
| Monolit testów | `src/test_verify.cpp`, 9614 linii | jeden plik |
| Monolit edytora | `src/editor_main.cpp`, 9346 linii | jeden plik |
| Duże pliki | `main.cpp` 3255, `LabStudio.cpp` 2721, `LabNetwork.cpp` 1916 | |
| Backendy RHI | tylko OpenGL, Vulkan/DX12/Metal to `MockDevice` | `src/LabRHI.cpp:44` |
| Shader sceny | Blinn-Phong, zgodne z celem HL2 | `src/LabRenderer.cpp:184` |
| Cubemapy | skybox przez `samplerCube` | `src/LabRenderer.cpp:872` |
| Lightmapy | klasa `BakedLightmap` istnieje, podpięcie do shadera sceny do sprawdzenia | `src/LabGraphics.cpp:506` |
| Normal mapping | pole `normalTex` w materiale, próbkowanie w shaderze do sprawdzenia | `include/LabGraphics.h:121` |
| Post-processing | bloom, ACES, winieta, HDR (RGBA16F) | `src/LabPostProcess.cpp` |
| Color grading | komentarz „Filmic Color Grading”, implementacja do sprawdzenia | `src/LabPostProcess.cpp:76` |
| Instancing | `glDrawElementsInstanced` | `src/LabGraphics.cpp:499` |
| Anti-aliasing | brak FXAA i MSAA | |
| Debug OpenGL | brak `glDebugMessageCallback` | |
| CI | tylko `windows-latest`, MSVC, Release | `.github/workflows/c-cpp.yml` |
| Linux / WASM | deklarowane w README, bez testów i bez CI | |
| Surowe `new` / `delete` | 32 `new` i 11 `delete` przy 151 użyciach `unique_ptr`/`shared_ptr` | `grep` po `src` i `include` |
| Największe skupisko surowych wskaźników | `src/LabRenderer.cpp`, 15 wystąpień | |
| Dług w kodzie | 49 znaczników TODO/FIXME/HACK | `grep` po `src` i `include` |
| Dług w planie | 54 otwarte pozycje w TODO.md | |
| Wątki | brak `std::thread` / `std::async` | ładowanie zasobów jest synchroniczne |
| Nawigacja | brak NavMesh i A\*, jest tylko plugin i znacznik „NavMesh Obstacle Box” | `src/editor_main.cpp:2962` |

## 2. Zasady pracy (dla każdego narzędzia i osoby)

1. **Nigdy nie pracuj bezpośrednio na `main`.** Jedno zadanie = jedna gałąź `ai/<nazwa-zadania>` lub `feat/<nazwa>`.
2. **Najpierw test, potem zmiana.** Dla refaktoru: test na obecne zachowanie musi przejść przed zmianą i po niej.
3. **Małe PR-y.** Maksymalnie ~400 zmienionych linii, poza mechanicznymi przeniesieniami (wtedy osobny PR, który nic nie zmienia logicznie).
4. **Każdy PR zawiera:** opis zmiany, listę zmienionych plików, wynik kompilacji, wynik testów, wynik sanitizerów, informację, czy zachowanie się zmieniło (tak/nie).
5. **Nie dodawaj nowych funkcji** dopóki fazy 1–3 nie są zamknięte.
6. **Nie dokumentuj tego, czego nie ma.** Każda obietnica w README musi mieć odpowiednik w kodzie albo oznaczenie „atrapa”.
7. **Kod GPU i fizykę przegląda człowiek**, niezależnie od tego, kto je napisał.
8. **Zasada własności pamięci:** żadnych surowych wskaźników jako właścicieli w polach klas ani w zwykłym kodzie. Właścicielem jest `std::unique_ptr` (lub kontener). Surowe `new` dopuszczamy tylko wewnątrz opakowań RAII, wrapperów C-API (OpenGL) albo alokatorów.
9. **Styl wizualny jest stały:** Blinn-Phong, bez PBR, bez TAA. Zmiana tego wymaga decyzji właściciela, nie PR-a.

## 3. Fazy

### Faza 1: higiena i bezpieczeństwo (1–2 tygodnie)

Cel: wyłapać błędy zanim zaczniemy cokolwiek przepisywać.

**1.1 Ostrzeżenia kompilatora**
- Włącz `-Wall -Wextra -Wpedantic -Werror` (GCC/Clang) i `/W4 /WX` (MSVC) w `CMakeLists.txt`.
- Napraw ostrzeżenia moduł po module. Każdy moduł to osobny PR.
- *Akceptacja:* build bez ostrzeżeń na obu kompilatorach dla wszystkich targetów.

**1.2 CI na Linuxie**
- Dodaj job `ubuntu-latest` z GCC lub Clang, Release i Debug.
- Testy wymagające OpenGL uruchamiaj przez Mesa + `xvfb-run` albo oznacz jako pominięte z powodu braku GPU, z jawnym komunikatem.
- *Akceptacja:* job przechodzi na czysto, a pominięte testy są wypisane w logu.

**1.3 Sanitizery**
- Dodaj job z `-fsanitize=address,undefined` (Debug).
- Napraw błędy, które wyjdą. Każdy błąd dostaje test regresji.
- *Akceptacja:* sanitizery czyste na testach.

**1.4 Kopiowalność klas z surowymi wskaźnikami**
- Dla każdej klasy z polem surowego wskaźnika zwalnianego w destruktorze sprawdź, czy jest kopiowalna. Jeśli tak, kopia zwolni ten sam zasób dwa razy.
- Dla każdej takiej klasy: albo `= delete` na kopiowaniu, albo `unique_ptr`.
- *Akceptacja:* lista klas z wynikiem sprawdzenia w PR, zero kopiowalnych klas z ręcznym zwalnianiem.

**1.5 Debug OpenGL** (nowe)
- Włącz `GL_DEBUG_OUTPUT` i `glDebugMessageCallback` w trybie Debug. Błędy DSA i stanu będą widoczne od razu.
- *Akceptacja:* callback działa, testy wypisują ostrzeżenia GL.

**1.6 Dokumentacja stanu**
- W README dodaj sekcję „Status modułów” z tabelą: moduł, stan (stabilny / eksperymentalny / atrapa), testy (tak/nie), platformy.
- Usuń badge „Linux | WASM” i „Universal 2D & 3D”, dopóki nie są prawdziwe.
- Dodaj sekcję „Znane ograniczenia”: atrapy RHI, brak NavMesh, brak streamingu, Linux/WASM bez testów.
- *Akceptacja:* każda obietnica w README ma odpowiednik w tabeli statusu.

### Faza 2: testy (2–4 tygodnie)

Cel: zabezpieczyć zachowanie przed refaktorem.

**2.1 Podział `test_verify.cpp`**
- Stwórz `tests/` z małym harnessem: rejestracja testu, nazwa, wynik, czas.
- Przenieś testy do plików per moduł: `tests/physics2d_test.cpp`, `tests/serialization_test.cpp`, `tests/gltf_test.cpp` itd.
- Zachowaj stary `TestVerify.exe` jako zbiorczy runner.
- *Akceptacja:* ta sama liczba testów przechodzi przed i po podziale. Liczba jest zapisana w README, nie w badge.

**2.2 Testy serializacji (priorytet najwyższy)**
- Dla każdego formatu: `.labscene`, `.labmap`, `.labpak`, `.labproject`, `.anim`, JSON 2D. Schemat: zapisz → wczytaj → porównaj → zapisz ponownie → porównaj bajty.
- Testy na pliki uszkodzone: obcięte, brakujące pola, złe typy. Oczekiwany wynik: kontrolowany błąd, nie crash.
- *Akceptacja:* każdy format ma test round-trip i test uszkodzonego pliku.

**2.3 Testy fizyki**
- 2D: AABB vs AABB, Circle vs AABB, styczność (dokładnie na krawędzi), raycast 2D na krawędzi.
- 3D: `moveAndSlide` na ścianie i na krawędzi, sweep sfery vs AABB (`CCDPhysics::sweepSphereVsAABB`), tunelowanie przy dużej prędkości.
- Testy właściwości: brak przenikania, energia nie rośnie bez powodu.
- *Akceptacja:* testy przechodzą z sanitizerami. Przypadki brzegowe udokumentowane w nazwie testu.

**2.4 Testy loaderów**
- glTF/GLB: poprawny plik, brakujące bufory, zły nagłówek, plik obcięty.
- Obrazy: wczytywanie tekstur, także przy błędnym pliku.
- *Akceptacja:* każdy przypadek daje albo poprawny wynik, albo komunikat błędu z nazwą pliku.

### Faza 3: struktura i pamięć (3–6 tygodni)

Cel: zmniejszyć koszt zmian bez zmiany zachowania.

**3.1 Rozbicie `editor_main.cpp`**
- Podziel na pliki per workspace (5 kategorii z README). Plik wejściowy zostaje tylko z pętlą i routingiem.
- Każdy workspace to osobny PR.
- *Akceptacja:* testy przed i po przechodzą. Żaden plik w `editor/` nie przekracza 1500 linii.

**3.2 Rozbicie `main.cpp` i `LabStudio.cpp`**
- Podział według odpowiedzialności, nie według liczby linii.
- *Akceptacja:* jak wyżej.

**3.3 Pamięć: usunięcie surowych właścicieli**
Konkretna lista, w kolejności od najprostszej:
1. `src/LabSpriteBatcher.cpp:103,120`: `_shader = new Shader(...)` i `delete _shader` → `std::unique_ptr<Shader>`.
2. `src/LabRenderer.cpp:877,881`: `_shader` skybox → `unique_ptr`.
3. `src/LabRenderer.cpp:1106–1108,1131`: `_defaultShader`, `_shadowDepthShader`, `_uiShader`, `_cubeMesh` → `unique_ptr`. Zwolnienia w destruktorze (linie 1178–1182) znikają.
4. `src/LabRenderer.cpp:1004`: `return new Mesh(...)` → zwracać `std::unique_ptr<Mesh>`. Zmienić wywołujących.
5. `src/LabRenderer.cpp:476,513,519,596`: bufory obrazów `new unsigned char[]` → `std::vector<unsigned char>`. Dane ze `stb_image` zwalniać przez `stbi_image_free`.
6. Pozostałe wystąpienia z `grep`: `LabHUDEditor.cpp`, `LabPlugin.cpp`, `LabCommand.cpp`, `LabSceneNode.h`, `LabProject.h`, `main.cpp`, `editor_main.cpp`.
- Przejście modułami. Przed każdym krokiem test z fazy 2 musi przechodzić.
- Opakowania RAII dla zasobów GPU (tekstura, bufor, shader, VAO). Wrappery C-API są dozwolone jako wyjątek.
- *Akceptacja:* ASan czysty. Brak surowych wskaźników jako właścicieli w polach klas.

**3.4 Porządki w TODO/FIXME**
- Przejdź 49 znaczników. Każdy: napraw, zamień na issue w GitHubie albo usuń jako nieaktualny.
- *Akceptacja:* w `src/` i `include/` zostają tylko znaczniki z numerem issue.

### Faza 4: renderer w stylu HL2 (po fazach 1–3)

Cel: domknąć wizualny cel. Każdy punkt to osobny PR z testem wizualnym (zrzut ekranu porównywany z oczekiwanym).

**4.1 Normal mapping**
- Sprawdź, czy shader sceny próbkuje `normalTex`. Jeśli nie, podepnij go i dodaj tangenty do siatek.
- Ostre normal mapy: bez wygładzania mip-map, które zmiękcza detal.
- *Akceptacja:* test wizualny z płaską powierzchnią i normal mapą pokazuje detal.

**4.2 Lightmapy**
- Sprawdź, czy `BakedLightmap` jest używany w shaderze sceny. Jeśli nie, podepnij go jako drugi zestaw UV.
- *Akceptacja:* oświetlenie statyczne pochodzi z lightmapy, a nie z obliczeń w czasie rzeczywistym.

**4.3 Color grading**
- Potwierdź implementację „Filmic Color Grading” w `LabPostProcess.cpp`. Jeśli to tylko komentarz, zaimplementuj korekcję kolorów (np. LUT albo krzywe kontrastu i nasycenia).
- *Akceptacja:* parametry korekcji zmieniają obraz w sposób mierzalny w teście.

**4.4 Anti-aliasing: FXAA**
- Dodaj FXAA jako krok po post-processingu. MSAA tylko jeśli FXAA nie wystarcza. TAA nie wchodzi w grę.
- *Akceptacja:* krawędzie na teście wizualnym są gładsze, bez rozmycia tekstur.

**4.5 Weryfikacja HL2-like**
- Potwierdź bloom subtelny (nie dominujący), HDR, winietę i ostre cienie kierunkowe na scenie testowej.
- *Akceptacja:* zrzut sceny testowej zatwierdzony przez właściciela jako zgodny z celem.

### Faza 5: funkcje (dopiero po fazie 4)

Kolejność według wpływu na grę FPS:
1. Menedżer scen (`SceneManager`), bo bez niego gra nie ma przejść.
2. Cache zasobów z liczeniem referencji.
3. Asynchroniczne ładowanie tekstur i modeli.
4. NavMesh i A\* (w oparciu o istniejący znacznik „NavMesh Obstacle Box”).
5. LOD i occlusion culling.
6. Blend Trees i IK.
7. Terrain.
8. Pozostałe funkcje z TODO.md.

Vulkan i DirectX 12 nie są w tym planie. Wracają tylko po decyzji właściciela.

## 4. Jak sprawdzić, że faza jest zamknięta

- [ ] Build bez ostrzeżeń na GCC/Clang/MSVC.
- [ ] CI zielone na Windows i Linuxie.
- [ ] Sanitizery czyste na testach.
- [ ] README i tabela statusu zgodne z kodem.
- [ ] Brak nowych znaczników TODO bez numeru issue.
- [ ] Brak kopiowalnych klas z ręcznym zwalnianiem.
- [ ] Przegląd człowieka dla zmian w fizyce, GPU i serializacji.
- [ ] Dla fazy 4: zrzuty testowe zatwierdzone przez właściciela.

## 5. Ryzyka

| Ryzyko | Prawdopodobieństwo | Co robić |
|---|---|---|
| Refaktor psuje zachowanie bez zauważenia | wysokie | testy przed refaktorem, małe PR-y |
| Kopia klasy z surowym wskaźnikiem zwalnia zasób dwa razy | nieznane, do sprawdzenia | krok 1.4 |
| Błąd w fizyce tylko w jednym scenariuszu | średnie | testy właściwości, ręczny przegląd |
| Czarny ekran po zmianie GPU | średnie | testy wizualne, debug OpenGL (1.5) |
| Wycieki pamięci w ścieżkach błędów | nieznane, do zmierzenia | ASan w CI, RAII |
| Lightmapy i normal mapy nie są podpięte | średnie | sprawdzenie w 4.1 i 4.2 przed pracą |
| Projekt rozrasta się szybciej niż naprawa | wysokie | faza 5 dopiero po 1–4 |
| Dokumentacja obiecuje więcej niż kod | wysokie | tabela statusu, sekcja ograniczeń |

## 6. Otwarte pytania dla właściciela

1. Czy Vulkan i DX12 zostają całkowicie poza planem, czy są na późniejszy etap? (Obecny plan zakłada, że są poza zakresem.)
2. Czy projekt będzie udostępniany publicznie, i w jakiej formie? (Wpływa na ostrość deklaracji w README.)
3. Kto przegląda PR-y? (Bez przeglądu człowieka fazy 3 i 4 nie są bezpieczne.)
4. Czy dla testów wizualnych HL2-like możemy przyjąć jedną scenę referencyjną jako punkt odniesienia?
