#!/usr/bin/env python3
"""Porównuje liczbę zarejestrowanych testów [Test N] w src/test_verify.cpp z liczbą testów zapisaną w README.md (w tym w badge) i opcjonalnie aktualizuje README.md z flagą --fix."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPORT = REPO_ROOT / "reports" / "checks-test_registry.md"

TEST_NUMBERED_RE = re.compile(r"\[Test\s+(\d+)\]")

# Wzorce dopasowujące zapis liczby testów w README.md wraz z funkcją podmiany dla --fix
README_RULES: list[tuple[re.Pattern[str], str]] = [
    (
        re.compile(r"(Tests-)(\d+)(%2F)(\d+)(%20Passed)"),
        r"\g<1>{n}\g<3>{n}\g<5>",
    ),
    (
        re.compile(r"(alt=\"Tests:\s*)(\d+)(/)(\d+)(\s+Passed\")"),
        r"\g<1>{n}\g<3>{n}\g<5>",
    ),
    (
        re.compile(r"(\*\*)(\d+)(\s+distinct\s+tests\*\*)"),
        r"\g<1>{n}\g<3>",
    ),
    (
        re.compile(r"(validating\s+)(\d+)(\s+engine\s+subsystems)"),
        r"\g<1>{n}\g<3>",
    ),
    (
        re.compile(r"(#\s*)(\d+)(-Test\s+automated)"),
        r"\g<1>{n}\g<3>",
    ),
    (
        re.compile(r"(\()(\d+)(\s+verification\s+tests\))"),
        r"\g<1>{n}\g<3>",
    ),
]


def count_registered_tests(test_file: Path) -> int:
    """Kanoniczna definicja testu: każdy zestaw oznaczony [Test N] (1..N) w src/test_verify.cpp."""
    text = test_file.read_text(encoding="utf-8", errors="replace")
    numbers = {int(m.group(1)) for m in TEST_NUMBERED_RE.finditer(text)}
    if not numbers:
        return 0
    return max(len(numbers), max(numbers))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--test-file",
        type=Path,
        default=REPO_ROOT / "src" / "test_verify.cpp",
        help="Ścieżka do pliku z testami (domyślnie: src/test_verify.cpp)",
    )
    parser.add_argument(
        "--readme",
        type=Path,
        default=REPO_ROOT / "README.md",
        help="Ścieżka do pliku README.md",
    )
    parser.add_argument(
        "--fix",
        action="store_true",
        help="Zaktualizuj liczbę testów w README.md (w tym badge) na podstawie rejestru w src/test_verify.cpp",
    )
    parser.add_argument(
        "--report",
        type=Path,
        default=DEFAULT_REPORT,
        help="Ścieżka do pliku raportu Markdown",
    )
    args = parser.parse_args(argv)

    actual_count = count_registered_tests(args.test_file)
    raw_bytes = args.readme.read_bytes()
    newline = "\r\n" if b"\r\n" in raw_bytes else "\n"
    readme_text = raw_bytes.decode("utf-8", errors="replace")
    readme_lines = readme_text.splitlines()

    mismatches: list[tuple[int, int, str]] = []
    fixed_lines: list[tuple[int, int, int]] = []
    updated_lines: list[str] = []
    modified = False

    for idx, line in enumerate(readme_lines, start=1):
        new_line = line
        line_mismatch_nums: list[int] = []

        for pat, repl_template in README_RULES:
            for m in pat.finditer(new_line):
                doc_num = int(m.group(2))
                if doc_num != actual_count:
                    line_mismatch_nums.append(doc_num)
            if args.fix:
                replaced = pat.sub(repl_template.format(n=actual_count), new_line)
                if replaced != new_line:
                    new_line = replaced
                    modified = True

        if line_mismatch_nums:
            if args.fix:
                fixed_lines.append((idx, line_mismatch_nums[0], actual_count))
            else:
                mismatches.append((idx, line_mismatch_nums[0], line.strip()))

        updated_lines.append(new_line)

    if modified and args.fix:
        out_text = newline.join(updated_lines) + (newline if readme_text.endswith(("\n", "\r\n")) else "")
        args.readme.write_bytes(out_text.encode("utf-8"))

    args.report.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "# Check: `test_registry`",
        "",
        f"- Plik testowy: `{args.test_file.name}`",
        f"- Kanoniczna definicja testu: pojedynczy blok `[Test N]` w `{args.test_file.name}`",
        f"- Liczba zarejestrowanych testów: **{actual_count}**",
        f"- Plik dokumentacji: `{args.readme.name}`",
        f"- Tryb: **{'--fix' if args.fix else 'report-only'}**",
        f"- Nienaprawione rozbieżności w `{args.readme.name}`: **{len(mismatches)}**",
        f"- Zaktualizowane linie w `{args.readme.name}`: **{len(fixed_lines)}**",
        "",
    ]

    if mismatches:
        lines.append("| Linia w README.md | Zapisana liczba | Rzeczywista liczba | Treść linii |")
        lines.append("|------------------:|----------------:|-------------------:|-------------|")
        for idx, doc_num, snippet in mismatches:
            safe_snippet = snippet.replace("|", "\\|")
            lines.append(f"| {idx} | {doc_num} | {actual_count} | `{safe_snippet}` |")
            print(
                f"[WARN] README.md:{idx}: zapisano {doc_num} testów, a w {args.test_file.name} zarejestrowano {actual_count}"
            )
        lines.append("")
    elif fixed_lines:
        lines.append("## Zaktualizowane linie w `README.md`")
        lines.append("")
        for idx, old_num, new_num in fixed_lines:
            lines.append(f"- `README.md:{idx}`: `{old_num}` -> `{new_num}`")
            print(f"[FIXED] README.md:{idx}: zaktualizowano liczbę testów {old_num} -> {new_num}")
        lines.append("")
    else:
        lines.append(f"Liczba testów w `{args.readme.name}` (**{actual_count}**) zgadza się z rejestrem testów.")
        lines.append("")
        print(f"[OK] Liczba testów w README.md ({actual_count}) jest zgodna z {args.test_file.name}.")

    args.report.write_text("\n".join(lines), encoding="utf-8")
    return 1 if mismatches else 0


if __name__ == "__main__":
    sys.exit(main())
