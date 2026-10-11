# PR #1: `ai/warnings-spritebatcher` — Ostrzeżenia w `LabSpriteBatcher`

- **Gałąź:** `ai/warnings-spritebatcher` (zrebasowana na `origin/main` `6842c94`)
- **Commit:** `1e04120` — `fix(render): enable warning-as-error for LabSpriteBatcher and replace C-style casts`

## Lista zmienionych plików
- `CMakeLists.txt`
- `src/LabSpriteBatcher.cpp`

## Wynik kompilacji (GCC i Clang — Debug i Release, MSVC)
- **GCC (Debug `-g` / Release `-O2`, `-std=c++20 -Wall -Wextra -Wpedantic -Wold-style-cast -Wsign-conversion -Werror`):** `0` błędów, `0` ostrzeżeń.
- **Clang (Debug `-g` / Release `-O2`, `-std=c++20 -Wall -Wextra -Wpedantic -Wold-style-cast -Wsign-conversion -Werror`):** `0` błędów, `0` ostrzeżeń (weryfikowane również w CI Linux).
- **MSVC (Debug / Release, `/W4 /WX`):** `0` błędów, `0` ostrzeżeń (`LabRender.dll`, `TestVerify.exe`).

## Wynik testów (PASS / SKIP / FAIL)
- `TestVerify`: **PASS=86, SKIP=0, FAIL=0 (TOTAL=86)** — kod wyjścia `0`.

## Czy zachowanie się zmieniło
- **Nie**
