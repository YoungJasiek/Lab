# CountMyCode.ps1
# -------------------------------------------------
# Policz linie kodu własnego (bez bibliotek)
# -------------------------------------------------

# -----------------------------------------------------------------
# 1️⃣ Katalogi, które chcemy pominąć – zwykle zawierają zależności
# -----------------------------------------------------------------
$excludeDirs = @(
    '.git',
    'build',
    'vendor',
    'third_party',
    'external',
    'libs',          # własny folder z bibliotekami, jeśli istnieje
    'CMakeFiles'     # pliki generowane przez CMake
)

# -----------------------------------------------------------------
# 2️⃣ Rozszerzenia plików C/C++ które nas interesują
# -----------------------------------------------------------------
$extensions = @('*.cpp','*.c','*.h','*.hpp')

# -----------------------------------------------------------------
# 3️⃣ Znajdź wszystkie pasujące pliki, wykluczając wyżej wymienione
# -----------------------------------------------------------------
$files = Get-ChildItem -Recurse -File -Include $extensions |
    Where-Object {
        # Ścieżka względna względem bieżącego katalogu
        $relative = $_.FullName.Substring($PWD.Path.Length + 1)
        -not ($excludeDirs | Where-Object { $relative -like "$_*" })
    }

# -----------------------------------------------------------------
# 4️⃣ Inicjalizacja liczników
# -----------------------------------------------------------------
$totalLines   = 0
$codeLines    = 0
$emptyLines   = 0
$commentLines = 0

# -----------------------------------------------------------------
# 5️⃣ Przetwarzaj każdy plik
# -----------------------------------------------------------------
foreach ($file in $files) {
    # Pobierz cały plik jako jedną całość (tekst)
    $rawContent = Get-Content -Path $file.FullName -Raw -ErrorAction SilentlyContinue -Encoding utf8

    # Podziel na linie – obsługuje zarówno \n jak i \r\n
    $lines = $rawContent -split "`r?`n"

    $totalLines += $lines.Count

    foreach ($line in $lines) {
        $trim = $line.Trim()
        if ($trim -eq '') {
            $emptyLines++
        } elseif ($trim.StartsWith('//') -or $trim.StartsWith('/*') -or $trim.EndsWith('*/')) {
            # Proste wykrywanie komentarzy C/C++
            $commentLines++
        } else {
            $codeLines++
        }
    }
}

# -----------------------------------------------------------------
# 6️⃣ Wyświetl podsumowanie
# -----------------------------------------------------------------
Write-Host "`n===== Wynik liczenia linii kodu ====="
Write-Host "Plikow przeszukanych                : $($files.Count)"
Write-Host "Laczna liczba linii (wszystkie)    : $totalLines"
Write-Host "Linie kodu (bez pustych i komentarzy): $codeLines"
Write-Host "Puste linie                         : $emptyLines"
Write-Host "Linie komentarzy                    : $commentLines"
Write-Host "=========================================`n"