# s0P0wn3d

Custom Command & Control (C2) framework - T-SEC-901 "Virology" project.

> **Legal scope:** this tool is built and demonstrated exclusively inside a
> private, consenting lab environment (see `docs/LAB_SETUP.md`). Do not run
> it against any system you do not own or have explicit written permission
> to test.

## What it is

s0P0wn3d is a from-scratch C2 system with two components:

| Component    | Language | Role                                              |
|--------------|----------|---------------------------------------------------|
| `implant/`   | C (Win32)| Runs on the target, connects back to the controller |
| `controller/`| Python 3 | Operator side: listens, manages sessions, issues commands |

Current status: **v0.1 "brut"** - raw reverse shell over plaintext TCP.
Known weaknesses of v0.1 are documented in the source headers and in
`docs/ARCHITECTURE.md`; each is a roadmap item, not an oversight.

## Install

Controller (Linux/macOS/WSL, Python ≥ 3.10, stdlib only):

```sh
git clone <repo-url> && cd T-SEC-901-PAR-9-1-virology-12
python3 controller/s0p0wn3d.py --help
```

Implant (cross-compile from WSL/Linux for the Windows target):

```sh
sudo apt-get install -y gcc-mingw-w64-x86-64   # once
# edit C2_HOST / C2_PORT in implant/implant.c first
x86_64-w64-mingw32-gcc -Os -o implant.exe implant/implant.c -lws2_32
```

## Use

```sh
# 1. Operator machine:
python3 controller/s0p0wn3d.py --host 0.0.0.0 --port 4444

# 2. Windows lab VM (same host-only network):
#    copy implant.exe over and run it
# 3. You get an interactive cmd.exe session in the controller.
```

Full walkthrough, including Wireshark proof and VM setup:
`docs/DEMO_GUIDE.md` and `docs/LAB_SETUP.md`.

## Documentation

- `docs/ARCHITECTURE.md` - design, protocol, roadmap per iteration
- `docs/LAB_SETUP.md` - reproducible lab environment manifest
- `docs/DEMO_GUIDE.md` - how to demo the tool live
- `docs/GITFLOW.md` - team roles, branches, workflow
- `docs/MITRE.md` - ATT&CK mapping and blue-team notes per capability

## External resources & sources

Architectural references studied (design patterns only - no code reused):

- [Sliver](https://github.com/BishopFox/sliver) - implant/controller separation, session model
- [Empire](https://github.com/BC-SECURITY/Empire) - command/tasking model
- [Metasploit](https://github.com/rapid7/metasploit-framework) - staged vs stageless payloads
- [MITRE ATT&CK](https://attack.mitre.org/) - tactic/technique mapping
- Microsoft Learn: WinSock 2, `CreateProcessA`, `STARTUPINFO` handle redirection
- [RFC 8446](https://www.rfc-editor.org/rfc/rfc8446) - TLS 1.3 (planned for v0.2)
