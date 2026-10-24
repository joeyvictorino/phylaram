use std::fs;
use std::process::Command;
use tempfile::tempdir;

const RAW_SHA256: &str = "2d711642b726b04401627ca9fbac32f5c8530fb1903cc4db02258717921a4881";

#[test]
fn rejects_unexpected_fourth_positional_argument() {
    let directory = tempdir().unwrap();
    let raw_path = directory.path().join("memory.raw");
    let map_path = directory.path().join("memory.raw.map.json");
    let sha_path = directory.path().join("memory.raw.sha256");

    fs::write(&raw_path, b"x").unwrap();
    fs::write(
        &map_path,
        format!(
            r#"{{
  "producer": "PhylaRAM",
  "producer_version": "0.1.0-alpha",
  "schema": "phylaram-map-2",
  "status": "complete",
  "logical_size": 1,
  "physical_bytes": 1,
  "acquired_bytes": 1,
  "unreadable_bytes": 0,
  "topology_changed": false,
  "sha256": "{RAW_SHA256}",
  "kernel_hints": null,
  "ranges": [{{"driver_run": 0, "start": "0x0", "length": 1}}],
  "unreadable": []
}}"#
        ),
    )
    .unwrap();
    fs::write(&sha_path, format!("{RAW_SHA256}  memory.raw\n")).unwrap();

    let output = Command::new(env!("CARGO_BIN_EXE_phylaram-verify"))
        .arg(&raw_path)
        .arg(&map_path)
        .arg(&sha_path)
        .arg("unexpected-extra-path")
        .output()
        .unwrap();

    assert!(
        !output.status.success(),
        "the verifier silently ignored an extra positional argument:\nstdout:\n{}\nstderr:\n{}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );
    assert!(
        String::from_utf8_lossy(&output.stderr).contains("unexpected positional argument"),
        "missing diagnostic for the rejected argument: {}",
        String::from_utf8_lossy(&output.stderr)
    );
}
