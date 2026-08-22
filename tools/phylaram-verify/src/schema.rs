use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RangeEntry {
    pub driver_run: u32,
    pub start: String,
    pub length: u64,
}

impl RangeEntry {
    pub fn parse_start(&self) -> Result<u64, std::num::ParseIntError> {
        let clean = self.start.trim_start_matches("0x").trim_start_matches("0X");
        u64::from_str_radix(clean, 16)
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct UnreadableEntry {
    pub start: String,
    pub length: u64,
    pub ntstatus: String,
}

impl UnreadableEntry {
    pub fn parse_start(&self) -> Result<u64, std::num::ParseIntError> {
        let clean = self.start.trim_start_matches("0x").trim_start_matches("0X");
        u64::from_str_radix(clean, 16)
    }

    pub fn parse_ntstatus(&self) -> Result<u32, std::num::ParseIntError> {
        let clean = self
            .ntstatus
            .trim_start_matches("0x")
            .trim_start_matches("0X");
        u32::from_str_radix(clean, 16)
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq, Default)]
pub struct KernelHintsMap {
    #[serde(default)]
    pub hypervisor_present: bool,
    #[serde(default)]
    pub vbs_active: bool,
    #[serde(default)]
    pub directory_table_base: Option<String>,
    #[serde(default)]
    pub kpcr_address: Option<String>,
    #[serde(default)]
    pub kernel_base: Option<String>,
    #[serde(default)]
    pub kernel_size: Option<u64>,
    #[serde(default)]
    pub major_version: Option<u32>,
    #[serde(default)]
    pub minor_version: Option<u32>,
    #[serde(default)]
    pub build_number: Option<u32>,
    #[serde(default)]
    pub processors: Option<u32>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct MapFile {
    pub producer: String,
    pub producer_version: String,
    pub schema: String,
    pub status: String,
    pub logical_size: u64,
    pub physical_bytes: u64,
    pub acquired_bytes: u64,
    pub unreadable_bytes: u64,
    pub topology_changed: bool,
    pub sha256: String,
    #[serde(default)]
    pub kernel_hints: Option<KernelHintsMap>,
    pub ranges: Vec<RangeEntry>,
    pub unreadable: Vec<UnreadableEntry>,
}
