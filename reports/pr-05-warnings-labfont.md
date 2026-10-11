# PR #5: `ai/warnings-labfont` — `include/LabFont.h` i `src/LabCore.cpp` (`STB_TRUETYPE_IMPLEMENTATION` / `C4505`)

- **Gałąź:** `ai/warnings-labfont` (oparta o `origin/main` `6842c94`)
- **Commity:**
  - `96200d8` — `fix(core): remove STBTT_STATIC and suppress C4505 around stb_truetype.h in LabFont.h`
  - `3450561` — `fix(core): move STB_TRUETYPE_IMPLEMENTATION from LabFont.h to LabCore.cpp`

## Lista zmienionych plików
- `include/LabFont.h` (zawiera wyłącznie deklaracje `stb_truetype.h`, bez `STB_TRUETYPE_IMPLEMENTATION` i bez `STBTT_STATIC`)
- `src/LabCore.cpp` (zawiera jedyną w `LabCore` definicję `#define STB_TRUETYPE_IMPLEMENTATION` owiniętą w `#pragma warning(push)`, `#pragma warning(disable: 4505)`, `#pragma warning(pop)`)

## Wynik kompilacji (GCC i Clang — Debug i Release, MSVC)
- **GCC (Debug `-g` / Release `-O2`, `-std=c++20 -DLAB_CORE_BUILD -Wall -Wextra -Wpedantic`):** `0` błędów, `0` ostrzeżeń (`src/LabCore.cpp`, `include/LabFont.h`).
- **Clang (Debug `-g` / Release `-O2`, `-std=c++20 -DLAB_CORE_BUILD -Wall -Wextra -Wpedantic`):** `0` błędów, `0` ostrzeżeń.
- **MSVC (Debug / Release, `/W4`):** `0` błędów, `0` ostrzeżeń `C4505` i **`0` błędów linkera `LNK2005`** przy budowie wszystkich celów (`LabCore`, `LabRender`, `LabWorld`, `LabHUDEditorCore`, `LabEditor`, `LabPlayer`, `LabServer`, `TestVerify`).

## Wynik testów (PASS / SKIP / FAIL)
- `TestVerify`: **PASS=86, SKIP=0, FAIL=0 (TOTAL=86)** — kod wyjścia `0`.

## Czy zachowanie się zmieniło
- **Nie**
