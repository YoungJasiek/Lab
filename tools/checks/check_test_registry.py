#!/usr/bin/env python3
"""Porównuje liczbę zarejestrowanych testów w src/test_verify.cpp z liczbą testów zapisaną w README.md i zgłasza rozbieżności."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPORT = REPO_ROOT / "reports" / "checks-test_registry.md"

TEST_NUMBERED_RE = re.compile(r"\[Test\s+(\d+)\]")
TEST_UNNUMBERED_HEADER_RE = re.compile(r'std::cout\s*<<\s*"\\?n?\[Test\]\s+(?:Verifying|Testing)\b')

README_PATTERNS = [
    re.compile(r"Tests-(\d+)%2F(\d+)"),
    re.compile(r"Tests:\s*(\d+)/(\d+)\s+Passed"),
    re.compile(r"\*\*(\d+)\s+distinct\s+tests\*\*"),
    re.compile(r"validating\s+(\d+)\s+engine\s+subsystems"),
    re.compile(r"(\d+)-Test\s+automated"),
    re.compile(r"\((\d+)\s+verification\s+tests\)"),
]


def count_registered_tests(test_file: Path) -> tuple[int, int, int]:
    text = test_file.read_text(encoding="utf-8", errors="replace")
    numbers = {int(m.group(1)) for m in TEST_NUMBERED_RE.finditer(text)}
    unnumbered = len(TEST_UNNUMBERED_HEADER_RE.findall(text))
    max_num = max(numbers) if numbers else 0
    total = max(max_num, len(numbers) + unnumbered)
    return total, len(numbers), max_num


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
        help="Flaga akceptowana dla spójności CLI (skrypt tylko raportuje rozbieżności)",
    )
    parser.add_argument(
        "--report",
        type=Path,
        default=DEFAULT_REPORT,
        help="Ścieżka do pliku raportu Markdown",
    )
    args = parser.parse_args(argv)

    actual_count, numbered_count, max_test_id = count_registered_tests(args.test_file)
    readme_lines = args.readme.read_text(encoding="utf-8", errors="replace").splitlines()

    mismatches: list[tuple[int, int, str]] = []

    for idx, line in enumerate(readme_lines, start=1):
        for pat in README_PATTERNS:
            m = pat.search(line)
            if m:
                documented = int(m.group(1))
                if documented != actual_count:
                    mismatches.append((idx, documented, line.strip()))
                break

    args.report.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "# Check: `test_registry`",
        "",
        f"- Plik testowy: `{args.test_file.name}`",
        f"- Rzeczywista liczba zestawów testowych (max `[Test N]`): **{actual_count}** (jawnie numerowanych `[Test N]`: {numbered_count}, najwyższy indeks: {max_test_id})",
        f"- Plik dokumentacji: `{args.readme.name}`",
        f"- Wykryte rozbieżności w `{args.readme.name}`: **{len(mismatches)}**",
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
    else:
        lines.append(f"Liczba testów w `{args.readme.name}` ({actual_count}) zgadza się z rejestrem testów.")
        lines.append("")
        print(f"[OK] Liczba testów w README.md ({actual_count}) jest zgodna z {args.test_file.name}.")

    args.report.write_text("\n".join(lines), encoding="utf-8")
    return 1 if mismatches else 0


if __name__ == "__main__":
    sys.exit(main())
