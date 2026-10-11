#!/usr/bin/env python3
"""Wypisuje wszystkie wystąpienia surowego new i delete poza komentarzami i literałami, porównuje wynik z surowym grep po src/ i include/ oraz raportuje wykluczenia z podaniem powodu."""

from __future__ import annotations

import argparse
import re
import sys
from collections import Counter
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPORT = REPO_ROOT / "reports" / "checks-raw_new_delete.md"

EXCLUDED_NAMES = {"stb_truetype.h", "stb_image.h", "miniaudio.h", "gl.c", "gl.h", "khrplatform.h", "onelua.c"}

TOKEN_SPEC = re.compile(
    r"(?P<line_comment>//[^\n]*)|(?P<block_comment>/\*.*?\*/)|(?P<string>\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*')",
    re.DOTALL,
)
RAW_GREP_RE = re.compile(r"(?<![A-Za-z0-9_~])(new|delete)(?:\s*\[\s*\])?\b")
DELETED_FN_RE = re.compile(r"=\s*delete\b")
OPERATOR_NEW_DELETE_RE = re.compile(r"\boperator\s+(?:new|delete)(?:\s*\[\s*\])?\b")
UNIQUE_PTR_NEW_RE = re.compile(
    r"\b(?:std::)?unique_ptr\s*<[^>]+>\s*(?:[A-Za-z_][A-Za-z0-9_]*)?\s*(?:\(|\{)\s*new\b"
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
            kind = "komentarz (słowo w języku angielskim)"
        else:
            kind = "literał napisowy (tekst UI / logu)"
        spans.append((match.start(), match.end(), kind))
        return "".join("\n" if ch == "\n" else " " for ch in s)

    masked = TOKEN_SPEC.sub(repl, text)
    return masked, spans


def find_span_kind(pos: int, spans: list[tuple[int, int, str]]) -> str | None:
    for start, end, kind in spans:
        if start <= pos < end:
            return kind
    return None


def collect_files(paths: list[Path]) -> list[Path]:
    files: list[Path] = []
    for p in paths:
        if p.is_file() and p.suffix.lower() in {".cpp", ".c", ".h", ".hpp"}:
            files.append(p)
        elif p.is_dir():
            for ext in ("*.cpp", "*.h", "*.hpp"):
                for f in sorted(p.rglob(ext)):
                    if f.name in EXCLUDED_NAMES or "glad" in f.parts:
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

    # Wyznacz offsety początków linii w oryginalnym tekście
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

            # Wystąpienie w aktywnym kodzie C++ — sprawdź wzorce wykluczające
            prefix_on_line = mline[: match.end()]
            if DELETED_FN_RE.search(prefix_on_line) and kw.startswith("delete"):
                excluded.append((idx, kw, "usunięta funkcja składowa (`= delete`)", snippet))
                continue

            if OPERATOR_NEW_DELETE_RE.search(mline):
                excluded.append((idx, kw, "deklaracja `operator new/delete`", snippet))
                continue

            if kw.startswith("new") and UNIQUE_PTR_NEW_RE.search(mline):
                excluded.append(
                    (
                        idx,
                        kw,
                        "bezpośrednia inicjalizacja `std::unique_ptr<T>(new ...)` (RAII, brak surowego posiadania)",
                        snippet,
                    )
                )
                continue

            active.append((idx, kw, snippet))

    return active, excluded


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
    files = collect_files(target_paths)

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
    total_grep_matches = len(all_active) + len(all_excluded)

    args.report.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "# Check: `raw_new_delete`",
        "",
        f"- Przeskanowane pliki: **{len(files)}**",
        f"- Wszystkie dopasowania `grep -rn \"\\b(new|delete)\\b\" include/ src/`: **{total_grep_matches}**",
        f"- Aktywne wystąpienia surowego `new` / `delete` w kodzie (backlog Fazy 3.3): **{len(all_active)}**",
        f"- Nowe / nieoczekiwane wystąpienia poza backlogiem Fazy 3.3: **{len(unexpected_active)}**",
        f"- Wykluczone wystąpienia łącznie: **{len(all_excluded)}**",
        "",
        "## Podsumowanie wykluczonych wystąpień według powodu",
        "",
        "| Powód wykluczenia | Liczba wystąpień |",
        "|-------------------|-----------------:|",
    ]

    for reason, count in sorted(reason_counts.items()):
        lines.append(f"| {reason} | {count} |")
    lines.append(f"| **Razem wykluczonych** | **{len(all_excluded)}** |")
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

    lines.append("## 2. Wykluczone dopasowania z `grep` (w tym `std::unique_ptr`, komentarze, napisy i `= delete`)")
    lines.append("")
    if all_excluded:
        lines.append("| Plik | Linia | Słowo | Powód wykluczenia | Fragment linii |")
        lines.append("|------|------:|-------|-------------------|----------------|")
        for f, line_no, kw, reason, snippet in all_excluded:
            rel = f.resolve().relative_to(REPO_ROOT) if f.resolve().is_relative_to(REPO_ROOT) else f
            safe_snippet = snippet.replace("|", "\\|")
            lines.append(f"| `{rel.as_posix()}` | {line_no} | `{kw}` | {reason} | `{safe_snippet}` |")
        lines.append("")

    args.report.write_text("\n".join(lines), encoding="utf-8")
    return 1 if unexpected_active else 0


if __name__ == "__main__":
    sys.exit(main())
