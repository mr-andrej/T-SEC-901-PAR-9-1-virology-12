# Lab environment

Everything runs on machines we own. No traffic ever leaves the host-only
virtual network.

## Manifest (pin these versions - reproducible demo)

| Role | OS / version | Tools | Notes |
|------|--------------|-------|-------|
| Hypervisor | VMware Workstation 17 (host: Windows 11) | - | snapshots enabled |
| Target VM | Windows 11 Pro 23H2+ (evaluation ISO from Microsoft) | Sysmon (SwiftOnSecurity config), Wireshark | **snapshot "clean" before first implant run** |
| Controller | the Windows host itself, Python 3.12 | `controller/s0p0wn3d.py` | see "why not WSL" below |
| Build machine | WSL2 Ubuntu 24.04 on host | gcc-mingw-w64 | cross-compiles the implant (native WinLibs gcc works too) |

Networking: VMware **VMnet1 (host-only)**, subnet 192.168.189.0/24. The VM
can talk to the host and nothing else - no route to the Internet. This is
both a safety measure and a legal one.

## IP plan (VMnet1, host-only)

| Machine | Address | How to check |
|---------|---------|--------------|
| Host / controller | 192.168.189.1 | `ipconfig` on Windows, "VMware Network Adapter VMnet1" |
| Windows target VM | 192.168.189.x (DHCP) | `ipconfig` inside the VM |

In the VM's VMware settings, set the network adapter to **Host-only**
(VMnet1). `C2_HOST` in `implant/implant.c` must match the host's VMnet1
address (default already set to 192.168.189.1).

## Why the controller does not run in WSL

WSL2 lives behind its own NAT (vEthernet 172.29.x.x). A listener bound in
WSL is forwarded to Windows *localhost* only - a VM on VMnet1 cannot reach
it. Running the Python controller natively on the Windows host avoids this
entirely. (Production alternative: `netsh interface portproxy`, not worth
it for the demo.) WSL is still the build box.

## Setup steps

### 1. Target VM (Windows 11)

1. Install Windows 11 from the evaluation ISO, local account, network
   adapter set to Host-only.
2. Pause Windows Update for the demo window (Settings -> Windows Update ->
   Pause) so a last-minute patch cannot break the keynote.
3. Keep Microsoft Defender **on** - the brief requires the tool to evade
   the AV of the declared environment, and Defender is our declared AV.
   (For v0.1 development only, add a folder exclusion so the raw shell is
   not quarantined before we even test the channel; document this, remove
   the exclusion from v0.2 onward.)
4. Install Sysmon. It is a CLI installer - double-clicking the exe does
   nothing visible. From an **elevated** PowerShell:
   ```powershell
   cd C:\Tools\sysmon   # wherever you put Sysmon64.exe
   Invoke-WebRequest -Uri "https://raw.githubusercontent.com/SwiftOnSecurity/sysmon-config/master/sysmonconfig-export.xml" -OutFile sysmonconfig.xml
   .\Sysmon64.exe -accepteula -i sysmonconfig.xml
   # verify:
   Get-WinEvent -LogName "Microsoft-Windows-Sysmon/Operational" -MaxEvents 5
   ```
   Sysmon gives us the telemetry a SOC would see (process creation,
   network connections) - our "does the blue team catch us?" oracle.
5. Install Wireshark (capture on the VM's Ethernet adapter, filter
   `tcp.port == 4444`).
6. **Take snapshot `clean-base`.**

### 2. Host (controller + build)

Controller (native Windows, Python 3.12 already installed):

```powershell
python controller/s0p0wn3d.py --host 0.0.0.0 --port 4444
```

Allow python.exe through Windows Firewall when prompted (private networks)
- inbound on VMnet1 must reach the listener.

Build the implant in WSL (or with native WinLibs gcc, same result):

```sh
sudo apt-get update && sudo apt-get install -y gcc-mingw-w64-x86-64   # once
x86_64-w64-mingw32-gcc -Os -o implant.exe implant/implant.c -lws2_32
```

Copy `implant.exe` to the Windows VM (VMware drag & drop, shared folders,
or a thumb-drive-style ISO). Note VMware Tools drag & drop requires the VM
tools installed - a shared folder is the reliable path.

### 3. Smoke test

1. Controller listening on the host.
2. Wireshark capturing in the VM (or on the host's VMnet1 adapter).
3. Run `implant.exe` in the VM -> session appears in the controller.
4. `whoami`, `ipconfig` round-trip. Then follow `docs/DEMO_GUIDE.md`.

## Safety rules

- VM network adapter on Host-only (VMnet1) whenever the implant runs.
  Never Bridged or NAT.
- The implant only ever runs inside the target VM.
- Revert the VM to `clean-base` between test sessions (also trains the
  "cover your tracks" objective: compare against rollback by hand).
