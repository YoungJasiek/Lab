#!/usr/bin/env python3
"""Wykrywa dyrektywy #include z nieistniejącym plikiem oraz nagłówki bibliotek zewnętrznych/systemowych podane w cudzysłowie zamiast w nawiasach kątowych."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPORT = REPO_ROOT / "reports" / "checks-includes.md"

EXCLUDED_NAMES = {"stb_truetype.h", "miniaudio.h", "gl.c", "gl.h", "khrplatform.h", "onelua.c"}

EXTERNAL_HEADERS = {
    # Standard C/C++ headers
    "algorithm", "array", "atomic", "cassert", "cctype", "cerrno", "cfenv",
    "cfloat", "charconv", "chrono", "cinttypes", "climits", "clocale", "cmath",
    "complex", "concepts", "condition_variable", "coroutine", "csetjmp",
    "csignal", "cstdarg", "cstddef", "cstdint", "cstdio", "cstdlib", "cstring",
    "ctime", "cuchar", "cwchar", "cwctype", "deque", "exception", "execution",
    "filesystem", "format", "forward_list", "fstream", "functional", "future",
    "initializer_list", "iomanip", "ios", "iosfwd", "iostream", "istream",
    "iterator", "latch", "limits", "list", "locale", "map", "memory",
    "memory_resource", "mutex", "new", "numbers", "numeric", "optional",
    "ostream", "queue", "random", "ranges", "ratio", "regex", "scoped_allocator",
    "semaphore", "set", "shared_mutex", "source_location", "span", "sstream",
    "stack", "stdexcept", "stop_token", "streambuf", "string", "string_view",
    "syncstream", "system_error", "thread", "tuple", "type_traits", "typeindex",
    "typeinfo", "unordered_map", "unordered_set", "utility", "valarray",
    "variant", "vector", "version",
    "assert.h", "ctype.h", "errno.h", "float.h", "limits.h", "locale.h",
    "math.h", "setjmp.h", "signal.h", "stdarg.h", "stddef.h", "stdint.h",
    "stdio.h", "stdlib.h", "string.h", "time.h",
    # External / OS libraries
    "GLFW/glfw3.h", "GLFW/glfw3native.h",
    "glad/gl.h", "KHR/khrplatform.h",
    "GL/gl.h", "GL/glu.h",
    "lua.h", "lualib.h", "lauxlib.h", "lua.hpp",
    "windows.h", "winsock2.h", "ws2tcpip.h", "wininet.h", "commdlg.h",
    "shlobj.h", "shellapi.h", "mmsystem.h",
    "sys/types.h", "sys/socket.h", "sys/ioctl.h", "sys/stat.h",
    "netinet/in.h", "arpa/inet.h", "netdb.h", "unistd.h", "fcntl.h", "dlfcn.h",
}

INCLUDE_RE = re.compile(r"^(\s*#\s*include\s*)([\"<])([^\"<>]+)([\">])(.*)$")


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


def resolve_local_include(source_file: Path, included: str, search_dirs: list[Path]) -> bool:
    candidate_rel = source_file.parent / included
    if candidate_rel.exists():
        return True
    for d in search_dirs:
        if (d / included).exists():
            return True
    return False


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
        help="Zamień cudzysłowy na nawiasy kątowe dla nagłówków bibliotek zewnętrznych/systemowych",
    )
    parser.add_argument(
        "--report",
        type=Path,
        default=DEFAULT_REPORT,
        help="Ścieżka do pliku raportu Markdown",
    )
    args = parser.parse_args(argv)

    target_paths = args.paths if args.paths else [REPO_ROOT / "include", REPO_ROOT / "src"]
    search_dirs = [
        REPO_ROOT / "include",
        REPO_ROOT / "src" / "glad" / "include",
        REPO_ROOT / "external" / "lua",
        REPO_ROOT,
    ]
    files = collect_files(target_paths)

    missing_includes: list[tuple[Path, int, str]] = []
    quoted_external: list[tuple[Path, int, str]] = []
    fixed_external: list[tuple[Path, int, str]] = []

    for f in files:
        raw_bytes = f.read_bytes()
        newline = "\r\n" if b"\r\n" in raw_bytes else "\n"
        text = raw_bytes.decode("utf-8", errors="replace")
        orig_lines = text.splitlines()
        new_lines: list[str] = []
        modified = False

        for idx, line in enumerate(orig_lines, start=1):
            m = INCLUDE_RE.match(line)
            if not m:
                new_lines.append(line)
                continue

            prefix, open_delim, header_name, _close_delim, suffix = m.groups()
            if open_delim == '"':
                if header_name in EXTERNAL_HEADERS:
                    if args.fix:
                        new_lines.append(f"{prefix}<{header_name}>{suffix}")
                        modified = True
                        fixed_external.append((f, idx, header_name))
                        continue
                    else:
                        quoted_external.append((f, idx, header_name))
                elif not resolve_local_include(f, header_name, search_dirs):
                    missing_includes.append((f, idx, header_name))

            new_lines.append(line)

        if modified:
            updated = newline.join(new_lines) + (newline if text.endswith(("\n", "\r\n")) else "")
            f.write_bytes(updated.encode("utf-8"))

    args.report.parent.mkdir(parents=True, exist_ok=True)
    total_issues = len(missing_includes) + len(quoted_external)
    lines = [
        "# Check: `includes`",
        "",
        f"- Przeskanowane pliki: **{len(files)}**",
        f"- Tryb: **{'--fix' if args.fix else 'report-only'}**",
        f"- Nieistniejące pliki `#include`: **{len(missing_includes)}**",
        f"- Nagłówki zewnętrzne w cudzysłowie `\"...\"`: **{len(quoted_external)}**",
        f"- Naprawione nagłówki zewnętrzne: **{len(fixed_external)}**",
        "",
    ]

    if missing_includes:
        lines.append("## Nieistniejące pliki w `#include`")
        lines.append("")
        for f, idx, hdr in missing_includes:
            rel = f.resolve().relative_to(REPO_ROOT) if f.resolve().is_relative_to(REPO_ROOT) else f
            lines.append(f"- `{rel.as_posix()}:{idx}` — brak pliku `{hdr}`")
            print(f"[FAIL] {rel.as_posix()}:{idx}: nieistniejący plik #include \"{hdr}\"")
        lines.append("")

    if quoted_external:
        lines.append("## Nagłówki zewnętrzne/systemowe w cudzysłowie zamiast `<...>`")
        lines.append("")
        for f, idx, hdr in quoted_external:
            rel = f.resolve().relative_to(REPO_ROOT) if f.resolve().is_relative_to(REPO_ROOT) else f
            lines.append(f"- `{rel.as_posix()}:{idx}` — `#include \"{hdr}\"` powinno być `#include <{hdr}>`")
            print(f"[WARN] {rel.as_posix()}:{idx}: zewnętrzny nagłówek w cudzysłowie: \"{hdr}\"")
        lines.append("")

    if fixed_external:
        lines.append("## Naprawione dyrektywy `#include`")
        lines.append("")
        for f, idx, hdr in fixed_external:
            rel = f.resolve().relative_to(REPO_ROOT) if f.resolve().is_relative_to(REPO_ROOT) else f
            lines.append(f"- `{rel.as_posix()}:{idx}` — zamieniono na `#include <{hdr}>`")
            print(f"[FIXED] {rel.as_posix()}:{idx}: zamieniono na #include <{hdr}>")
        lines.append("")

    if total_issues == 0 and not fixed_external:
        lines.append("Wszystkie dyrektywy `#include` są poprawne.")
        lines.append("")
        print(f"[OK] Wszystkie dyrektywy #include ({len(files)} plików) są poprawne.")

    args.report.write_text("\n".join(lines), encoding="utf-8")
    return 1 if total_issues > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
