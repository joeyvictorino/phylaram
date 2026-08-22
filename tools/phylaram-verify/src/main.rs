use phylaram_verify::verifier;
use std::env;
use std::path::Path;
use std::process;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 3 {
        eprintln!("PhylaRAM Offline Verifier 1.0");
        eprintln!("Usage: phylaram-verify <memory.raw> <memory.raw.map.json> [memory.raw.sha256]");
        process::exit(1);
    }

    let raw_path = Path::new(&args[1]);
    let map_path = Path::new(&args[2]);
    let sha_path = if args.len() >= 4 {
        Some(Path::new(&args[3]))
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
            println!("  Logical Size     : {} bytes", map.logical_size);
            println!("  Physical RAM     : {} bytes", map.physical_bytes);
            println!("  Acquired         : {} bytes", map.acquired_bytes);
            println!("  Unreadable       : {} bytes", map.unreadable_bytes);
            println!("  Topology Changed : {}", map.topology_changed);
            println!("  SHA-256          : {}", map.sha256);
            println!("  RAM Runs         : {}", map.ranges.len());
            println!("  Unreadable Spans : {}", map.unreadable.len());
            process::exit(0);
        }
        Err(e) => {
            eprintln!("\n[FAILED] Verification error: {}", e);
            process::exit(1);
        }
    }
}
