#!/usr/bin/env python3
"""Wypisuje wszystkie wystąpienia surowego new i delete poza komentarzami i literałami znakowymi, z plikiem i numerem linii (tylko raport)."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPORT = REPO_ROOT / "reports" / "checks-raw_new_delete.md"

EXCLUDED_NAMES = {"stb_truetype.h", "miniaudio.h", "gl.c", "gl.h", "khrplatform.h", "onelua.c"}

TOKEN_SPEC = re.compile(
    r"//[^\n]*|/\*.*?\*/|\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*'",
    re.DOTALL,
)
NEW_DELETE_RE = re.compile(
    r"(?<![A-Za-z0-9_~])(?:operator\s+(?:new|delete)(?:\s*\[\s*\])?|=\s*delete\b|(new|delete)(?:\s*\[\s*\])?)\b"
)


def mask_comments_and_strings(text: str) -> str:
    """Zastępuje komentarze i literały spacjami, zachowując znaki nowej linii i numery wierszy."""
    def repl(match: re.Match[str]) -> str:
        s = match.group(0)
        return "".join("\n" if ch == "\n" else " " for ch in s)

    return TOKEN_SPEC.sub(repl, text)


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


def scan_file(path: Path) -> list[tuple[int, str, str]]:
    content = path.read_text(encoding="utf-8", errors="replace")
    masked = mask_comments_and_strings(content)
    orig_lines = content.splitlines()
    masked_lines = masked.splitlines()

    findings: list[tuple[int, str, str]] = []
    for idx, mline in enumerate(masked_lines, start=1):
        for match in NEW_DELETE_RE.finditer(mline):
            kw = match.group(1)
            if not kw:
                continue  # pomijamy deklaracje operator new / operator delete
            snippet = orig_lines[idx - 1].strip() if idx - 1 < len(orig_lines) else ""
            findings.append((idx, kw, snippet))
    return findings


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
        help="Flaga akceptowana dla spójności CLI (skrypt nigdy nie modyfikuje kodu, bo wybór właściciela wymaga decyzji człowieka)",
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

    all_findings: list[tuple[Path, int, str, str]] = []
    for f in files:
        for line_no, kw, snippet in scan_file(f):
            all_findings.append((f, line_no, kw, snippet))

    args.report.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "# Check: `raw_new_delete`",
        "",
        f"- Przeskanowane pliki: **{len(files)}**",
        f"- Wykryte wystąpienia `new` / `delete`: **{len(all_findings)}**",
        "",
    ]

    if all_findings:
        lines.append("| Plik | Linia | Słowo kluczowe | Fragment kodu |")
        lines.append("|------|------:|----------------|---------------|")
        for f, line_no, kw, snippet in all_findings:
            rel = f.resolve().relative_to(REPO_ROOT) if f.resolve().is_relative_to(REPO_ROOT) else f
            safe_snippet = snippet.replace("|", "\\|")
            lines.append(f"| `{rel.as_posix()}` | {line_no} | `{kw}` | `{safe_snippet}` |")
            print(f"[WARN] {rel.as_posix()}:{line_no}: surowe '{kw}' -> {snippet}")
        lines.append("")
    else:
        lines.append("Brak wystąpień surowego `new` / `delete`.")
        lines.append("")
        print("[OK] Nie znaleziono surowego new/delete.")

    args.report.write_text("\n".join(lines), encoding="utf-8")
    return 1 if all_findings else 0


if __name__ == "__main__":
    sys.exit(main())
