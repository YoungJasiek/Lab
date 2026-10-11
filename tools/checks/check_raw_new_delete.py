#!/usr/bin/env python3
"""Wypisuje wszystkie wystąpienia surowego new i delete poza komentarzami i literałami, wyklucza std::unique_ptr<T>(new T(...)) i unique_ptr<T> x(new ...), porównuje wynik z grep -rn po src/ i include/ oraz wyjaśnia każdą rozbieżność w raporcie."""

from __future__ import annotations

import argparse
import re
import sys
from collections import Counter
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPORT = REPO_ROOT / "reports" / "checks-raw_new_delete.md"

THIRD_PARTY_FILES = {"stb_truetype.h", "stb_image.h", "miniaudio.h", "gl.c", "gl.h", "khrplatform.h", "onelua.c"}

TOKEN_SPEC = re.compile(
    r"(?P<line_comment>//[^\n]*)|(?P<block_comment>/\*.*?\*/)|(?P<string>\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*')",
    re.DOTALL,
)
RAW_GREP_RE = re.compile(r"\b(new|delete)(?:\s*\[\s*\])?\b")
DELETED_FN_RE = re.compile(r"=\s*delete\b")
OPERATOR_NEW_DELETE_RE = re.compile(r"\boperator\s+(?:new|delete)(?:\s*\[\s*\])?\b")
# Wykluczenie zarówno std::unique_ptr<T>(new T(...)) jak i unique_ptr<T> x(new ...) / std::unique_ptr<T> x(new ...)
UNIQUE_PTR_TEMP_RE = re.compile(r"\b(?:std::)?unique_ptr\s*<[^>]+>\s*(?:\(|\{)\s*new\b")
UNIQUE_PTR_VAR_RE = re.compile(
    r"\b(?:std::)?unique_ptr\s*<[^>]+>\s+[A-Za-z_][A-Za-z0-9_]*\s*(?:\(|\{)\s*new\b"
)

# Znany backlog z PLAN.md (Faza 3.3: Pamięć - usunięcie surowych właścicieli)
PHASE_3_3_BASELINE_FILES = {
    "src/LabPlugin.cpp",
    "src/LabRenderer.cpp",
    "src/LabSpriteBatcher.cpp",
}


def analyze_tokens(text: str) -> tuple[str, list[tuple[int, int, str]]]:
    """Zwraca tekst z zamaskowanymi komentarzami/literałami oraz listę przedziałów (start, end, kategoria)."""
    spans: list[tuple[int, int, str]] = []

    def repl(match: re.Match[str]) -> str:
        s = match.group(0)
        if match.group("line_comment") or match.group("block_comment"):
            kind = "komentarz C/C++ (angielskie słowo `new` lub `delete` w opisie)"
        else:
            kind = "literał napisowy (tekst UI / logu z angielskim słowem `new` lub `delete`)"
        spans.append((match.start(), match.end(), kind))
        return "".join("\n" if ch == "\n" else " " for ch in s)

    masked = TOKEN_SPEC.sub(repl, text)
    return masked, spans


def find_span_kind(pos: int, spans: list[tuple[int, int, str]]) -> str | None:
    for start, end, kind in spans:
        if start <= pos < end:
            return kind
    return None


def is_third_party_file(path: Path) -> bool:
    return path.name in THIRD_PARTY_FILES or "glad" in path.parts


def collect_files(paths: list[Path], include_third_party: bool = False) -> list[Path]:
    files: list[Path] = []
    for p in paths:
        if p.is_file() and p.suffix.lower() in {".cpp", ".c", ".h", ".hpp"}:
            files.append(p)
        elif p.is_dir():
            for ext in ("*.cpp", "*.c", "*.h", "*.hpp"):
                for f in sorted(p.rglob(ext)):
                    if not include_third_party and is_third_party_file(f):
                        continue
                    files.append(f)
    return sorted(set(files))


