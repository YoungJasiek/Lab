# AGENTS.md: zasady pracy nad Lab Engine

Ten plik dotyczy każdego narzędzia AI (i człowieka) pracującego nad repozytorium. Czytaj go przed każdą zmianą. Plan prac jest w `PLAN.md`.

## Kontekst

- **Silnik:** Lab, FPS w stylu Source Engine (Half-Life 2).
- **Język:** nowoczesne C++20 (C++23 tylko tam, gdzie kompilatory to wspierają).
- **Grafika:** OpenGL 4.5+ Core, Direct State Access (DSA). Minimum 4.5, zalecane 4.6.
- **Cel:** czysty, dostępny, skalowalny silnik. Wydajność liczy się od początku.

## Styl wizualny (stały)

- Oświetlenie: **Blinn-Phong / Phong. Bez PBR.**
- Detal: ostre normal mapy, lightmapy, cubemapy.
- Post-processing: subtelny bloom, HDR, korekcja kolorów, winieta.
- Anti-aliasing: **FXAA lub MSAA. Bez TAA.**
- Bez ciężkiego SSAO i fotorealizmu.
- Zmiana któregokolwiek z tych punktów wymaga decyzji właściciela, nie PR-a.

## UI / UX i nazewnictwo przycisków

- Etykiety przycisków, narzędzi i ikon w edytorach (Hammer, LFM, Studio, HUD) muszą być **krótkie, zwarte i czytelne** (zazwyczaj 1–2 słowa).
- Nigdy nie używaj rozwlekłych opisów technicznych na przyciskach i ikonach.
- Zestawienie standardów:
  - Używaj **Bake** zamiast Bake Production Map / Bake Production .lmap.
  - Używaj **Render** lub **Export** zamiast Render via ffmpeg / Export Cinematic MP4.
  - Używaj **Play** lub **Run** zamiast Launch Game Simulation.
  - Używaj **Build** zamiast Build All Geometry.
  - Używaj **Save** zamiast Save Map File to Disk.

## Zasady kodu

**Zawsze:**
1. Używaj RAII. Właściciel zasobu to `std::unique_ptr`, `std::shared_ptr` albo kontener.
2. Używaj `std::span` zamiast surowych wskaźnik + rozmiar.
3. Pisz kod const-correct.
4. Traktuj kontekst OpenGL i zasoby GPU jako obiekty RAII.
5. Trzymaj się C++ Core Guidelines.
6. Preferuj abstrakcje o zerowym koszcie.
7. Myśl o cache i designie zorientowanym na dane.
8. Kompiluj z `-Wall -Wextra -Wpedantic -Werror` (MSVC: `/W4 /WX`).
9. Mierz przed optymalizacją (profiler w silniku, Tracy, Optick).

**Nigdy:**
1. Nie używaj surowego `new`/`delete` w kodzie silnika ani gry. Wyjątki: wewnątrz opakowań RAII, wrapperów C-API (OpenGL) i alokatorów.
2. Nie mieszaj legacy OpenGL (`glBegin`/`glEnd`, funkcje fixed-function). Tylko core profile.
3. Nie twórz globalnego, stanowego OpenGL. Stan przekazuj jawnie.
4. Nie ignoruj własności i czasu życia obiektów.
5. Nie dodawaj zależności bez zgody właściciela.

**Architektura:**
- Silnik ma już architekturę komponentową (`Entity` / `Component`). Nie przepisuj jej na pełny ECS bez uzasadnienia i testów.
- Unikaj globalnego stanu. Preferuj wstrzykiwanie zależności albo lokalizator usług.
- Każdy system silnika ma jasne API i testy.
- Zachowaj hot-reload shaderów i zasobów.

## Zasady pracy

1. Nie pracuj bezpośrednio na `main`. Jedno zadanie = jedna gałąź `ai/<nazwa>` albo `feat/<nazwa>`.
2. Najpierw test, potem zmiana. Przy refaktorze test na obecne zachowanie musi przejść przed zmianą i po niej.
3. Małe PR-y: ok. 400 zmienionych linii, poza mechanicznymi przeniesieniami.
4. Każdy PR zawiera: opis, listę plików, wynik kompilacji, wynik testów, wynik sanitizerów, informację, czy zachowanie się zmieniło.
5. Kod GPU, fizykę i serializację przegląda człowiek.
6. Nie dodawaj nowych funkcji, dopóki fazy 1–3 z `PLAN.md` nie są zamknięte.
7. Nie dokumentuj tego, czego nie ma w kodzie.

## Format wyjścia

Przy generowaniu kodu podawaj:
- Nowoczesny C++ (minimum C++20).
- Nagłówki z `#pragma once` albo strażnikami include.
- Podział na `.h` i `.cpp` (albo moduły).
- `CMakeLists.txt`, jeśli dodajesz pliki lub targety.
- Komentarze tylko tam, gdzie tłumaczą *dlaczego*, nie *co*.
- Przykład użycia API silnika.
- Uwagi o wydajności i typowych wąskich gardłach.

## Język

- Odpowiadaj po polsku, chyba że użytkownik poprosi inaczej.
- Kod, identyfikatory i komentarze w kodzie piszemy po angielsku.
- Bądź dostępny, techniczny i praktyczny.
