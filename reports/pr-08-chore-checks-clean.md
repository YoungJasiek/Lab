# PR #8: `ai/chore-checks-clean` — Mechaniczny PR: `check_trailing_whitespace.py --fix`, `check_includes.py --fix`, `check_test_registry.py --fix`

- **Gałąź:** `ai/chore-checks-clean` (oparta o `origin/main` `6842c94`, osobno od zmian logicznych)
- **Commit:** `fa747ad` — `chore(checks): apply mechanical check fixes preserving original CRLF/LF line endings`

## Lista zmienionych plików
- **Usunięcie białych znaków na końcu linii z zachowaniem oryginalnych końców linii `\r\n` / `\n` (`check_trailing_whitespace.py --fix` — 22 pliki, dokładnie 139 linii):**
  - `include/LabEditor.h` (5 linii)
  - `include/LabHUD.h` (3 linie)
  - `include/LabInteractive.h` (1 linia)
  - `include/LabMap.h` (1 linia)
  - `include/LabMath.h` (1 linia)
  - `include/LabRenderer.h` (3 linie)
  - `include/LabWeapon.h` (1 linia)
  - `src/LabCore.cpp` (2 linie)
  - `src/LabExportPipeline.cpp` (4 linie)
  - `src/LabHUDEditor.cpp` (42 linie)
  - `src/LabNetwork.cpp` (4 linie)
  - `src/LabParticles.cpp` (1 linia)
  - `src/LabPlugin.cpp` (1 linia)
  - `src/LabPostProcess.cpp` (1 linia)
  - `src/LabProject.cpp` (1 linia)
  - `src/LabRenderer.cpp` (13 linii)
  - `src/LabStudio.cpp` (8 linii)
  - `src/editor_main.cpp` (5 linii)
  - `src/main.cpp` (2 linie)
  - `src/player_main.cpp` (4 linie)
  - `src/server_main.cpp` (1 linia)
  - `src/test_verify.cpp` (35 linii)
- **Zamiana cudzysłowów na nawiasy kątowe dla nagłówków zewnętrznych Lua (`check_includes.py --fix` — 3 pliki, 9 linii):**
  - `src/LabExportPipeline.cpp`
  - `src/LabScript.cpp`
  - `src/plugins/LuaDropinPlugin.cpp`
- **Synchronizacja liczby testów i badge w `README.md` na podstawie wykonanych testów z kroku 1 (`check_test_registry.py --fix` — 1 plik, 4 linie; bez modyfikacji `validating 77 engine subsystems` w linii 151):**
  - `README.md` (`77` → `86` w liniach 12, 33, 214, 243)
- **Skrypty kontrolne (`tools/checks/` — 5 plików):**
  - `tools/checks/check_includes.py`
  - `tools/checks/check_pragma_once.py`
  - `tools/checks/check_raw_new_delete.py`
  - `tools/checks/check_test_registry.py`
  - `tools/checks/check_trailing_whitespace.py`

## Wynik skryptów kontrolnych w trybie raportu (po PR)
- `python tools/checks/check_pragma_once.py` → kod wyjścia **`0`** (`78/78` nagłówków OK)
- `python tools/checks/check_trailing_whitespace.py` → kod wyjścia **`0`** (`144/144` plików wolnych od trailing whitespace)
- `python tools/checks/check_includes.py` → kod wyjścia **`0`** (`143/143` plików z poprawnymi `#include`)
- `python tools/checks/check_test_registry.py` → kod wyjścia **`0`** (wykonane testy z kroku 1: `86` == `README.md`: `86`)
- `python tools/checks/check_raw_new_delete.py` → kod wyjścia **`0`** (21 znanych wystąpień z backlogu Fazy 3.3, 55 wykluczonych wystąpień w kodzie silnika + wykluczone pliki third-party z pełnym wyjaśnieniem w `reports/checks-raw_new_delete.md`)

## Wynik kompilacji (GCC i Clang — Debug i Release, MSVC)
- **GCC (Debug `-g` / Release `-O2`, `-std=c++20 -Wall -Wextra -Wpedantic`):** `0` błędów.
- **Clang (Debug `-g` / Release `-O2`, `-std=c++20 -Wall -Wextra -Wpedantic`):** `0` błędów.
- **MSVC (Debug / Release, `/W4`):** `0` błędów.

## Wynik testów (PASS / SKIP / FAIL)
- `TestVerify`: **PASS=86, SKIP=0, FAIL=0 (TOTAL=86)** — kod wyjścia `0`.

## Czy zachowanie się zmieniło
- **Nie** (wyłącznie zmiany mechaniczne, dokładnie `152 insertions(+), 152 deletions(-)` w plikach źródłowych i `README.md` z zachowaniem oryginalnych końców linii `\r\n` i `\n`).
