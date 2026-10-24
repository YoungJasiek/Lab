"""
verify_pipeline.py - Zautomatyzowany skrypt weryfikacji silnika Lab / Frozen-Life SDK.
Uruchamia TestVerify.exe, weryfikuje wyniki testow i konwertuje wygenerowane klatki OpenGL (.bmp) na format .png.
"""

import os
import sys
import subprocess
import glob
from pathlib import Path
from PIL import Image

ROOT_DIR = Path(__file__).resolve().parent.parent
BUILD_BIN = ROOT_DIR / "build" / "Release" / "TestVerify.exe"

def get_artifact_dir():
    if "ARTIFACT_DIR" in os.environ and os.environ["ARTIFACT_DIR"].strip():
        return Path(os.environ["ARTIFACT_DIR"].strip())
    
    brain_root = Path(r"C:\Users\jancz\.gemini\antigravity\brain")
    active_id = "bc517531-1833-4a47-9817-f51dfc11269b"
    active_path = brain_root / active_id
    if active_path.exists():
        return active_path
    
    if brain_root.exists():
        dirs = [d for d in brain_root.iterdir() if d.is_dir() and not d.name.startswith(".")]
        if dirs:
            dirs.sort(key=lambda d: d.stat().st_mtime, reverse=True)
            return dirs[0]
            
    return active_path

ARTIFACT_DIR = get_artifact_dir()

def run_tests():
    if not BUILD_BIN.exists():
        print(f"[BLAD] Nie znaleziono pliku wykonywalnego testow: {BUILD_BIN}")
        print("Najpierw uruchom: cmake --build build --config Release")
        return False, 0, 0, 0

    print(f"[TEST] Uruchamianie: {BUILD_BIN}")
    proc = subprocess.run([str(BUILD_BIN)], cwd=str(ROOT_DIR), capture_output=True, text=True, encoding="utf-8", errors="replace")
    
    output = proc.stdout + "\n" + proc.stderr
    print(f"[TEST] Kod wyjscia: {proc.returncode}")
    
    pass_count = 0
    fail_count = 0
    skip_count = 0
    
    for line in output.splitlines():
        if "[TestSummary]" in line:
            print(f"[PODSUMOWANIE] {line.strip()}")
            parts = line.strip().split()
            for part in parts:
                if part.startswith("PASS="):
                    pass_count = int(part.split("=")[1])
                elif part.startswith("FAIL="):
                    fail_count = int(part.split("=")[1])
                elif part.startswith("SKIP="):
                    skip_count = int(part.split("=")[1])

    if proc.returncode != 0 or fail_count > 0:
        print("[BLAD] Testy nie powiodly sie!")
        lines = output.splitlines()
        print("\n".join(lines[-40:]))
        return False, pass_count, fail_count, skip_count
        
    return True, pass_count, fail_count, skip_count

def convert_screenshots():
    print(f"[EXPORT] Konwersja wygenerowanych klatek testowych .bmp do .png w {ARTIFACT_DIR}...")
    ARTIFACT_DIR.mkdir(parents=True, exist_ok=True)
    web_img_dir = ROOT_DIR / "web" / "assets" / "img"
    if web_img_dir.exists():
        web_img_dir.mkdir(parents=True, exist_ok=True)
    
    bmp_files = list(ROOT_DIR.glob("test_*.bmp"))
    if not bmp_files:
        print("[OSTRZEZENIE] Nie znaleziono zadnych plikow test_*.bmp w glownym katalogu.")
        return 0

    exported = 0
    for bmp_path in bmp_files:
        try:
            im = Image.open(bmp_path)
            # Save standard name
            out_standard = ARTIFACT_DIR / f"{bmp_path.stem}.png"
            im.save(out_standard, format="PNG")
            
            # Save after_ name for backward compatibility
            out_after = ARTIFACT_DIR / f"after_{bmp_path.stem}.png"
            im.save(out_after, format="PNG")

            if web_img_dir.exists():
                web_out = web_img_dir / f"{bmp_path.stem}.png"
                im.save(web_out, format="PNG")
                
            exported += 1
        except Exception as e:
            print(f"[BLAD] Blad zapisu {bmp_path.name}: {e}")

    print(f"[EXPORT] Pomyslnie przekonwertowano {exported} klatek do {ARTIFACT_DIR}")
    return exported

def main():
    print("==================================================")
    print("   Lab / Frozen-Life SDK: Weryfikacja i Testy     ")
    print("==================================================")
    success, p, f, s = run_tests()
    if not success:
        sys.exit(1)
        
    exported = convert_screenshots()
    print("==================================================")
    print(f"[STATUS] WYNIK: {'SUKCES' if success else 'PORAZKA'}")
    print(f"[STATUS] Testy: PASS={p}, FAIL={f}, SKIP={s}")
    print(f"[STATUS] Klatki: {exported} zapisanych plikow PNG")
    print("==================================================")

if __name__ == "__main__":
    main()
