#!/usr/bin/env python3
"""Sprawdza, czy każdy plik nagłówkowy w katalogu include/ posiada #pragma once lub klasyczny strażnik include (#ifndef/#define)."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPORT = REPO_ROOT / "reports" / "checks-pragma_once.md"

PRAGMA_ONCE_RE = re.compile(r"^\s*#\s*pragma\s+once\b", re.MULTILINE)
IFNDEF_GUARD_RE = re.compile(
    r"^\s*#\s*ifndef\s+([A-Za-z_][A-Za-z0-9_]*)\s*\r?\n\s*#\s*define\s+\1\b",
    re.MULTILINE,
)


def strip_comments(text: str) -> str:
    """Usuwa komentarze blokowe i liniowe na potrzeby detekcji dyrektyw preprocesora."""
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//[^\n]*", "", text)
    return text


def has_include_guard(content: str) -> bool:
    cleaned = strip_comments(content)
    if PRAGMA_ONCE_RE.search(cleaned):
        return True
    if IFNDEF_GUARD_RE.search(cleaned):
        return True
    return False


def collect_headers(paths: list[Path]) -> list[Path]:
    headers: list[Path] = []
    for p in paths:
        if p.is_file() and p.suffix.lower() in {".h", ".hpp"}:
            headers.append(p)
        elif p.is_dir():
            for ext in ("*.h", "*.hpp"):
                headers.extend(sorted(p.rglob(ext)))
    return sorted(set(headers))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "paths",
        nargs="*",
        type=Path,
        help="Ścieżki do plików lub katalogów (domyślnie: include/)",
    )
    parser.add_argument(
        "--fix",
        action="store_true",
        help="Dodaj brakujące #pragma once na początku pliku nagłówkowego",
    )
    parser.add_argument(
        "--report",
        type=Path,
        default=DEFAULT_REPORT,
        help="Ścieżka do pliku raportu Markdown",
    )
    args = parser.parse_args(argv)

    target_paths = args.paths if args.paths else [REPO_ROOT / "include"]
    headers = collect_headers(target_paths)

    missing: list[Path] = []
    fixed: list[Path] = []

    for header in headers:
        raw_bytes = header.read_bytes()
        content = raw_bytes.decode("utf-8", errors="replace")
        if not has_include_guard(content):
            if args.fix:
                newline = "\r\n" if b"\r\n" in raw_bytes else "\n"
                updated = f"#pragma once{newline}" + content
                header.write_bytes(updated.encode("utf-8"))
                fixed.append(header)
            else:
                missing.append(header)

    args.report.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "# Check: `pragma_once`",
        "",
        f"- Przeskanowane nagłówki: **{len(headers)}**",
        f"- Tryb: **{'--fix' if args.fix else 'report-only'}**",
        f"- Brak strażnika include: **{len(missing)}**",
        f"- Naprawione pliki: **{len(fixed)}**",
        "",
    ]

    if missing:
        lines.append("## Pliki bez `#pragma once` / strażnika include")
        lines.append("")
        for p in missing:
            rel = p.resolve().relative_to(REPO_ROOT) if p.resolve().is_relative_to(REPO_ROOT) else p
            lines.append(f"- `{rel.as_posix()}`")
            print(f"[FAIL] Brak #pragma once lub strażnika include: {rel.as_posix()}")
        lines.append("")
    elif fixed:
        lines.append("## Naprawione pliki")
        lines.append("")
        for p in fixed:
            rel = p.resolve().relative_to(REPO_ROOT) if p.resolve().is_relative_to(REPO_ROOT) else p
            lines.append(f"- `{rel.as_posix()}`")
            print(f"[FIXED] Dodano #pragma once: {rel.as_posix()}")
        lines.append("")
    else:
        lines.append("Wszystkie nagłówki posiadają `#pragma once` lub strażnik include.")
        lines.append("")
        print(f"[OK] Wszystkie nagłówki ({len(headers)}) posiadają #pragma once lub strażnik include.")

    args.report.write_text("\n".join(lines), encoding="utf-8")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