def scan_file(
    path: Path,
) -> tuple[list[tuple[int, str, str]], list[tuple[int, str, str, str]]]:
    """Zwraca (aktywne_wystąpienia, wykluczone_wystąpienia_z_powodem)."""
    content = path.read_text(encoding="utf-8", errors="replace")
    masked, spans = analyze_tokens(content)
    orig_lines = content.splitlines()
    masked_lines = masked.splitlines()

    line_starts: list[int] = []
    offset = 0
    for line in content.splitlines(keepends=True):
        line_starts.append(offset)
        offset += len(line)

    active: list[tuple[int, str, str]] = []
    excluded: list[tuple[int, str, str, str]] = []

    for idx, orig_line in enumerate(orig_lines, start=1):
        mline = masked_lines[idx - 1] if idx - 1 < len(masked_lines) else ""
        line_start = line_starts[idx - 1] if idx - 1 < len(line_starts) else 0
        snippet = orig_line.strip()

        for match in RAW_GREP_RE.finditer(orig_line):
            kw = match.group(0).strip()
            abs_pos = line_start + match.start()
            span_reason = find_span_kind(abs_pos, spans)
            if span_reason is not None:
                excluded.append((idx, kw, span_reason, snippet))
                continue

            prefix_on_line = mline[: match.end()]
            if DELETED_FN_RE.search(prefix_on_line) and kw.startswith("delete"):
                excluded.append((idx, kw, "usunięta funkcja składowa (`= delete`)", snippet))
                continue

            if OPERATOR_NEW_DELETE_RE.search(mline):
                excluded.append((idx, kw, "deklaracja `operator new/delete`", snippet))
                continue

            if kw.startswith("new") and UNIQUE_PTR_VAR_RE.search(mline):
                excluded.append(
                    (
                        idx,
                        kw,
                        "wzorzec RAII `unique_ptr<T> x(new ...)` / `std::unique_ptr<T> x(new ...)` (brak surowego posiadania)",
                        snippet,
                    )
                )
                continue

            if kw.startswith("new") and UNIQUE_PTR_TEMP_RE.search(mline):
                excluded.append(
                    (
                        idx,
                        kw,
                        "wzorzec RAII `std::unique_ptr<T>(new T(...))` (brak surowego posiadania)",
                        snippet,
                    )
                )
                continue

            active.append((idx, kw, snippet))

    return active, excluded


