# Pytania do właściciela projektu

## 1. [Zadanie 1 — `map.yaml`] Przeznaczenie nieśledzonego pliku `map.yaml`
- **Kontekst:** W katalogu głównym repozytorium znajduje się nieśledzony plik `map.yaml`, który zgodnie z poleceniem nie został zacommitowany na żadnej gałęzi.
- **Pytanie:** Czy `map.yaml` ma zostać dodany do `.gitignore`, przeniesiony do katalogu narzędziowego/dokumentacji, czy zacommitowany w osobnym PR po wyjaśnieniu jego roli?
- **Proponowana odpowiedź:** Dodać `map.yaml` do `.gitignore`, dopóki nie jest używany przez kod silnika ani pipeline CI.

---

## Rozstrzygnięte i zrealizowane w tej iteracji
- **[Rozstrzygnięte — Zadanie 4 (`include/LabFont.h`)]:** Przeniesiono `#define STB_TRUETYPE_IMPLEMENTATION` oraz owinięty w `#pragma warning(push/disable:4505/pop)` `#include "stb_truetype.h"` z `include/LabFont.h` do `src/LabCore.cpp` (`ai/warnings-labfont`, commit `3450561`), pozostawiając w `include/LabFont.h` wyłącznie deklaracje i weryfikując brak błędu `LNK2005`.
- **[Rozstrzygnięte — Zadanie 2, 3, 6 (`tools/checks/` i `ai/chore-checks-clean`)]:** Wykonano osobny mechaniczny PR (`ai/chore-checks-clean`, commit `71fa018`) z `check_trailing_whitespace.py --fix` na 22 plikach, `check_includes.py --fix` na 3 plikach i `check_test_registry.py --fix` aktualizującym `README.md` do kanonicznej liczby `86` testów (`[Test 1]`..`[Test 86]`), po którym wszystkie 5 skryptów w trybie raportu zwraca kod `0`.
