# Production EV, Microsoft WHQL Attestation Signing & HVCI Guide

This guide describes the complete procedure for obtaining a production Microsoft WHQL Attestation signature for `phylaram.sys`, allowing PhylaRAM to load seamlessly on production Windows systems with Secure Boot, Hypervisor-Protected Code Integrity (HVCI), and Memory Integrity enabled without enabling `bcdedit /set testsigning on`.

---

## 1. Prerequisites

1. **Extended Validation (EV) Code Signing Certificate:**
   - Procure a hardware-token (FIPS 140-2 Level 2) or Cloud HSM (Azure Key Vault / DigiCert ONE / Sectigo) EV Code Signing Certificate.
   - Standard code signing certificates are rejected by Microsoft Hardware Dev Center.

2. **Microsoft Partner Center & Hardware Dev Center Account:**
   - Register an organization account on [Microsoft Partner Center](https://partner.microsoft.com/dashboard/hardware).
   - Associate your organization's EV certificate with the Partner Center account.

3. **Windows Driver Kit (WDK) & SDK on Build Machine:**
   - Includes `Inf2Cat.exe`, `MakeCab.exe`, and `SignTool.exe`.

---

## 2. Kernel HVCI & Memory Integrity Compliance Checklist

`phylaram.sys` has been engineered from inception to be 100% compliant with HVCI / Memory Integrity rules:

| Requirement | Implementation in PhylaRAM | Status |
| :--- | :--- | :---: |
| **Control Flow Guard** | `/guard:cf` enabled in MSVC KMDF compiler & linker properties | ✅ Pass |
| **No Executable Pool** | All pool allocations use non-executable pool types (`NonPagedPoolNx`) | ✅ Pass |
| **No Runtime Code Modification** | Zero inline hooking, no PTE tampering, no writable code sections | ✅ Pass |
| **Secure SDDL Descriptors** | Device control object protected with `D:P(A;;GA;;;SY)(A;;GA;;;BA)` | ✅ Pass |
| **Exclusive Access** | Enforced via `WdfDeviceInitSetExclusive(..., TRUE)` | ✅ Pass |
| **Page Segment Directives** | Paged functions declared with `#pragma alloc_text(PAGE, ...)` | ✅ Pass |

---

## 3. Step-by-Step Submission Procedure

### Step 3.1: Build Release Driver
Build the 64-bit release driver binary:
```cmd
msbuild driver\phylaram.vcxproj /p:Configuration=Release /p:Platform=x64 /v:minimal
```

### Step 3.2: Generate Submission Package (.cab)
Run the automated submission preparation script:
```powershell
powershell -ExecutionPolicy Bypass -File scripts\prepare_whql_submission.ps1
```
This generates `dist\WHQL_Submission\PhylaRAM_WHQL_Submission.cab` containing:
- `phylaram.sys`
- `phylaram.inf`
- `phylaram.cat` (generated via `Inf2Cat`)

### Step 3.3: Sign Cabinet with EV Certificate
Sign the cabinet file using your organization's EV certificate:
```cmd
signtool.exe sign /fd SHA256 /sha1 <YOUR_EV_CERT_THUMBPRINT> /tr http://timestamp.digicert.com /td SHA256 dist\WHQL_Submission\PhylaRAM_WHQL_Submission.cab
```

### Step 3.4: Submit to Microsoft Hardware Developer Center
1. Navigate to **[Microsoft Hardware Developer Center Dashboard](https://partner.microsoft.com/dashboard/hardware)**.
2. Click **Submit new driver**.
3. Upload `PhylaRAM_WHQL_Submission.cab`.
4. Select target operating systems:
   - *Windows 10, version 1809 and later (x64)*
   - *Windows 11 (x64)*
   - *Windows Server 2019, 2022, 2025 (x64)*
5. Complete the submission wizard. Microsoft automated attestation signing typically finishes within 10 to 30 minutes.

### Step 3.5: Download & Distribute Production Driver
1. Download the signed package (`.zip`) from Partner Center.
2. Extract the Microsoft-signed `phylaram.sys` and `phylaram.cat`.
3. The resulting binary carries an Authenticode leaf signature issued by `Microsoft Windows Hardware Compatibility Publisher` and loads on all production Windows systems out-of-the-box.
