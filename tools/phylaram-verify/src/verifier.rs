use crate::hash::calculate_logical_raw_sha256;
use crate::schema::{MapFile, RangeEntry, UnreadableEntry};
use std::collections::BTreeSet;
use std::fmt;
use std::fs::{self, File};
use std::io::{Read, Seek, SeekFrom};
use std::path::Path;

const ZERO_CHECK_CHUNK_BYTES: usize = 1024 * 1024;

#[derive(Debug)]
pub enum VerificationError {
    Io(std::io::Error),
    Json(serde_json::Error),
    ParseInt(std::num::ParseIntError),
    InvalidProducer(String),
    InvalidSchema(String),
    InvalidStatus(String),
    InvalidHashEncoding(String),
    SizeMismatch { expected: u64, actual: u64 },
    ZeroLengthRange { index: usize },
    RangeArithmeticOverflow { index: usize },
    RangeBeyondLogicalSize { index: usize, end: u64, logical_size: u64 },
    RangeOverlap { previous_end: u64, next_start: u64 },
    HighestPhysicalEndMismatch { expected: u64, actual: u64 },
    DuplicateDriverRun(u32),
    DriverRunDomainMismatch,
    PhysicalSumOverflow,
    PhysicalSumMismatch { expected: u64, actual: u64 },
    AccountingOverflow,
    AccountingMismatch { physical: u64, accounted: u64 },
    InvalidUnreadableSpan { index: usize, start: u64, length: u64 },
    UnreadableArithmeticOverflow { index: usize },
    UnreadableOverlap { previous_end: u64, next_start: u64 },
    UnreadableOutsidePhysicalRun { index: usize, start: u64, end: u64 },
    UnreadableSumOverflow,
    UnreadableSumMismatch { expected: u64, actual: u64 },
    UnreadableRepresentationMismatch { start: u64, offset: u64 },
    HashMismatch { expected: String, actual: String },
    SidecarMismatch { sidecar: String, computed: String },
}

impl fmt::Display for VerificationError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Io(error) => write!(f, "I/O error: {error}"),
            Self::Json(error) => write!(f, "JSON parse error: {error}"),
            Self::ParseInt(error) => write!(f, "integer parse error: {error}"),
            Self::InvalidProducer(producer) => write!(f, "invalid producer: '{producer}'"),
            Self::InvalidSchema(schema) => write!(f, "unsupported map schema: '{schema}'"),
            Self::InvalidStatus(status) => write!(f, "invalid terminal status: '{status}'"),
            Self::InvalidHashEncoding(hash) => {
                write!(f, "map SHA-256 is not exactly 64 hexadecimal characters: '{hash}'")
            }
            Self::SizeMismatch { expected, actual } => {
                write!(f, "RAW size mismatch: expected {expected} bytes, got {actual}")
            }
            Self::ZeroLengthRange { index } => {
                write!(f, "physical range {index} has zero length")
            }
            Self::RangeArithmeticOverflow { index } => {
                write!(f, "physical range {index} overflows the 64-bit address domain")
            }
            Self::RangeBeyondLogicalSize {
                index,
                end,
                logical_size,
            } => write!(
                f,
                "physical range {index} ends at 0x{end:X}, beyond logical size 0x{logical_size:X}"
            ),
            Self::RangeOverlap {
                previous_end,
                next_start,
            } => write!(
                f,
                "physical ranges overlap: previous end 0x{previous_end:X}, next start 0x{next_start:X}"
            ),
            Self::HighestPhysicalEndMismatch { expected, actual } => write!(
                f,
                "logical size does not equal the highest physical range end: expected 0x{expected:X}, got 0x{actual:X}"
            ),
            Self::DuplicateDriverRun(run) => write!(f, "duplicate driver_run index {run}"),
            Self::DriverRunDomainMismatch => {
                write!(f, "driver_run indices are not exactly the zero-based range domain")
            }
            Self::PhysicalSumOverflow => write!(f, "sum of physical range lengths overflowed u64"),
            Self::PhysicalSumMismatch { expected, actual } => write!(
                f,
                "physical byte total mismatch: map says {expected}, ranges sum to {actual}"
            ),
            Self::AccountingOverflow => {
                write!(f, "acquired_bytes + unreadable_bytes overflowed u64")
            }
            Self::AccountingMismatch {
                physical,
                accounted,
            } => write!(
                f,
                "physical byte accounting mismatch: physical {physical}, acquired + unreadable {accounted}"
            ),
            Self::InvalidUnreadableSpan {
                index,
                start,
                length,
            } => write!(
                f,
                "unreadable span {index} is invalid: start 0x{start:X}, length {length}"
            ),
            Self::UnreadableArithmeticOverflow { index } => write!(
                f,
                "unreadable span {index} overflows the 64-bit address domain"
            ),
            Self::UnreadableOverlap {
                previous_end,
                next_start,
            } => write!(
                f,
                "unreadable spans overlap: previous end 0x{previous_end:X}, next start 0x{next_start:X}"
            ),
            Self::UnreadableOutsidePhysicalRun { index, start, end } => write!(
                f,
                "unreadable span {index} [0x{start:X}, 0x{end:X}) is not contained in one reported physical run"
            ),
            Self::UnreadableSumOverflow => {
                write!(f, "sum of unreadable span lengths overflowed u64")
            }
            Self::UnreadableSumMismatch { expected, actual } => write!(
                f,
                "unreadable byte total mismatch: map says {expected}, spans sum to {actual}"
            ),
            Self::UnreadableRepresentationMismatch { start, offset } => write!(
                f,
                "RAW representation is non-zero inside unreadable span starting at 0x{start:X}, at relative offset {offset}"
            ),
            Self::HashMismatch { expected, actual } => write!(
                f,
                "map hash mismatch: expected {expected}, computed {actual}"
            ),
            Self::SidecarMismatch { sidecar, computed } => write!(
                f,
                "SHA-256 sidecar mismatch: sidecar '{sidecar}', computed '{computed}'"
            ),
        }
    }
}

