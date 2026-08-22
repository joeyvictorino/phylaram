# PhylaRAM: Official Microsoft/Apple Incident Response UI Specification

**Document ID:** `PHYLA-GUI-SPEC-003`  
**Status:** Canonical Visual Standard  
**Design Philosophy:** "Apple Designed, Microsoft Built" — Official Sysinternals / PowerToys Quality  
**Audience:** Non-technical on-site users & sysadmins during live incident response, powered by the professional CLI engine for principal IR consultants  

---

## 1. The Core Product Dual-Persona

During an active cyber incident or breach investigation, friction kills evidence:

```text
+---------------------------------------------------------------------------------------------------+
|                                 PhylaRAM Dual-Persona Architecture                                |
+-------------------------------------------------+-------------------------------------------------+
| Non-Technical / On-Site Sysadmin Persona        | Principal IR Consultant Persona                 |
| (The Official Fluent Windows 11 GUI)            | (The Non-Interactive C++20 / Rust CLI)          |
+-------------------------------------------------+-------------------------------------------------+
| - Zero technical jargon                         | - Pure headless CLI: `phylaram.exe memory.raw`  |
| - Handed to any employee or sysadmin over Slack | - EDR Live Response / Velociraptor automatable  |
| - 1 single action: "Start Memory Capture"       | - 16 MiB direct I/O chunking, 4 KiB isolation   |
| - Reassuring progress & time estimates          | - Strict exit codes: 0 (complete), 2 (partial)  |
| - 1 single result: "Open Folder to Send Files"  | - Offline verification via `phylaram-verify`    |
| - Zero confusing forensic analysis buttons      | - Direct Volatility 3 & MemProcFS analysis      |
+-------------------------------------------------+-------------------------------------------------+
```

---

## 2. Visual Identity & Design System

### 2.1 Aesthetic Language: Official Microsoft Fluent Meets Apple Simplicity
- **Canvas:** Dark mode Windows 11 Mica & Acrylic material with subtle backdrop diffusion.
- **Typography:** Segoe UI Variable Display for headers, Segoe UI Text for body, and subtle system accents.
- **Colors:** Deep charcoal neutral card surfaces (`#20232A`), official Windows 11 accent blue (`#0078D4` / `#4A9EFF`) for primary action, and reassuring emerald green (`#107C41` / `#00CC6A`) for verified completion.
- **Zero Antivirus / Gamer Tropes:** No cyan halos, no gamer RGB meters, and no confusing options.

---

## 3. The 3-Step Incident Response User Flow

### Screen 1: The One-Click IR Start Screen
- **Header:** `PhylaRAM` — `Live Memory Capture for Windows`
- **Detected System Memory:** `32.0 GB Physical Memory Detected`
- **Destination:** Auto-selects external USB or default case folder (`Save Evidence To: E:\Incident_Response\memory.raw (1.8 TB Free · Ready)`)
- **Single Primary Action:** `[ Start Memory Capture ]` (High-contrast official Windows 11 style primary button)

### Screen 2: The Calm Progress Screen
- **Header:** `PhylaRAM` — `Capturing Memory...`
- **Progress Bar:** Smooth Windows 11 progress bar (`21.8 GB of 32.0 GB captured · 68%`)
- **Reassurance Notice:** `Estimated time remaining: ~12 seconds · Please keep this computer on until capture completes.`
- **Action:** Discrete `[ Cancel ]` button.

### Screen 3: The Evidence Secured Screen
- **Status Icon:** Reassuring green checkmark badge: `Memory Capture Complete`
- **Subtitle:** `Physical memory has been safely saved and verified.`
- **Evidence Card:** `Location: E:\Incident_Response\memory.raw (32.0 GB) · Evidence bundle ready for your Incident Response team (3 files)`
- **Actions:**
  - Primary: `[ Open Folder ]` (Opens Windows Explorer with the 3 evidence files highlighted).
  - Secondary: `[ Copy Path ]` (Copies file path to clipboard for instant pasting into incident chat).

---

## 4. Under-the-Hood Guarantees

Regardless of whether PhylaRAM is invoked via the **One-Click GUI** or the **Headless CLI**, the exact same forensic invariants are strictly enforced:
1. `file offset == physical address` (Flat raw physical image).
2. `UNREADABLE != ZERO` (Exact NTSTATUS isolated at 4 KiB page boundaries).
3. 6-path preflight collision protection.
4. Restrictive SDDL driver extraction into `%ProgramData%\PhylaRAM\Temp\`.
5. Quoted service path in SCM (CWE-428 elimination).
6. Automatic `memory.raw.map.json` provenance sidecar and `memory.raw.sha256` digest generation.
