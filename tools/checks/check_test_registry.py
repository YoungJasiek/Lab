#!/usr/bin/env python3
"""Porównuje liczbę zarejestrowanych testów [Test N] w src/test_verify.cpp oraz liczbę realnie wykonanych testów (PASS z kroku 1) z liczbą testów zapisaną w README.md (w tym w badge) i opcjonalnie aktualizuje README.md z flagą --fix."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPORT = REPO_ROOT / "reports" / "checks-test_registry.md"
DEFAULT_TEST_LOG = REPO_ROOT / "build" / "test_verify_output.log"

TEST_NUMBERED_RE = re.compile(r"\[Test\s+(\d+)\]")
TEST_SUMMARY_RE = re.compile(r"\[TestSummary\]\s+PASS=(\d+)\s+SKIP=(\d+)\s+FAIL=(\d+)\s+TOTAL=(\d+)")

# Wzorce dopasowujące zapis liczby testów w README.md (z wyłączeniem „validating N engine subsystems”)
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
        re.compile(r"(#\s*)(\d+)(-Test\s+automated)"),
        r"\g<1>{n}\g<3>",
    ),
    (
        re.compile(r"(\()(\d+)(\s+verification\s+tests\))"),
        r"\g<1>{n}\g<3>",
    ),
]


def count_registered_tests(test_file: Path) -> int:
    """Kanoniczna definicja testu: jeden blok [Test N] w src/test_verify.cpp."""
    text = test_file.read_text(encoding="utf-8", errors="replace")
    matches = [int(m.group(1)) for m in TEST_NUMBERED_RE.finditer(text)]
    if not matches:
        return 0
    return max(len(set(matches)), max(matches))


def parse_executed_tests_from_log(log_file: Path) -> tuple[int, int, int] | None:
    """Zwraca (PASS, SKIP, FAIL) z wyniku wykonania TestVerify (krok 1), jeśli plik logu istnieje."""
    if not log_file.is_file():
        return None
    raw = log_file.read_bytes()
    if raw.startswith((b"\xff\xfe", b"\xfe\xff")):
        text = raw.decode("utf-16", errors="replace")
    else:
        text = raw.decode("utf-8", errors="replace").replace("\x00", "")
    m = TEST_SUMMARY_RE.search(text)
    if m:
        return int(m.group(1)), int(m.group(2)), int(m.group(3))
    executed = len(TEST_NUMBERED_RE.findall(text))
    if executed > 0 and "All automated and visual verifications passed" in text:
        return executed, 0, 0
    return None


def get_executed_test_count(test_file: Path, log_file: Path = DEFAULT_TEST_LOG) -> int:
    """Zwraca liczbę wykonanych testów z kroku 1 (lub liczbę bloków [Test N], jeśli log nie jest podany)."""
    parsed = parse_executed_tests_from_log(log_file)
    if parsed is not None:
        return parsed[0]
    return count_registered_tests(test_file)


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
        "--test-file",
        type=Path,
        default=REPO_ROOT / "src" / "test_verify.cpp",
        help="Ścieżka do pliku z testami (domyślnie: src/test_verify.cpp)",
    )
    parser.add_argument(
        "--test-log",
        type=Path,
        default=DEFAULT_TEST_LOG,
        help="Ścieżka do logu z wykonania TestVerify z kroku 1 (domyślnie: build/test_verify_output.log)",
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
        help="Zaktualizuj liczbę testów w README.md (w tym badge) na podstawie liczby testów",
    )
    parser.add_argument(
        "--report",
        type=Path,
        default=DEFAULT_REPORT,
        help="Ścieżka do pliku raportu Markdown",
    )
    args = parser.parse_args(argv)

    registered_count = count_registered_tests(args.test_file)
    log_summary = parse_executed_tests_from_log(args.test_log)
    executed_count = log_summary[0] if log_summary is not None else registered_count
    skip_count = log_summary[1] if log_summary is not None else 0
    fail_count = log_summary[2] if log_summary is not None else 0

    raw_lines = args.readme.read_bytes().splitlines(keepends=True)

    mismatches: list[tuple[int, int, str]] = []
    fixed_lines: list[tuple[int, int, int]] = []
    updated_chunks: list[bytes] = []
    modified = False

    for idx, raw_line in enumerate(raw_lines, start=1):
        body_bytes, eol_bytes = split_eol(raw_line)
        line_str = body_bytes.decode("utf-8", errors="replace")
        new_line_str = line_str
        line_mismatch_nums: list[int] = []

        for pat, repl_template in README_RULES:
            for m in pat.finditer(new_line_str):
                doc_num = int(m.group(2))
                if doc_num != executed_count:
                    line_mismatch_nums.append(doc_num)
            if args.fix:
                replaced = pat.sub(repl_template.format(n=executed_count), new_line_str)
                if replaced != new_line_str:
                    new_line_str = replaced
                    modified = True

        if line_mismatch_nums:
            if args.fix:
                fixed_lines.append((idx, line_mismatch_nums[0], executed_count))
            else:
                mismatches.append((idx, line_mismatch_nums[0], line_str.strip()))

        updated_chunks.append(new_line_str.encode("utf-8") + eol_bytes)

    if modified and args.fix:
        args.readme.write_bytes(b"".join(updated_chunks))

    execution_mismatch = (
        log_summary is not None
        and (executed_count != registered_count or skip_count > 0 or fail_count > 0)
    )

    args.report.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "# Check: `test_registry`",
        "",
        f"- Plik testowy: `{args.test_file.name}`",
        f"- Kanoniczna definicja testu: pojedynczy blok `[Test N]` w `{args.test_file.name}`",
        f"- Liczba zarejestrowanych bloków `[Test N]`: **{registered_count}**",
        f"- Liczba wykonanych testów (krok 1): **{executed_count}**"
        + (f" (`PASS={executed_count}, SKIP={skip_count}, FAIL={fail_count}` z `{args.test_log.name}`)" if log_summary else " (na podstawie rejestru `[Test N]`)"),
        f"- Plik dokumentacji: `{args.readme.name}` (z wyłączeniem opisu architektury `validating N engine subsystems`)",
        f"- Tryb: **{'--fix' if args.fix else 'report-only'}**",
        f"- Nienaprawione rozbieżności w `{args.readme.name}`: **{len(mismatches)}**",
        f"- Zaktualizowane linie w `{args.readme.name}`: **{len(fixed_lines)}**",
        "",
    ]

    if execution_mismatch:
        lines.append(
            f"[ERROR] Rozbieżność wykonania testów z kroku 1: PASS={executed_count}, SKIP={skip_count}, FAIL={fail_count}, zarejestrowano={registered_count}"
        )
        lines.append("")
        print(
            f"[ERROR] Wykonane testy z kroku 1 (PASS={executed_count}, SKIP={skip_count}, FAIL={fail_count}) != zarejestrowane [Test N] ({registered_count})"
        )

    if mismatches:
        lines.append("| Linia w README.md | Zapisana liczba | Liczba wykonanych testów | Treść linii |")
        lines.append("|------------------:|----------------:|-------------------------:|-------------|")
        for idx, doc_num, snippet in mismatches:
            safe_snippet = snippet.replace("|", "\\|")
            lines.append(f"| {idx} | {doc_num} | {executed_count} | `{safe_snippet}` |")
            print(
                f"[WARN] README.md:{idx}: zapisano {doc_num} testów, a wykonano/zarejestrowano {executed_count}"
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
        lines.append(
            f"Liczba testów w `{args.readme.name}` (**{executed_count}**) zgadza się z liczbą wykonanych testów (`[Test 1]`..`[Test {executed_count}]`)."
        )
        lines.append("")
        print(f"[OK] Wykonane testy: {executed_count} (zgodne z {args.readme.name} i {args.test_file.name}).")

    args.report.write_text("\n".join(lines), encoding="utf-8")
    return 1 if (mismatches or execution_mismatch) else 0


if __name__ == "__main__":
    sys.exit(main())