impl std::error::Error for VerificationError {}

impl From<std::io::Error> for VerificationError {
    fn from(error: std::io::Error) -> Self {
        Self::Io(error)
    }
}

impl From<serde_json::Error> for VerificationError {
    fn from(error: serde_json::Error) -> Self {
        Self::Json(error)
    }
}

impl From<std::num::ParseIntError> for VerificationError {
    fn from(error: std::num::ParseIntError) -> Self {
        Self::ParseInt(error)
    }
}

#[derive(Debug, Clone, Copy)]
struct ParsedRange {
    start: u64,
    end: u64,
}

fn is_sha256_hex(value: &str) -> bool {
    value.len() == 64 && value.bytes().all(|byte| byte.is_ascii_hexdigit())
}

fn validate_ranges(map: &MapFile) -> Result<Vec<ParsedRange>, VerificationError> {
    if map.ranges.is_empty() {
        return Err(VerificationError::DriverRunDomainMismatch);
    }

    let mut parsed = Vec::with_capacity(map.ranges.len());
    let mut seen_driver_runs = BTreeSet::new();
    let mut previous_end = 0u64;
    let mut physical_sum = 0u64;
    let mut highest_end = 0u64;

    for (index, range) in map.ranges.iter().enumerate() {
        if range.length == 0 {
            return Err(VerificationError::ZeroLengthRange { index });
        }
        if !seen_driver_runs.insert(range.driver_run) {
            return Err(VerificationError::DuplicateDriverRun(range.driver_run));
        }

        let start = range.parse_start()?;
        let end = start
            .checked_add(range.length)
            .ok_or(VerificationError::RangeArithmeticOverflow { index })?;
        if end > map.logical_size {
            return Err(VerificationError::RangeBeyondLogicalSize {
                index,
                end,
                logical_size: map.logical_size,
            });
        }
        if index != 0 && start < previous_end {
            return Err(VerificationError::RangeOverlap {
                previous_end,
                next_start: start,
            });
        }

        physical_sum = physical_sum
            .checked_add(range.length)
            .ok_or(VerificationError::PhysicalSumOverflow)?;
        previous_end = end;
        highest_end = highest_end.max(end);
        parsed.push(ParsedRange { start, end });
    }

    if physical_sum != map.physical_bytes {
        return Err(VerificationError::PhysicalSumMismatch {
            expected: map.physical_bytes,
            actual: physical_sum,
        });
    }
    if highest_end != map.logical_size {
        return Err(VerificationError::HighestPhysicalEndMismatch {
            expected: highest_end,
            actual: map.logical_size,
        });
    }

    let expected_runs: BTreeSet<u32> = (0..map.ranges.len())
        .map(|index| u32::try_from(index))
        .collect::<Result<_, _>>()
        .map_err(|_| VerificationError::DriverRunDomainMismatch)?;
    if seen_driver_runs != expected_runs {
        return Err(VerificationError::DriverRunDomainMismatch);
    }

    Ok(parsed)
}

fn span_is_contained_in_range(start: u64, end: u64, ranges: &[ParsedRange]) -> bool {
    ranges
        .iter()
        .any(|range| start >= range.start && end <= range.end)
}

