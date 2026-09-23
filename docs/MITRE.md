# MITRE ATT&CK mapping & blue-team notes

One row per capability. Updated as capabilities land. "Blue team note" =
how a SOC detects this behavior and what telemetry to watch.

## Core (mandatory)

| Capability | Tactic | Technique | Blue team note |
|------------|--------|-----------|----------------|
| Reverse shell (v0.1) | Execution / C2 | T1059 (Command and Scripting Interpreter), T1071 (Application Layer Protocol) | Child `cmd.exe` under an unknown parent (Sysmon EID 1); outbound TCP on an unusual port; plaintext commands visible to IDS |
| C2 tunnel | Command and Control | T1071.001 (Web) or T1071.004 (DNS), T1572 (Protocol Tunneling) | Long-lived session, regular beacon intervals, domain/IP rarity, payload size regularity |
| Credential extraction | Credential Access | T1003 (OS Credential Dumping), T1552 (Unsecured Credentials) | LSASS handle access (Sysmon EID 10) with suspicious GrantedAccess; reads of SAM hive |
| Persistence | Persistence | T1547.001 (Registry Run Keys) - first iteration | Autoruns baseline diff; new Run key value; later iterations target stealthier ASEPs |
| Restart on kill | Persistence / Defense Evasion | T1543 (Create or Modify System Process) or watchdog pattern | Process respawning seconds after termination; parent/child watchdog pair |
| Encrypted comms | C2 / Defense Evasion | T1573.002 (Asymmetric Cryptography) | TLS to non-standard port, self-signed cert, no SNI - but content no longer inspectable; pivot to metadata detection |
| Log rollback | Defense Evasion | T1070.001 (Clear Windows Event Logs) - selective variant | Event-log service restart, gaps in sequence numbers, EID 1102 if a full clear slips through |

## Extended commands (as implemented)

| Command | Tactic | Technique | Blue team note |
|---------|--------|-----------|----------------|
| keylog | Collection / Credential Access | T1056.001 (Input Capture: Keylogging) | `SetWindowsHookEx`/`GetAsyncKeyState` patterns, unexpected file growth |
| rdp | Lateral Movement / Persistence | T1021.001 (RDP) + T1078 | Registry flip of `fDenyTSConnections`, new 3389 listener, logon type 10 events |
| crack | Credential Access | T1110.002 (Offline Cracking) | Happens attacker-side: defense is strong hash hygiene (LSA protection, gMSA) |
| pth | Lateral Movement | T1550.002 (Use Alternate Auth Material: Pass the Hash) | NTLM logons with unusual source, EID 4624 type 3 + logon process NtLmSsp anomalies |
| loot | Collection / Exfiltration | T1005 (Data from Local System), T1041 | Bulk reads of sensitive paths, large outbound transfers |
| phish | Initial Access (assist) | T1566 (Phishing) | Generated lure inherits artifacts of the compromised host - mail gateway + user reporting |
| propagate | Lateral Movement | T1021 (Remote Services), T1570 | New admin$ writes, remote service creation (EID 7045) |
| privesc | Privilege Escalation | T1068 / T1548 (per vector) | Token manipulation, unexpected service/scheduled task creation |
| syscall | Defense Evasion | T1106 (Native API) | Userland EDR hooks bypassed; detect via kernel telemetry / ETW-TI, anomalous syscall patterns |
| shell (built-ins) | Execution | T1059, reduced process telemetry | *Less* visible than cmd.exe - detection shifts to network + memory |

## Method note

Each capability ships in two steps per the subject: (1) known library/tool
to make it work, documented as a known weakness with its detection surface;
(2) replaced by our own stealthier implementation. Both steps get a row or
an update here.
