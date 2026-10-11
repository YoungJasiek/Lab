#!/usr/bin/env python3
"""Wykrywa (i opcjonalnie usuwa z flagą --fix) białe znaki na końcu linii oraz sprawdza końcowy znak nowej linii w plikach źródłowych."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPORT = REPO_ROOT / "reports" / "checks-trailing_whitespace.md"

EXCLUDED_NAMES = {"stb_truetype.h", "miniaudio.h", "gl.c", "gl.h", "khrplatform.h", "onelua.c"}


def collect_files(paths: list[Path]) -> list[Path]:
    files: list[Path] = []
    for p in paths:
        if p.is_file():
            files.append(p)
        elif p.is_dir():
            for ext in ("*.cpp", "*.h", "*.hpp"):
                for f in sorted(p.rglob(ext)):
                    if f.name in EXCLUDED_NAMES or "glad" in f.parts:
                        continue
                    files.append(f)
    return sorted(set(files))


def check_and_optionally_fix(path: Path, fix: bool) -> tuple[list[int], bool, bool]:
    raw = path.read_bytes()
    if not raw:
        return [], False, False

    newline_bytes = b"\r\n" if b"\r\n" in raw else b"\n"
    text = raw.decode("utf-8", errors="replace")
    normalized = text.replace("\r\n", "\n")

    missing_eof_newline = not normalized.endswith("\n")
    raw_lines = normalized.split("\n")
    if not missing_eof_newline:
        raw_lines = raw_lines[:-1]

    trailing_ws_lines: list[int] = []
    cleaned_lines: list[str] = []
    for idx, line in enumerate(raw_lines, start=1):
        stripped = line.rstrip(" \t")
        if len(stripped) != len(line):
            trailing_ws_lines.append(idx)
        cleaned_lines.append(stripped)

    has_issues = bool(trailing_ws_lines) or missing_eof_newline
    was_fixed = False

    if has_issues and fix:
        sep = newline_bytes.decode("ascii")
        new_text = sep.join(cleaned_lines) + sep
        path.write_bytes(new_text.encode("utf-8"))
        was_fixed = True

    return trailing_ws_lines, missing_eof_newline, was_fixed


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
        help="Usuń białe znaki na końcu linii i dodaj brakujący znak nowej linii na końcu pliku",
    )
    parser.add_argument(
        "--report",
        type=Path,
        default=DEFAULT_REPORT,
        help="Ścieżka do pliku raportu Markdown",
    )
    args = parser.parse_args(argv)

    target_paths = args.paths if args.paths else [REPO_ROOT / "include", REPO_ROOT / "src"]
    files = collect_files(target_paths)

    unfixed_issues: list[tuple[Path, list[int], bool]] = []
    fixed_files: list[tuple[Path, int, bool]] = []

    for f in files:
        tws_lines, missing_eof, was_fixed = check_and_optionally_fix(f, args.fix)
        if tws_lines or missing_eof:
            if was_fixed:
                fixed_files.append((f, len(tws_lines), missing_eof))
            else:
                unfixed_issues.append((f, tws_lines, missing_eof))

    args.report.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "# Check: `trailing_whitespace`",
        "",
        f"- Przeskanowane pliki: **{len(files)}**",
        f"- Tryb: **{'--fix' if args.fix else 'report-only'}**",
        f"- Pliki z problemami (nienaprawione): **{len(unfixed_issues)}**",
        f"- Naprawione pliki: **{len(fixed_files)}**",
        "",
    ]

    if unfixed_issues:
        lines.append("| Plik | Liczba linii z trailing whitespace | Przykładowe linie | Brak końcowego `\\n` |")
        lines.append("|------|-----------------------------------:|-------------------|----------------------|")
        for f, tws, missing_eof in unfixed_issues:
            rel = f.resolve().relative_to(REPO_ROOT) if f.resolve().is_relative_to(REPO_ROOT) else f
            sample = ", ".join(str(n) for n in tws[:8]) + ("..." if len(tws) > 8 else "")
            eof_str = "TAK" if missing_eof else "nie"
            lines.append(f"| `{rel.as_posix()}` | {len(tws)} | {sample or '-'} | {eof_str} |")
            print(f"[WARN] {rel.as_posix()}: {len(tws)} linii z trailing whitespace, brak EOF newline={missing_eof}")
        lines.append("")
    elif fixed_files:
        lines.append("## Naprawione pliki")
        lines.append("")
        for f, count, missing_eof in fixed_files:
            rel = f.resolve().relative_to(REPO_ROOT) if f.resolve().is_relative_to(REPO_ROOT) else f
            lines.append(f"- `{rel.as_posix()}` (oczyszczone linie: {count}, dodano EOF newline: {missing_eof})")
            print(f"[FIXED] {rel.as_posix()}: oczyszczono {count} linii, EOF newline={missing_eof}")
        lines.append("")
    else:
        lines.append("Brak białych znaków na końcach linii oraz wszystkie pliki kończą się znakiem nowej linii.")
        lines.append("")
        print(f"[OK] Wszystkie pliki ({len(files)}) są wolne od trailing whitespace.")

    args.report.write_text("\n".join(lines), encoding="utf-8")
    return 1 if unfixed_issues else 0


if __name__ == "__main__":
    sys.exit(main())
