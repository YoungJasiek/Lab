# PR #6: `ai/warnings-from-chars-parsers` — `sscanf` → `std::from_chars` w `LabAsset2D.cpp` i `LabTheme.cpp`

- **Gałąź:** `ai/warnings-from-chars-parsers` (oparta o `origin/main` `6842c94`)
- **Commity:**
  - `8a67f2f` — `test(render): add round-trip tests for SpriteSheet2D and LabTheme JSON parsers`
  - `28dce0f` — `fix(render): replace sscanf with std::from_chars in LabAsset2D and LabTheme`
  - `1042839` — `test(parsers): move SpriteSheet2D and LabTheme round-trip tests into Test 82 Sub-test E`

## Lista zmienionych plików
- `src/test_verify.cpp`
- `src/LabAsset2D.cpp`
- `src/LabTheme.cpp`

## Wynik kompilacji (GCC i Clang — Debug i Release, MSVC)
- **GCC (Debug `-g` / Release `-O2`, `-std=c++20 -Wall -Wextra -Wpedantic -Werror`):** `0` błędów, `0` ostrzeżeń w `src/LabAsset2D.cpp` i `src/LabTheme.cpp`.
- **Clang (Debug `-g` / Release `-O2`, `-std=c++20 -Wall -Wextra -Wpedantic -Werror`):** `0` błędów, `0` ostrzeżeń w `src/LabAsset2D.cpp` i `src/LabTheme.cpp`.
- **MSVC (Debug / Release, `/W4`):** `0` błędów, `0` ostrzeżeń `C4996` w `src/LabAsset2D.cpp` i `src/LabTheme.cpp` (`LabRender.dll`, `TestVerify.exe`).

## Wynik testów (PASS / SKIP / FAIL)
- `TestVerify`: **PASS=86, SKIP=0, FAIL=0 (TOTAL=86)** — kod wyjścia `0` (w tym nowy `Sub-test E` wewnątrz `[Test 82]` weryfikujący round-trip `SpriteSheet2D::saveToFile`/`loadFromFile` oraz `LabTheme::saveThemeToJson`/`loadThemeFromJson` dla tablic `[r, g, b]` i hex `"#RRGGBB"`).

## Czy zachowanie się zmieniło
- **Nie** (dodatkowo parsowanie liczb zmiennoprzecinkowych przez `std::from_chars` jest niezależne od lokalizacji `LC_NUMERIC`).
