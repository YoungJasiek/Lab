# PR #7: `ai/warnings-labproject-getenv` — Helper `getEnvVar` (`getenv_s` / `std::getenv`) w `LabProject.cpp`

- **Gałąź:** `ai/warnings-labproject-getenv` (oparta o `origin/main` `6842c94`)
- **Commit:** `9f92ff1` — `fix(core): wrap getenv in getEnvVar helper using getenv_s on MSVC`

## Lista zmienionych plików
- `src/LabProject.cpp`

## Wynik kompilacji (GCC i Clang — Debug i Release, MSVC)
- **GCC (Debug `-g` / Release `-O2`, `-std=c++20 -DLAB_CORE_BUILD -Wall -Wextra -Wpedantic -Werror`):** `0` błędów, `0` ostrzeżeń w `src/LabProject.cpp`.
- **Clang (Debug `-g` / Release `-O2`, `-std=c++20 -DLAB_CORE_BUILD -Wall -Wextra -Wpedantic -Werror`):** `0` błędów, `0` ostrzeżeń w `src/LabProject.cpp`.
- **MSVC (Debug / Release, `/W4`):** `0` błędów, `0` ostrzeżeń `C4996` w `src/LabProject.cpp` (`LabCore.dll`, `TestVerify.exe`).

## Wynik testów (PASS / SKIP / FAIL)
- `TestVerify`: **PASS=86, SKIP=0, FAIL=0 (TOTAL=86)** — kod wyjścia `0`.

## Czy zachowanie się zmieniło
- **Nie**