fn validate_unreadable_spans(
    map: &MapFile,
    ranges: &[ParsedRange],
) -> Result<(), VerificationError> {
    let mut previous_end = 0u64;
    let mut unreadable_sum = 0u64;

    for (index, span) in map.unreadable.iter().enumerate() {
        let start = span.parse_start()?;
        let _ntstatus = span.parse_ntstatus()?;
        if span.length == 0 {
            return Err(VerificationError::InvalidUnreadableSpan {
                index,
                start,
                length: span.length,
            });
        }

        let end = start
            .checked_add(span.length)
            .ok_or(VerificationError::UnreadableArithmeticOverflow { index })?;
        if index != 0 && start < previous_end {
            return Err(VerificationError::UnreadableOverlap {
                previous_end,
                next_start: start,
            });
        }
        if !span_is_contained_in_range(start, end, ranges) {
            return Err(VerificationError::UnreadableOutsidePhysicalRun { index, start, end });
        }

        unreadable_sum = unreadable_sum
            .checked_add(span.length)
            .ok_or(VerificationError::UnreadableSumOverflow)?;
        previous_end = end;
    }

    if unreadable_sum != map.unreadable_bytes {
        return Err(VerificationError::UnreadableSumMismatch {
            expected: map.unreadable_bytes,
            actual: unreadable_sum,
        });
    }

    Ok(())
}

fn verify_unreadable_representation(
    raw_path: &Path,
    unreadable: &[UnreadableEntry],
) -> Result<(), VerificationError> {
    if unreadable.is_empty() {
        return Ok(());
    }

    let mut file = File::open(raw_path)?;
    let mut buffer = vec![0u8; ZERO_CHECK_CHUNK_BYTES];

    for span in unreadable {
        let start = span.parse_start()?;
        file.seek(SeekFrom::Start(start))?;

        let mut remaining = span.length;
        let mut relative_offset = 0u64;
        while remaining != 0 {
            let requested = usize::try_from(remaining.min(buffer.len() as u64))
                .expect("bounded by buffer length");
            file.read_exact(&mut buffer[..requested])?;

            if let Some(non_zero) = buffer[..requested].iter().position(|byte| *byte != 0) {
                return Err(VerificationError::UnreadableRepresentationMismatch {
                    start,
                    offset: relative_offset + non_zero as u64,
                });
            }

            remaining -= requested as u64;
            relative_offset += requested as u64;
        }
    }

    Ok(())
}

fn validate_status(map: &MapFile) -> Result<(), VerificationError> {
    let expected = if map.topology_changed || map.unreadable_bytes != 0 {
        "incomplete"
    } else {
        "complete"
    };

    if map.status != expected {
        return Err(VerificationError::InvalidStatus(map.status.clone()));
    }
    Ok(())
}

