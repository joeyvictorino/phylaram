use phylaram_verify::schema::{MapFile, RangeEntry, UnreadableEntry};
use phylaram_verify::verifier::verify_bundle;
use sha2::{Digest, Sha256};
use std::fs::File;
use std::io::Write;
use tempfile::tempdir;

#[test]
fn test_e2e_valid_bundle_verification() {
    let dir = tempdir().unwrap();
    let raw_path = dir.path().join("memory.raw");
    let map_path = dir.path().join("memory.raw.map.json");
    let sha_path = dir.path().join("memory.raw.sha256");

    let mut raw_data = vec![0u8; 1024 * 1024]; // 1 MiB
    raw_data[0x1000..0x2000].fill(0xAA);
    raw_data[0x50000..0x70000].fill(0xBB);

    {
        let mut f = File::create(&raw_path).unwrap();
        f.write_all(&raw_data).unwrap();
    }

    let mut hasher = Sha256::new();
    hasher.update(&raw_data);
    let hash_hex = hex::encode(hasher.finalize());

    let map = MapFile {
        producer: "PhylaRAM".to_string(),
        producer_version: "0.1.0-alpha".to_string(),
        schema: "phylaram-map-2".to_string(),
        status: "complete".to_string(),
        logical_size: 1024 * 1024,
        physical_bytes: 0x1000 + 0x20000,
        acquired_bytes: 0x1000 + 0x20000,
        unreadable_bytes: 0,
        topology_changed: false,
        sha256: hash_hex.clone(),
        kernel_hints: None,
        wavelet_entropy: None,
        compliance_standards: None,
        ranges: vec![
            RangeEntry {
                driver_run: 0,
                start: "0x1000".to_string(),
                length: 0x1000,
            },
            RangeEntry {
                driver_run: 1,
                start: "0x50000".to_string(),
                length: 0x20000,
            },
        ],
        unreadable: vec![],
    };

    std::fs::write(&map_path, serde_json::to_string_pretty(&map).unwrap()).unwrap();
    std::fs::write(&sha_path, format!("{}  memory.raw\n", hash_hex)).unwrap();

    let verified = verify_bundle(&raw_path, &map_path, Some(&sha_path)).unwrap();
    assert_eq!(verified.status, "complete");
    assert_eq!(verified.logical_size, 1024 * 1024);
}

#[test]
fn test_e2e_incomplete_unreadable_verification() {
    let dir = tempdir().unwrap();
    let raw_path = dir.path().join("memory.raw");
    let map_path = dir.path().join("memory.raw.map.json");

    let raw_data = vec![0u8; 1024 * 1024];
    std::fs::write(&raw_path, &raw_data).unwrap();

    let mut hasher = Sha256::new();
    hasher.update(&raw_data);
    let hash_hex = hex::encode(hasher.finalize());

    let map = MapFile {
        producer: "PhylaRAM".to_string(),
        producer_version: "0.1.0-alpha".to_string(),
        schema: "phylaram-map-2".to_string(),
        status: "incomplete".to_string(),
        logical_size: 1024 * 1024,
        physical_bytes: 1024 * 1024,
        acquired_bytes: (1024 * 1024) - 4096,
        unreadable_bytes: 4096,
        topology_changed: false,
        sha256: hash_hex,
        kernel_hints: None,
        wavelet_entropy: None,
        compliance_standards: None,
        ranges: vec![RangeEntry {
            driver_run: 0,
            start: "0x0".to_string(),
            length: 1024 * 1024,
        }],
        unreadable: vec![UnreadableEntry {
            start: "0x4000".to_string(),
            length: 4096,
            ntstatus: "0xC000009C".to_string(),
        }],
    };

    std::fs::write(&map_path, serde_json::to_string_pretty(&map).unwrap()).unwrap();
    let verified = verify_bundle(&raw_path, &map_path, None).unwrap();
    assert_eq!(verified.status, "incomplete");
}
