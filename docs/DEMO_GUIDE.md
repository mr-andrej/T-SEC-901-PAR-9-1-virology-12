# Demo guide - v0.1 reverse shell (follow-up)

Goal of the demo: prove a working implant->controller channel live, and show
we understand what a defender sees.

Lab state (verified working):
- Host / controller: Windows host, 192.168.189.1 (VMnet1, host-only)
- Target VM: Windows 11, 192.168.189.x (VMware adapter set to Host-only)
- Sysmon installed on the VM (default config for now: EID 1 process
  creation is logged; EID 3 network connections need the SwiftOnSecurity
  config - load later with `.\Sysmon64.exe -c sysmonconfig.xml`)
- Windows Update paused on the VM for the demo window

## 5-minute demo script (every step verified on the real lab)

1. **Controller up** (Windows host):
   ```
   python controller/s0p0wn3d.py --host 0.0.0.0 --port 4444
   ```
   Shows: `waiting for implant...` (allow the Windows Firewall prompt).

2. **Start Wireshark** in the VM on its Ethernet adapter, filter:
   `tcp.port == 4444`.

3. **Run the implant** in the VM: `.\implant.exe`.
   Controller prints: `[+] session from 192.168.189.x:...` followed by the
   cmd.exe banner from inside the VM.

4. **Run commands** from the controller prompt:
   ```
   whoami
   ipconfig
   ```
   Expect the VM user and the VM's 192.168.189.x address - proof the
   commands execute inside the VM, not on the host.

5. **Resilience**: type `exit`. Session drops, controller returns to
   listening, and about 5 seconds later a new `[+] session from ...`
   appears on its own - the implant's reconnect loop. This is the seed of
   the persistence requirement.

6. **Show the Wireshark capture**: commands and their output are readable
   in plaintext. Say it out loud: "this is v0.1's documented weakness #1 -
   v0.2 encrypts with TLS and we will re-run this exact capture to prove
   it." A weakness presented with its fix is method, not failure.

7. **Show the defender view**: Event Viewer -> Applications and Services
   Logs -> Microsoft -> Windows -> Sysmon -> Operational. Find Event ID 1
   (Process Create): `cmd.exe` with parent `implant.exe`. Same message:
   "v0.3 replaces the spawned shell with built-in commands, and this is
   exactly the telemetry we are designing against."

## Fallbacks

- VM misbehaves -> the controller was also tested on localhost with a mock
  implant; the wire behavior is identical.
- No session -> check the VM adapter is Host-only (VMnet1), `ping
  192.168.189.1` from the VM, and that the firewall prompt for python.exe
  was accepted.
- Defender quarantines implant.exe -> expected for v0.1; use the
  documented folder exclusion and turn the moment into a talking point
  about signature-based detection.

## Questions/blockers to raise with Thomas (prepared)

1. Is a self-signed TLS CA pinned in the implant acceptable as the
   "asymmetric encryption" proof, or is a specific scheme expected?
2. For "survive a reboot": is a registry Run key acceptable as the first
   persistence mechanism, knowing we then replace it with a stealthier one?
3. Log rollback: are we expected to edit Windows Event Logs (wevtutil /
   API) specifically, or is a general "selective cleanup, not wipe"
   demonstration on any log source acceptable?
4. Anti-virus: is Microsoft Defender with default settings the declared AV,
   or should we declare a different one?