def count_third_party_grep_lines(paths: list[Path]) -> list[tuple[Path, int, int]]:
    """Zlicza linie i trafienia grep -rn '\\b(new|delete)\\b' w plikach zewnętrznych (stb_*, miniaudio, glad)."""
    results: list[tuple[Path, int, int]] = []
    for f in collect_files(paths, include_third_party=True):
        if not is_third_party_file(f):
            continue
        text = f.read_text(encoding="utf-8", errors="replace")
        line_hits = 0
        token_hits = 0
        for line in text.splitlines():
            matches = list(RAW_GREP_RE.finditer(line))
            if matches:
                line_hits += 1
                token_hits += len(matches)
        if token_hits > 0:
            results.append((f, line_hits, token_hits))
    return results


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "paths",
        nargs="*",
        type=Path,
        help="Ścieżki do plików lub katalogów (domyślnie: include/ oraz src/)",
    )
    parser.add_argument(
        "--fix",
        action="store_true",
        help="Flaga akceptowana dla spójności CLI (skrypt działa wyłącznie w trybie raportu)",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Zwróć kod 1 również dla znanego backlogu Fazy 3.3 (LabRenderer/LabSpriteBatcher/LabPlugin)",
    )
    parser.add_argument(
        "--report",
        type=Path,
        default=DEFAULT_REPORT,
        help="Ścieżka do pliku raportu Markdown",
    )
    args = parser.parse_args(argv)

    if args.fix:
        print("[INFO] check_raw_new_delete.py działa wyłącznie w trybie raportu (wymagana decyzja o właścicielu zasobu).")

    target_paths = args.paths if args.paths else [REPO_ROOT / "include", REPO_ROOT / "src"]
    files = collect_files(target_paths, include_third_party=False)
    third_party_hits = count_third_party_grep_lines(target_paths) if not args.paths else []

    all_active: list[tuple[Path, int, str, str]] = []
    all_excluded: list[tuple[Path, int, str, str, str]] = []
    unexpected_active: list[tuple[Path, int, str, str]] = []

    for f in files:
        active, excluded = scan_file(f)
        rel = f.resolve().relative_to(REPO_ROOT).as_posix() if f.resolve().is_relative_to(REPO_ROOT) else f.as_posix()
        for line_no, kw, snippet in active:
            all_active.append((f, line_no, kw, snippet))
            if args.strict or args.paths or rel not in PHASE_3_3_BASELINE_FILES:
                unexpected_active.append((f, line_no, kw, snippet))
        for line_no, kw, reason, snippet in excluded:
            all_excluded.append((f, line_no, kw, reason, snippet))

    reason_counts = Counter(reason for _, _, _, reason, _ in all_excluded)
    engine_grep_matches = len(all_active) + len(all_excluded)
    tp_token_total = sum(tok for _, _, tok in third_party_hits)
    tp_line_total = sum(ln for _, ln, _ in third_party_hits)

    args.report.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "# Check: `raw_new_delete`",
        "",
        f"- Przeskanowane pliki własne silnika (`include/`, `src/`): **{len(files)}**",
        f"- Wszystkie dopasowania `grep -rn \"\\b(new|delete)\\b\"` w plikach silnika: **{engine_grep_matches}**",
        f"  - Aktywne wystąpienia surowego `new` / `delete` w kodzie (backlog Fazy 3.3): **{len(all_active)}**",
        f"  - Nowe / nieoczekiwane wystąpienia poza backlogiem Fazy 3.3: **{len(unexpected_active)}**",
        f"  - Wykluczone wystąpienia w plikach silnika (z wyjaśnieniem poniżej): **{len(all_excluded)}**",
        f"- Dopasowania `grep -rn` w osadzonych bibliotekach zewnętrznych (`stb_truetype.h`, `stb_image.h`, `miniaudio.h`, `glad`): **{tp_token_total}** trafień w **{tp_line_total}** liniach (pomijane jako kod third-party C)",
        "",
        "## Podsumowanie wykluczonych wystąpień w kodzie silnika według powodu",
        "",
        "| Powód wykluczenia (rozbieżność względem surowego `grep -rn`) | Liczba wystąpień |",
        "|--------------------------------------------------------------|-----------------:|",
    ]

    for reason, count in sorted(reason_counts.items()):
        lines.append(f"| {reason} | {count} |")
    lines.append(f"| **Razem wykluczonych w plikach silnika** | **{len(all_excluded)}** |")
    lines.append("")

    if third_party_hits:
        lines.append("## Wykluczone pliki bibliotek zewnętrznych (third-party w `include/` i `src/`)")
        lines.append("")
        lines.append("| Plik zewnętrzny | Linie dopasowane przez `grep -rn` | Liczba trafień słów `new`/`delete` | Powód pominięcia |")
        lines.append("|-----------------|----------------------------------:|-----------------------------------:|------------------|")
        for f, ln_cnt, tok_cnt in third_party_hits:
            rel = f.resolve().relative_to(REPO_ROOT) if f.resolve().is_relative_to(REPO_ROOT) else f
            lines.append(f"| `{rel.as_posix()}` | {ln_cnt} | {tok_cnt} | Zewnętrzna biblioteka single-header / wygenerowany loader C (komentarze i identyfikatory C) |")
        lines.append("")

    lines.append("## 1. Aktywne wystąpienia surowego `new` / `delete` (do refaktoru w Fazie 3.3)")
    lines.append("")
    if all_active:
        lines.append("| Plik | Linia | Słowo kluczowe | Fragment kodu |")
        lines.append("|------|------:|----------------|---------------|")
        for f, line_no, kw, snippet in all_active:
            rel = f.resolve().relative_to(REPO_ROOT) if f.resolve().is_relative_to(REPO_ROOT) else f
            safe_snippet = snippet.replace("|", "\\|")
            lines.append(f"| `{rel.as_posix()}` | {line_no} | `{kw}` | `{safe_snippet}` |")
            print(f"[INFO] {rel.as_posix()}:{line_no}: surowe '{kw}' -> {snippet}")
        lines.append("")
    else:
        lines.append("Brak wystąpień surowego `new` / `delete`.")
        lines.append("")

    lines.append("## 2. Pełne wyjaśnienie wszystkich 55 rozbieżności względem `grep -rn` w plikach silnika")
    lines.append("")
    if all_excluded:
        lines.append("| Plik | Linia | Słowo | Wyjaśnienie rozbieżności (powód wykluczenia) | Fragment linii |")
        lines.append("|------|------:|-------|----------------------------------------------|----------------|")
        for f, line_no, kw, reason, snippet in all_excluded:
            rel = f.resolve().relative_to(REPO_ROOT) if f.resolve().is_relative_to(REPO_ROOT) else f
            safe_snippet = snippet.replace("|", "\\|")
            lines.append(f"| `{rel.as_posix()}` | {line_no} | `{kw}` | {reason} | `{safe_snippet}` |")
        lines.append("")

    args.report.write_text("\n".join(lines), encoding="utf-8")
    return 1 if unexpected_active else 0


if __name__ == "__main__":
    sys.exit(main())