fn validate_sidecar(
    sha_path: &Path,
    computed_hash: &str,
) -> Result<(), VerificationError> {
    let sidecar_content = fs::read_to_string(sha_path)?;
    let sidecar_hash = sidecar_content
        .split_whitespace()
        .next()
        .unwrap_or("")
        .trim();

    if !is_sha256_hex(sidecar_hash)
        || !computed_hash.eq_ignore_ascii_case(sidecar_hash)
    {
        return Err(VerificationError::SidecarMismatch {
            sidecar: sidecar_hash.to_owned(),
            computed: computed_hash.to_owned(),
        });
    }
    Ok(())
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
    if !is_sha256_hex(&map.sha256) {
        return Err(VerificationError::InvalidHashEncoding(map.sha256));
    }

    let raw_metadata = fs::metadata(raw_path)?;
    if raw_metadata.len() != map.logical_size {
        return Err(VerificationError::SizeMismatch {
            expected: map.logical_size,
            actual: raw_metadata.len(),
        });
    }

    let ranges = validate_ranges(&map)?;
    validate_unreadable_spans(&map, &ranges)?;

    let accounted = map
        .acquired_bytes
        .checked_add(map.unreadable_bytes)
        .ok_or(VerificationError::AccountingOverflow)?;
    if accounted != map.physical_bytes {
        return Err(VerificationError::AccountingMismatch {
            physical: map.physical_bytes,
            accounted,
        });
    }

    validate_status(&map)?;
    verify_unreadable_representation(raw_path, &map.unreadable)?;

    let computed_hash = calculate_logical_raw_sha256(raw_path, map.logical_size)?;
    if !computed_hash.eq_ignore_ascii_case(&map.sha256) {
        return Err(VerificationError::HashMismatch {
            expected: map.sha256,
            actual: computed_hash.clone(),
        });
    }

    if let Some(sidecar_path) = sha_path {
        validate_sidecar(sidecar_path, &computed_hash)?;
    }

    Ok(map)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::schema::{MapFile, RangeEntry, UnreadableEntry};
    use sha2::{Digest, Sha256};
    use std::fs;
    use tempfile::tempdir;

    fn hex_hash(bytes: &[u8]) -> String {
        hex::encode(Sha256::digest(bytes))
    }

    fn base_map(raw: &[u8]) -> MapFile {
        MapFile {
            producer: "PhylaRAM".to_owned(),
            producer_version: "0.1.0-alpha".to_owned(),
            schema: "phylaram-map-2".to_owned(),
            status: "complete".to_owned(),
            logical_size: raw.len() as u64,
            physical_bytes: raw.len() as u64,
            acquired_bytes: raw.len() as u64,
            unreadable_bytes: 0,
            topology_changed: false,
            sha256: hex_hash(raw),
            kernel_hints: None,
            ranges: vec![RangeEntry {
                driver_run: 0,
                start: "0x0".to_owned(),
                length: raw.len() as u64,
            }],
            unreadable: Vec::new(),
        }
    }

    fn verify_fixture(raw: &[u8], map: &MapFile) -> Result<MapFile, VerificationError> {
        let directory = tempdir().unwrap();
        let raw_path = directory.path().join("memory.raw");
        let map_path = directory.path().join("memory.raw.map.json");
        fs::write(&raw_path, raw).unwrap();
        fs::write(&map_path, serde_json::to_vec(map).unwrap()).unwrap();
        verify_bundle(&raw_path, &map_path, None)
    }

    #[test]
    fn rejects_unreadable_span_outside_physical_ranges() {
        let raw = vec![0u8; 8192];
        let mut map = base_map(&raw);
        map.physical_bytes = 4096;
        map.acquired_bytes = 0;
        map.unreadable_bytes = 4096;
        map.status = "incomplete".to_owned();
        map.ranges[0].length = 4096;
        map.logical_size = 8192;
        map.unreadable = vec![UnreadableEntry {
            start: "0x1000".to_owned(),
            length: 4096,
            ntstatus: "0xC0000001".to_owned(),
        }];

        let error = verify_fixture(&raw, &map).unwrap_err();
        assert!(matches!(
            error,
            VerificationError::HighestPhysicalEndMismatch { .. }
                | VerificationError::UnreadableOutsidePhysicalRun { .. }
        ));
    }

    #[test]
    fn rejects_overlapping_unreadable_spans() {
        let raw = vec![0u8; 8192];
        let mut map = base_map(&raw);
        map.acquired_bytes = 2048;
        map.unreadable_bytes = 6144;
        map.status = "incomplete".to_owned();
        map.unreadable = vec![
            UnreadableEntry {
                start: "0x0".to_owned(),
                length: 4096,
                ntstatus: "0xC0000001".to_owned(),
            },
            UnreadableEntry {
                start: "0x800".to_owned(),
                length: 2048,
                ntstatus: "0xC0000001".to_owned(),
            },
        ];

        let error = verify_fixture(&raw, &map).unwrap_err();
        assert!(matches!(error, VerificationError::UnreadableOverlap { .. }));
    }

    #[test]
    fn rejects_non_zero_bytes_claimed_unreadable() {
        let mut raw = vec![0u8; 8192];
        raw[4096] = 0xAA;
        let mut map = base_map(&raw);
        map.acquired_bytes = 4096;
        map.unreadable_bytes = 4096;
        map.status = "incomplete".to_owned();
        map.unreadable = vec![UnreadableEntry {
            start: "0x1000".to_owned(),
            length: 4096,
            ntstatus: "0xC0000001".to_owned(),
        }];

        let error = verify_fixture(&raw, &map).unwrap_err();
        assert!(matches!(
            error,
            VerificationError::UnreadableRepresentationMismatch { .. }
        ));
    }

    #[test]
    fn accepts_zero_backed_unreadable_span_with_exact_accounting() {
        let mut raw = vec![0x55u8; 8192];
        raw[4096..].fill(0);
        let mut map = base_map(&raw);
        map.acquired_bytes = 4096;
        map.unreadable_bytes = 4096;
        map.status = "incomplete".to_owned();
        map.unreadable = vec![UnreadableEntry {
            start: "0x1000".to_owned(),
            length: 4096,
            ntstatus: "0xC0000001".to_owned(),
        }];

        verify_fixture(&raw, &map).unwrap();
    }
}
