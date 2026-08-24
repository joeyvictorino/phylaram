======================================================================
                         PHYLA RAM (ALPHA)
         LAB / CONTROLLED TEST ENVIRONMENT ONLY — TEST-SIGNED
======================================================================

WARNING:
This archive contains an ALPHA PRE-RELEASE of PhylaRAM with a TEST-SIGNED
KMDF kernel driver (phylaram.sys).

• THIS DRIVER IS NOT PRODUCTION-SIGNED BY MICROSOFT.
• THIS BUILD HAS NOT BEEN CERTIFIED UNDER WHCP/HLK.
• DO NOT DISABLE SECURE BOOT OR HVCI/MEMORY INTEGRITY ON A REAL EVIDENCE HOST.
• USE EXCLUSIVELY IN DEDICATED FORENSIC TEST VMS / LAB MACHINES.

----------------------------------------------------------------------
HOW TO TEST IN A LAB VM:
----------------------------------------------------------------------
1. In an elevated Command Prompt or PowerShell in your test VM:
     bcdedit /set testsigning on
2. Reboot the test machine.
3. Import the test signing certificate (run install_test_cert.bat as Admin).
4. Run:
     phylaram.exe C:\evidence\test_capture.raw
   Or launch the GUI:
     phylaram.exe --gui
5. Verify the capture with the independent Rust verifier:
     phylaram-verify.exe C:\evidence\test_capture.raw C:\evidence\test_capture.raw.map.json C:\evidence\test_capture.raw.sha256

----------------------------------------------------------------------
FINALIZED EVIDENCE BUNDLE CONTRACT:
----------------------------------------------------------------------
Every successful capture transaction produces exactly three files:
  1. memory.raw          - Flat physical-address-preserving binary image
  2. memory.raw.map.json - Canonical provenance map (phylaram-map-2)
  3. memory.raw.sha256   - SHA-256 cryptographic sidecar

Independent offline verification with phylaram-verify is mandatory before
drawing forensic conclusions.

For documentation, roadmap, and open validation gates, visit:
https://github.com/joeyvictorino/phylaram
