# PR #4: `ai/fix-network-socket-sign` — `UDPSocket::open` (`-Wsign-compare` z `INVALID_SOCKET`)

- **Gałąź:** `ai/fix-network-socket-sign` (oparta o `origin/main` `6842c94`)
- **Commity:**
  - `e38d214` — `fix(network): use SOCKET type in UDPSocket::open to fix -Wsign-compare with INVALID_SOCKET`
  - `9d82a2e` — `test(network): move UDPSocket test into Test 33 and verify SOCKET vs intptr_t comparison`

## Lista zmienionych plików
- `src/LabNetwork.cpp`
- `src/test_verify.cpp`

## Potwierdzenie niezmienności wyniku porównania `SOCKET` vs `intptr_t` z `INVALID_SOCKET` (Windows x64)
Na Windows x64 (`_WIN64`) w `winsock2.h`:
- `typedef UINT_PTR SOCKET;` (`uintptr_t` / `unsigned long long`, 64-bitowy typ bezznakowy)
- `#define INVALID_SOCKET (SOCKET)(~0)` (`0xFFFFFFFFFFFFFFFFULL` = `18446744073709551615ULL`)

| Przypadek | Wartość zwrócona przez `socket()` (`raw`) | Stary kod: `intptr_t s = raw; (s == INVALID_SOCKET)` | Nowy kod: `SOCKET s = raw; (s == INVALID_SOCKET)` | Czy wynik identyczny? |
|---|---|---|---|---|
| **Błąd (`socket()` zwraca `INVALID_SOCKET`)** | `0xFFFFFFFFFFFFFFFFULL` | `s = -1LL`; po konwersji arytmetycznej do `SOCKET` (`0xFFFFFFFFFFFFFFFFULL == 0xFFFFFFFFFFFFFFFFULL`) → **`true`** | `s = 0xFFFFFFFFFFFFFFFFULL`; (`0xFFFFFFFFFFFFFFFFULL == 0xFFFFFFFFFFFFFFFFULL`) → **`true`** | **Tak (`true == true`)** |
| **Sukces (`socket()` zwraca np. uchwyt `128`)** | `128ULL` (`0x80ULL`) | `s = 128LL`; po konwersji do `SOCKET` (`128ULL == 0xFFFFFFFFFFFFFFFFULL`) → **`false`** | `s = 128ULL`; (`128ULL == 0xFFFFFFFFFFFFFFFFULL`) → **`false`** | **Tak (`false == false`)** |

## Wynik kompilacji (GCC i Clang — Debug i Release, MSVC)
- **GCC (Debug `-g` / Release `-O2`, `-std=c++20 -Wall -Wextra -Wpedantic -Werror`):** `0` błędów, `0` ostrzeżeń w `src/LabNetwork.cpp`.
- **Clang (Debug `-g` / Release `-O2`, `-std=c++20 -Wall -Wextra -Wpedantic -Werror`):** `0` błędów, `0` ostrzeżeń w `src/LabNetwork.cpp`.
- **MSVC (Debug / Release, `/W4`):** `0` błędów, `0` ostrzeżeń w `src/LabNetwork.cpp` (`LabNetwork.dll`, `TestVerify.exe`).

## Wynik testów (PASS / SKIP / FAIL)
- `TestVerify`: **PASS=86, SKIP=0, FAIL=0 (TOTAL=86)** — kod wyjścia `0` (w tym rozszerzony `[Test 33]` weryfikujący równoważność starego i nowego porównania `INVALID_SOCKET` dla błędu i sukcesu oraz `UDPSocket::open(0)`, `sendTo`/`recvFrom` po loopbacku `127.0.0.1` i `close()`).

## Czy zachowanie się zmieniło
- **Nie**
