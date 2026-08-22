# Third-party references

No third-party source files are vendored verbatim in this source package. PhylaRAM's implementation is original code informed by the following public implementations and API references:

- Microsoft Windows Driver Samples: non-PnP KMDF control-device and IOCTL patterns. Repository license: Microsoft Public License (MS-PL).
- Microsoft Driver Module Framework (DMF) `NonPnp1`: non-PnP driver lifecycle and Service Control Manager loader patterns. Repository license: MIT.
- Microsoft AVML: 64-bit range algebra, bounded acquisition and test philosophy. Repository license: MIT.
- Velocidex WinPmem: embedded-driver packaging, 16 MiB bulk reads and preserving partial reads. Repository license: Apache-2.0.
- Microsoft Windows classic samples: CNG SHA-256 usage. Repository license: MIT.
- Microsoft Windows DDI documentation: `MmGetPhysicalMemoryRangesEx2` and `MmCopyMemory` contracts.

If future revisions copy or modify third-party source rather than merely reimplementing public patterns, update this file and comply with the relevant license and attribution requirements.
