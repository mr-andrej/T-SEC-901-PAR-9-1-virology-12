# s0P0wn3d - Architecture

## Overview

Classic implant/controller C2 design, built from scratch.

```
+----------------+        TCP (TLS from v0.2)        +------------------+
|   Windows VM   |  <==============================  | Operator machine |
|                |   reverse connection (implant     |                  |
|  implant.exe   |   dials out - NAT-friendly)       |  s0p0wn3d.py     |
|  (C, Win32)    |                                   |  (Python 3)      |
+----------------+                                   +------------------+
```

The **implant** initiates the connection (reverse shell). This is the
standard reason C2 implants dial out rather than listen: outbound traffic
crosses NAT and most perimeter firewalls, and no listening port appears on
the target (`netstat` shows an *established outbound* connection, which is
far less suspicious than a new listener).

## Components

### Implant (`implant/implant.c`)

- Pure C against Win32 + WinSock2, no external dependencies, compiles to a
  small standalone PE.
- v0.1: connect-back loop → spawn `cmd.exe` with std handles bound to the
  socket → block until the shell exits → reconnect.
- Single responsibility per iteration: v0.1 proves the channel; stealth,
  crypto and persistence are layered on in later iterations (see roadmap).

### Controller (`controller/s0p0wn3d.py`)

- Python 3 stdlib only.
- v0.1: single-session listener with an interactive operator prompt.
- Later iterations: session table (multiple implants), command routing,
  pluggable command modules (`keylog`, `loot`, ...), crypto handshake.

## Protocol

| Version | Transport | Framing | Crypto |
|---------|-----------|---------|--------|
| v0.1    | raw TCP   | none (timeout-drained stream) | none - *documented weakness* |
| v0.2    | TCP       | length-prefixed messages | TLS, asymmetric (self-signed CA pinned in implant) |
| v0.3+   | TCP or HTTP(S)/DNS tunnel | tasking model (beacon + jitter) | TLS + per-session keys |

## Roadmap / known-weakness resolution order

1. **v0.1 "brut"** - this milestone. Raw reverse shell. Deliberately noisy:
   plaintext, hardcoded C2, spawns `cmd.exe`. Purpose: working channel for
   the first follow-up.
2. **v0.2** - TLS with asymmetric keys (prove encryption in Wireshark:
   v0.1 shows readable plaintext, v0.2 shows `Application Data` only),
   config-file C2 address, proper message framing, multi-session controller.
3. **v0.3** - persistence (registry Run key, then a stealthier method),
   restart-on-kill (watchdog), built-in shell commands to avoid spawning
   `cmd.exe` (reduces Sysmon process-creation telemetry).
4. **v0.4+** - extended commands (loot, keylog, privesc, ...), each mapped
   in `docs/MITRE.md`, log rollback/cover-tracks, AV evasion work against
   the declared lab AV.

## Defender's view (why each v0.1 weakness is detectable)

| Weakness | Detection |
|----------|-----------|
| Plaintext shell traffic | Any IDS sees command/output strings on port 4444 |
| Hardcoded IP/port | Static IOC, extractable with `strings` |
| `cmd.exe` child process | Sysmon Event ID 1, unusual parent-child pair |
| Fixed 5 s reconnect | Beaconing pattern in NetFlow, constant interval |

Each of these is an intentional teaching point: the blue-team notes in
`docs/MITRE.md` explain how a SOC catches v0.1, and each later iteration
removes exactly one detection surface.
