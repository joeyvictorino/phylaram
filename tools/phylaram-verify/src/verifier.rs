use crate::hash::calculate_logical_raw_sha256;
use crate::schema::MapFile;
use std::fmt;
use std::fs;
use std::path::Path;

#[derive(Debug)]
pub enum VerificationError {
    Io(std::io::Error),
    Json(serde_json::Error),
    ParseInt(std::num::ParseIntError),
    InvalidProducer(String),
    InvalidSchema(String),
    InvalidStatus(String),
    SizeMismatch { expected: u64, actual: u64 },
    RangeOverlap { prev_end: u64, next_start: u64 },
    ZeroLengthRange,
    PhysicalSumMismatch { expected: u64, actual: u64 },
    AccountingMismatch { physical: u64, sum: u64 },
    HashMismatch { expected: String, actual: String },
    SidecarMismatch { sidecar: String, computed: String },
    TopologyStatusMismatch,
    UnreadableStatusMismatch,
    InvalidUnreadableSpan { start: u64, len: u64 },
}

impl fmt::Display for VerificationError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            VerificationError::Io(e) => write!(f, "I/O error: {}", e),
            VerificationError::Json(e) => write!(f, "JSON parse error: {}", e),
            VerificationError::ParseInt(e) => write!(f, "Integer parse error: {}", e),
            VerificationError::InvalidProducer(p) => write!(f, "Invalid producer: '{}'", p),
            VerificationError::InvalidSchema(s) => write!(f, "Invalid schema: '{}'", s),
            VerificationError::InvalidStatus(s) => write!(f, "Invalid status: '{}'", s),
            VerificationError::SizeMismatch { expected, actual } => {
                write!(
                    f,
                    "Size mismatch: expected {} bytes, got {}",
                    expected, actual
                )
            }
            VerificationError::RangeOverlap {
                prev_end,
                next_start,
            } => {
                write!(
                    f,
                    "Range overlap: prev_end 0x{:X} > next_start 0x{:X}",
                    prev_end, next_start
                )
            }
            VerificationError::ZeroLengthRange => write!(f, "Found 0-length physical RAM range"),
            VerificationError::PhysicalSumMismatch { expected, actual } => {
                write!(
                    f,
                    "Physical sum mismatch: expected {} bytes, sum of ranges is {}",
                    expected, actual
                )
            }
            VerificationError::AccountingMismatch { physical, sum } => {
                write!(
                    f,
                    "Accounting mismatch: physical {} != acquired + unreadable {}",
                    physical, sum
                )
            }
            VerificationError::HashMismatch { expected, actual } => {
                write!(
                    f,
                    "Hash mismatch: expected {}, computed {}",
                    expected, actual
                )
            }
            VerificationError::SidecarMismatch { sidecar, computed } => {
                write!(
                    f,
                    "Sidecar hash mismatch: sidecar '{}', computed '{}'",
                    sidecar, computed
                )
            }
            VerificationError::TopologyStatusMismatch => {
                write!(f, "Topology changed but status is not 'incomplete'")
            }
            VerificationError::UnreadableStatusMismatch => {
                write!(f, "Unreadable bytes > 0 but status is not 'incomplete'")
            }
            VerificationError::InvalidUnreadableSpan { start, len } => {
                write!(
                    f,
                    "Invalid unreadable span: start 0x{:X}, length {}",
                    start, len
                )
            }
        }
    }
}

impl std::error::Error for VerificationError {}

impl From<std::io::Error> for VerificationError {
    fn from(e: std::io::Error) -> Self {
        VerificationError::Io(e)
    }
}

impl From<serde_json::Error> for VerificationError {
    fn from(e: serde_json::Error) -> Self {
        VerificationError::Json(e)
    }
}

impl From<std::num::ParseIntError> for VerificationError {
    fn from(e: std::num::ParseIntError) -> Self {
        VerificationError::ParseInt(e)
    }
}

pub fn verify_bundle(
    raw_path: &Path,
    map_path: &Path,
    sha_path: Option<&Path>,
) -> Result<MapFile, VerificationError> {
    let map_data = fs::read_to_string(map_path)?;
    let map: MapFile = serde_json::from_str(&map_data)?;

    if map.producer != "PhylaRAM" {
        return Err(VerificationError::InvalidProducer(map.producer));
    }
    if map.schema != "phylaram-map-1" && map.schema != "phylaram-map-2" {
        return Err(VerificationError::InvalidSchema(map.schema));
    }

    let meta = fs::metadata(raw_path)?;
    if meta.len() != map.logical_size {
        return Err(VerificationError::SizeMismatch {
            expected: map.logical_size,
            actual: meta.len(),
        });
    }

    let mut sum_physical = 0u64;
    let mut prev_end = 0u64;
    for (i, r) in map.ranges.iter().enumerate() {
        if r.length == 0 {
            return Err(VerificationError::ZeroLengthRange);
        }
        let start = r.parse_start()?;
        if i > 0 && start < prev_end {
            return Err(VerificationError::RangeOverlap {
                prev_end,
                next_start: start,
            });
        }
        sum_physical += r.length;
        prev_end = start + r.length;
    }

    if sum_physical != map.physical_bytes {
        return Err(VerificationError::PhysicalSumMismatch {
            expected: map.physical_bytes,
            actual: sum_physical,
        });
    }

    let mut sum_unreadable = 0u64;
    for u in &map.unreadable {
        let start = u.parse_start()?;
        let _ntstatus = u.parse_ntstatus()?;
        if u.length == 0 {
            return Err(VerificationError::InvalidUnreadableSpan {
                start,
                len: u.length,
            });
        }
        sum_unreadable += u.length;
    }

    if sum_unreadable != map.unreadable_bytes {
        return Err(VerificationError::AccountingMismatch {
            physical: map.unreadable_bytes,
            sum: sum_unreadable,
        });
    }

    let accounted = map.acquired_bytes + map.unreadable_bytes;
    if accounted != map.physical_bytes {
        return Err(VerificationError::AccountingMismatch {
            physical: map.physical_bytes,
            sum: accounted,
        });
    }

    if map.topology_changed && map.status != "incomplete" {
        return Err(VerificationError::TopologyStatusMismatch);
    }
    if map.unreadable_bytes > 0 && map.status != "incomplete" {
        return Err(VerificationError::UnreadableStatusMismatch);
    }
    if !map.topology_changed && map.unreadable_bytes == 0 && map.status != "complete" {
        return Err(VerificationError::InvalidStatus(map.status));
    }

    let computed_hash = calculate_logical_raw_sha256(raw_path, map.logical_size)?;
    if !computed_hash.eq_ignore_ascii_case(&map.sha256) {
        return Err(VerificationError::HashMismatch {
            expected: map.sha256,
            actual: computed_hash.clone(),
        });
    }

    if let Some(sha_file) = sha_path {
        let sidecar_content = fs::read_to_string(sha_file)?;
        let first_word = sidecar_content
            .split_whitespace()
            .next()
            .unwrap_or("")
            .trim();
        if !computed_hash.eq_ignore_ascii_case(first_word) {
            return Err(VerificationError::SidecarMismatch {
                sidecar: first_word.to_string(),
                computed: computed_hash,
            });
        }
    }

    Ok(map)
}
