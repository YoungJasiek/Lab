"""
run_sdk.py - Oficjalne narzedzie uruchomieniowe i zarzadzajace dla Frozen-Life SDK.
Pozwala na wygodne budowanie, testowanie oraz uruchamianie gry i narzedzi autorskich.
"""

import sys
import os
import argparse
import subprocess
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent
BUILD_DIR = ROOT_DIR / "build" / "Release"

TOOLS = {
    "game": BUILD_DIR / "FrozenLife.exe",
    "editor": BUILD_DIR / "LabEditor.exe",
    "studio": BUILD_DIR / "LabStudio.exe",
    "server": BUILD_DIR / "LabServer.exe",
    "player": BUILD_DIR / "LabPlayer.exe",
    "verify": ROOT_DIR / "scripts" / "verify_pipeline.py"
}

def build_project(config="Release"):
    print(f"[SDK BUILD] Kompilacja rozwiazania w trybie {config}...")
    cmd = ["cmake", "--build", "build", "--config", config, "--parallel"]
    res = subprocess.run(cmd, cwd=str(ROOT_DIR))
    if res.returncode != 0:
        print("[SDK BUILD] [BLAD] Kompilacja nie powiodla sie!")
        return False
    print("[SDK BUILD] Kompilacja zakonczona sukcesem.")
    return True

def run_target(name, extra_args=None):
    if name not in TOOLS:
        print(f"[SDK] Nieznany cel: {name}")
        return False
        
    target_path = TOOLS[name]
    if name == "verify":
        cmd = [sys.executable, str(target_path)]
        if extra_args:
            cmd.extend(extra_args)
        return subprocess.run(cmd, cwd=str(ROOT_DIR)).returncode == 0

    if not target_path.exists():
        print(f"[SDK] Plik wykonywalny nie istnieje: {target_path}")
        print("Sprobuj najpierw skompilowac projekt: python scripts/run_sdk.py --build")
        return False

    cmd = [str(target_path)]
    if extra_args:
        cmd.extend(extra_args)
        
    print(f"[SDK LAUNCH] Uruchamianie: {target_path.name} {' '.join(extra_args or [])}")
    return subprocess.run(cmd, cwd=str(ROOT_DIR)).returncode == 0

def main():
    parser = argparse.ArgumentParser(description="Frozen-Life SDK CLI Manager")
    parser.add_argument("--build", action="store_true", help="Skompiluj projekt (Release)")
    parser.add_argument("--game", action="store_true", help="Uruchom gre Frozen-Life (FrozenLife.exe)")
    parser.add_argument("--editor", action="store_true", help="Uruchom edytor poziomow (LabEditor.exe)")
    parser.add_argument("--studio", action="store_true", help="Uruchom podglad modeli i animacji (LabStudio.exe)")
    parser.add_argument("--server", action="store_true", help="Uruchom serwer dedykowany (LabServer.exe)")
    parser.add_argument("--player", action="store_true", help="Uruchom odtwarzacz paczek (LabPlayer.exe)")
    parser.add_argument("--verify", action="store_true", help="Uruchom zestaw weryfikacyjny i wyeksportuj klatki")
    parser.add_argument("extra", nargs="*", help="Dodatkowe argumenty przekazywane do programu")

    args = parser.parse_args()

    if len(sys.argv) == 1:
        parser.print_help()
        sys.exit(0)

    if args.build:
        if not build_project():
            sys.exit(1)

    if args.verify:
        if not run_target("verify", args.extra):
            sys.exit(1)

    if args.game:
        run_target("game", args.extra)
    elif args.editor:
        run_target("editor", args.extra)
    elif args.studio:
        run_target("studio", args.extra)
    elif args.server:
        run_target("server", args.extra)
    elif args.player:
        run_target("player", args.extra)

if __name__ == "__main__":
    main()
