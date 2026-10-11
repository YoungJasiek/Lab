# PR #2: `ai/ci-linux` — Job CI na Linuxie (Faza 1.2), Mesa + `xvfb-run` i skrypty kontrolne Pythona

- **Gałąź:** `ai/ci-linux` (oparta o `origin/main` `6842c94`)
- **Commity:**
  - `4a1457d` — `ci: add Linux GCC/Clang matrix, OpenGL headless skip, and non-blocking warnings job`
  - `e21ee01` — `ci(checks): add Python static check scripts in tools/checks and report-only CI steps`
  - `22fa825` — `ci(linux): run TestVerify via Mesa+xvfb-run, report PASS/SKIP/FAIL, and refine check scripts`
  - `7869f6e` — `ci(linux): dynamically count executed [Test N] blocks, fail on missing GL context unless allowed, and refine check scripts`
  - `0e80924` — `fix(checks): support UTF-16LE test logs in check_test_registry.py`

## Lista zmienionych plików
- `.github/workflows/c-cpp.yml`
- `src/test_verify.cpp`
- `tests/allowed_skips.txt`
- `tools/checks/check_pragma_once.py`
- `tools/checks/check_raw_new_delete.py`
- `tools/checks/check_trailing_whitespace.py`
- `tools/checks/check_includes.py`
- `tools/checks/check_test_registry.py`

## Wynik kompilacji (GCC i Clang — Debug i Release, MSVC)
- **GCC (Debug / Release, `-std=c++20 -Wall -Wextra -Wpedantic`):** `0` błędów.
- **Clang (Debug / Release, `-std=c++20 -Wall -Wextra -Wpedantic`):** `0` błędów (skonfigurowane w macierzy `build-linux` i `warnings`).
- **MSVC (Debug / Release, `/W4`):** `0` błędów (`TestVerify.vcxproj -> TestVerify.exe`).

## Wynik testów (PASS / SKIP / FAIL)
- `TestVerify`: **PASS=86, SKIP=0, FAIL=0 (TOTAL=86)** — kod wyjścia `0` (zliczane dynamicznie w czasie wykonania dla każdego bloku `[Test N]` przez `TestCountingStreambuf`).
  - Brak GLFW, okna lub kontekstu OpenGL 4.5+ jest traktowany jako **błąd (`FAIL=1, SKIP=0`, kod wyjścia `1`)**, chyba że pominięcie jest jawnie wpisane z uzasadnieniem w `tests/allowed_skips.txt`.
  - Na CI Linux zainstalowano `xvfb`, `libgl1-mesa-dri`, `libglx-mesa0`, `mesa-utils`, `xauth`, a `TestVerify` uruchamiany jest przez `xvfb-run -a -s "-screen 0 1280x720x24"`.
  - Job kończy się błędem (`exit 1`), jeśli `SKIP > 0` lub `FAIL > 0`.

## Czy zachowanie się zmieniło
- **Nie** (dodano dynamiczne zliczanie realnie wykonanych bloków `[Test 1]`..`[Test 86]` oraz raportowanie `[TestSummary] PASS=86 SKIP=0 FAIL=0 TOTAL=86`).
