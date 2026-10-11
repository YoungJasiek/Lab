#!/usr/bin/env python3
"""Wykrywa #include z nieistniejącym plikiem oraz nagłówki zewnętrzne podane w cudzysłowie zamiast w nawiasach kątowych (opcjonalnie naprawia z --fix, zachowując oryginalne końce linii)."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPORT = REPO_ROOT / "reports" / "checks-includes.md"

EXCLUDED_NAMES = {"stb_truetype.h", "stb_image.h", "miniaudio.h", "gl.c", "gl.h", "khrplatform.h", "onelua.c"}

EXTERNAL_HEADERS = {
    "lua.h",
    "lualib.h",
    "lauxlib.h",
    "luaconf.h",
    "GLFW/glfw3.h",
    "GLFW/glfw3native.h",
    "windows.h",
    "winsock2.h",
    "ws2tcpip.h",
    "commdlg.h",
    "shlobj.h",
}

INCLUDE_RE = re.compile(r"^(\s*#\s*include\s*)\"([^\"]+)\"(.*)$")


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


def resolve_quoted_include(source_file: Path, inc_target: str, include_dirs: list[Path]) -> bool:
    rel_candidate = (source_file.parent / inc_target).resolve()
    if rel_candidate.is_file():
        return True
    for inc_dir in include_dirs:
        if (inc_dir / inc_target).is_file():
            return True
    return False


def split_eol(line_bytes: bytes) -> tuple[bytes, bytes]:
    if line_bytes.endswith(b"\r\n"):
        return line_bytes[:-2], b"\r\n"
    if line_bytes.endswith(b"\n"):
        return line_bytes[:-1], b"\n"
    if line_bytes.endswith(b"\r"):
        return line_bytes[:-1], b"\r"
    return line_bytes, b""


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
        help="Zamień #include \"zewnetrzny.h\" na #include <zewnetrzny.h> dla znanych bibliotek zewnętrznych",
    )
    parser.add_argument(
        "--report",
        type=Path,
        default=DEFAULT_REPORT,
        help="Ścieżka do pliku raportu Markdown",
    )
    args = parser.parse_args(argv)

    target_paths = args.paths if args.paths else [REPO_ROOT / "include", REPO_ROOT / "src"]
    include_dirs = [
        REPO_ROOT / "include",
        REPO_ROOT / "src",
        REPO_ROOT / "games" / "frozen_life" / "include",
    ]
    files = collect_files(target_paths)

    missing_includes: list[tuple[Path, int, str]] = []
    quoted_external: list[tuple[Path, int, str]] = []
    fixed_entries: list[tuple[Path, int, str]] = []

    for f in files:
        raw_lines = f.read_bytes().splitlines(keepends=True)
        modified = False
        new_chunks: list[bytes] = []

        for idx, raw_line in enumerate(raw_lines, start=1):
            body_bytes, eol_bytes = split_eol(raw_line)
            line_str = body_bytes.decode("utf-8", errors="replace")
            m = INCLUDE_RE.match(line_str)
            if not m:
                new_chunks.append(raw_line)
                continue

            prefix, target, suffix = m.group(1), m.group(2), m.group(3)
            if target in EXTERNAL_HEADERS or target.startswith(("GLFW/", "GL/", "KHR/")):
                if args.fix:
                    new_line_str = f"{prefix}<{target}>{suffix}"
                    new_chunks.append(new_line_str.encode("utf-8") + eol_bytes)
                    modified = True
                    fixed_entries.append((f, idx, target))
                else:
                    quoted_external.append((f, idx, target))
                    new_chunks.append(raw_line)
                continue

            if not resolve_quoted_include(f, target, include_dirs):
                missing_includes.append((f, idx, target))

            new_chunks.append(raw_line)

        if modified and args.fix:
            f.write_bytes(b"".join(new_chunks))

    args.report.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "# Check: `includes`",
        "",
        f"- Przeskanowane pliki: **{len(files)}**",
        f"- Tryb: **{'--fix' if args.fix else 'report-only'}**",
        f"- Brakujące pliki `#include`: **{len(missing_includes)}**",
        f"- Nagłówki zewnętrzne w cudzysłowie (nienaprawione): **{len(quoted_external)}**",
        f"- Naprawione dyrektywy `#include`: **{len(fixed_entries)}**",
        "",
    ]

    if missing_includes:
        lines.append("## Brakujące pliki `#include`")
        lines.append("")
        lines.append("| Plik | Linia | Szukany nagłówek |")
        lines.append("|------|------:|------------------|")
        for f, idx, target in missing_includes:
            rel = f.resolve().relative_to(REPO_ROOT) if f.resolve().is_relative_to(REPO_ROOT) else f
            lines.append(f"| `{rel.as_posix()}` | {idx} | `\"{target}\"` |")
            print(f"[ERROR] {rel.as_posix()}:{idx}: nie znaleziono pliku #include \"{target}\"")
        lines.append("")

    if quoted_external:
        lines.append("## Nagłówki bibliotek zewnętrznych w cudzysłowie (zamiast `<...>`)")
        lines.append("")
        lines.append("| Plik | Linia | Nagłówek | Zalecana forma |")
        lines.append("|------|------:|----------|----------------|")
        for f, idx, target in quoted_external:
            rel = f.resolve().relative_to(REPO_ROOT) if f.resolve().is_relative_to(REPO_ROOT) else f
            lines.append(f"| `{rel.as_posix()}` | {idx} | `\"{target}\"` | `<{target}>` |")
            print(f"[WARN] {rel.as_posix()}:{idx}: zewnętrzny nagłówek \"{target}\" powinien używać <{target}>")
        lines.append("")

    if fixed_entries:
        lines.append("## Naprawione dyrektywy `#include`")
        lines.append("")
        for f, idx, target in fixed_entries:
            rel = f.resolve().relative_to(REPO_ROOT) if f.resolve().is_relative_to(REPO_ROOT) else f
            lines.append(f"- `{rel.as_posix()}:{idx}`: `\"{target}\"` -> `<{target}>`")
            print(f"[FIXED] {rel.as_posix()}:{idx}: zamieniono na #include <{target}>")
        lines.append("")

    if not missing_includes and not quoted_external and not fixed_entries:
        lines.append("Wszystkie dyrektywy `#include` wskazują na istniejące pliki i używają właściwej notacji.")
        lines.append("")
        print(f"[OK] Wszystkie dyrektywy #include ({len(files)} plików) są poprawne.")

    args.report.write_text("\n".join(lines), encoding="utf-8")
    return 1 if (missing_includes or quoted_external) else 0


if __name__ == "__main__":
    sys.exit(main())
