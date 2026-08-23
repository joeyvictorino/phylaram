use phylaram_verify::verifier;
use std::env;
use std::path::Path;
use std::process;

fn main() {
    let raw_args: Vec<String> = env::args().collect();
    let mut positional = Vec::new();
    let mut verbose = false;

    for arg in raw_args.into_iter().skip(1) {
        if arg == "--verbose" || arg == "-v" || arg == "--map" {
            verbose = true;
        } else if arg == "--help" || arg == "-h" {
            println!("PhylaRAM Offline Verifier 0.1.0-alpha");
            println!("Usage: phylaram-verify <memory.raw> <memory.raw.map.json> [memory.raw.sha256] [--verbose]");
            process::exit(0);
        } else {
            positional.push(arg);
        }
    }

    if positional.len() < 2 {
        eprintln!("PhylaRAM Offline Verifier 0.1.0-alpha");
        eprintln!("Usage: phylaram-verify <memory.raw> <memory.raw.map.json> [memory.raw.sha256] [--verbose]");
        process::exit(1);
    }

    let raw_path = Path::new(&positional[0]);
    let map_path = Path::new(&positional[1]);
    let sha_path = if positional.len() >= 3 {
        Some(Path::new(&positional[2]))
    } else {
        None
    };

    println!("Verifying PhylaRAM acquisition bundle...");
    println!("  RAW Image : {}", raw_path.display());
    println!("  Map JSON  : {}", map_path.display());
    if let Some(s) = sha_path {
        println!("  SHA-256   : {}", s.display());
    }

    match verifier::verify_bundle(raw_path, map_path, sha_path) {
        Ok(map) => {
            println!("\n[VALID] Evidence bundle verified successfully.");
            println!(
                "  Producer         : {} v{}",
                map.producer, map.producer_version
            );
            println!("  Schema           : {}", map.schema);
            println!("  Status           : {}", map.status);
            println!("  Logical Size     : {} bytes ({:.2} GiB)", map.logical_size, map.logical_size as f64 / (1024.0 * 1024.0 * 1024.0));
            println!("  Physical RAM     : {} bytes ({:.2} MiB)", map.physical_bytes, map.physical_bytes as f64 / (1024.0 * 1024.0));
            println!("  Acquired         : {} bytes", map.acquired_bytes);
            println!("  Unreadable       : {} bytes", map.unreadable_bytes);
            println!("  Topology Changed : {}", map.topology_changed);
            println!("  SHA-256          : {}", map.sha256);
            println!("  RAM Runs         : {}", map.ranges.len());
            println!("  Unreadable Spans : {}", map.unreadable.len());

            if let Some(ref hints) = map.kernel_hints {
                println!("\n  Kernel Telemetry / Hints:");
                if let Some(ref dtb) = hints.directory_table_base {
                    println!("    System DTB (CR3) : {}", dtb);
                }
                if let Some(ref kpcr) = hints.kpcr_address {
                    println!("    Executing KPCR   : {}", kpcr);
                }
                if let Some(ref kbase) = hints.kernel_base {
                    println!("    Kernel Base      : {}", kbase);
                }
                if let Some(build) = hints.build_number {
                    println!("    Windows Build    : {}", build);
                }
                println!("    Hypervisor       : {}", if hints.hypervisor_present { "Present" } else { "None" });
            }

            if verbose {
                println!("\n  Physical Memory Topology Table:");
                println!("  -------------------------------------------------------------");
                println!("  Run | Start Physical Addr | Length (Bytes) | Length (MiB)");
                println!("  ----+---------------------+----------------+-------------");
                for r in &map.ranges {
                    let mib = r.length as f64 / (1024.0 * 1024.0);
                    println!("  {:3} | {:>19} | {:>14} | {:>10.2} MiB", r.driver_run, r.start, r.length, mib);
                }
                println!("  -------------------------------------------------------------");

                if !map.unreadable.is_empty() {
                    println!("\n  Unreadable Extents:");
                    for u in &map.unreadable {
                        println!("    - Start: {}, Length: {} bytes, NTSTATUS: {}", u.start, u.length, u.ntstatus);
                    }
                }
            }

            process::exit(0);
        }
        Err(e) => {
            eprintln!("\n[FAILED] Verification error: {}", e);
            process::exit(1);
        }
    }
}
