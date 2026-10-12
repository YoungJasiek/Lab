# Security Policy

## Supported Versions

Lab Engine is currently in active pre-release development (`0.1.x`). Only the latest commit on the `main` branch and the current `0.1.x` development line receive security fixes.

| Version | Supported          |
| ------- | ------------------ |
| 0.1.x   | :white_check_mark: |
| < 0.1.0 | :x:                |

## Scope

We are particularly interested in vulnerabilities affecting:

- **Dedicated Server & Networking (`LabNetwork`, `LabServer.exe`):** packet parsing, buffer overflows, remote code execution (RCE), or denial-of-service (DoS) via malformed UDP packets.
- **Asset & Map Parsers (`LabCore`, `LabWorld`):** memory corruption or arbitrary file access when loading `.labpak`, `.labproject`, `.labmap`, `.labscene`, or `.gltf`/`.glb` files.
- **Virtual File System (`VFS`):** directory traversal outside mounted project/package roots.
- **Scripting Runtime (`LabScript`):** sandbox escapes or unintended memory access in the embedded Lua 5.4 bridge.

### Known Tracked Limitations (Not Eligible as New Reports)

The following items are already documented in [`README.md`](README.md) and [`PLAN.md`](PLAN.md) and are scheduled for hardening:
- Lua `os` and `io` library sandboxing in `ScriptEngine` (`LabScript.dll`).
- Canonical path validation in `VFS` (`LabCore.dll`).

## Reporting a Vulnerability

**Please do not report security vulnerabilities through public GitHub Issues, Discussions, or Pull Requests.**

Instead, please report them privately using **[GitHub Private Vulnerability Reporting](../../security/advisories/new)** in this repository.

When submitting a report, please include:
1. Affected module/executable (e.g., `LabServer.exe`, `LabNetwork.dll`, `LabWorld.dll`).
2. Minimal reproduction steps, proof-of-concept (PoC) input/packet/file, or crash stack trace.
3. Compiler, OS, and commit hash tested against.

### Response Timeline & Process

- **Initial Acknowledgment:** within **72 hours** of submission.
- **Status Update & Triage:** within **7 days**, confirming whether the issue is reproduced and accepted.
- **If Accepted:** we will develop and verify a fix (including a regression test in `TestVerify` and sanitizer verification where applicable), then publish a patch and credit you in the release notes/advisory (unless you prefer to remain anonymous).
- **If Declined:** we will provide a technical explanation (e.g., out of scope, requires local untrusted binary execution, or already tracked in `PLAN.md`).
